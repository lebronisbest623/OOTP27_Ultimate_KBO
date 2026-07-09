$ootpPid = 38288
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KboProbe2 {
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr OpenProcess(UInt32 access, bool inherit, UInt32 pid);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool ReadProcessMemory(IntPtr h, UInt64 addr, byte[] buf, UIntPtr sz, out UIntPtr rd);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool CloseHandle(IntPtr h);
}
'@

$h = [KboProbe2]::OpenProcess(0x0410, $false, [uint32]$ootpPid)
if ($h -eq [IntPtr]::Zero) { Write-Host "OpenProcess failed"; return }

$proc = Get-Process -Id $ootpPid
$base = [uint64]$proc.MainModule.BaseAddress.ToInt64()
$pe = [IO.File]::ReadAllBytes($proc.MainModule.FileName)
$peOff = [BitConverter]::ToInt32($pe, 0x3c)
$secCnt = [BitConverter]::ToUInt16($pe, $peOff + 6)
$optSz = [BitConverter]::ToUInt16($pe, $peOff + 20)
$secBase = $peOff + 24 + $optSz

function U64($b, [int]$o) { [BitConverter]::ToUInt64($b, $o) }
function U32($b, [int]$o) { [BitConverter]::ToUInt32($b, $o) }
function I32($b, [int]$o) { [BitConverter]::ToInt32($b, $o) }

# Find .data section
for ($si = 0; $si -lt $secCnt; $si++) {
    $so = $secBase + 40 * $si
    $chars = [BitConverter]::ToUInt32($pe, $so + 36)
    $hasWrite = ($chars -band 0x80000000) -ne 0
    $hasExec  = ($chars -band 0x20000000) -ne 0
    if (-not $hasWrite -or $hasExec) { continue }

    $vA = [BitConverter]::ToUInt32($pe, $so + 12)
    $vSz = [BitConverter]::ToUInt32($pe, $so + 8)
    $name = [System.Text.Encoding]::ASCII.GetString($pe, $so, 8) -replace "`0.*", ""
    Write-Host "Found data section: $name at RVA 0x$($vA.ToString('X8')) size 0x$($vSz.ToString('X8'))"

    # Read in 64KB chunks, find pointer candidates
    $chunkSize = 65536
    $foundGlobal = $false
    for ($chunkOff = 0; $chunkOff -lt $vSz -and -not $foundGlobal; $chunkOff += $chunkSize) {
        $readSize = [Math]::Min($chunkSize, $vSz - $chunkOff)
        $addr = $base + [uint64]$vA + [uint64]$chunkOff
        $buf = [byte[]]::new($readSize)
        [UIntPtr]$rd = [UIntPtr]::Zero
        if (-not [KboProbe2]::ReadProcessMemory($h, $addr, $buf, [UIntPtr]::new([uint64]$readSize), [ref]$rd)) {
            continue
        }
        if ($rd.ToUInt64() -ne [uint64]$readSize) { continue }

        for ($off = 0; $off -le $readSize - 8; $off += 8) {
            $v = U64 $buf $off
            if ($v -lt 0x10000 -or $v -gt 0x7FFFFFFFFFFF) { continue }

            # Test if this pointer points to a valid global database
            $dbBuf = [byte[]]::new(0xb0)
            [UIntPtr]$dbRd = [UIntPtr]::Zero
            if (-not [KboProbe2]::ReadProcessMemory($h, $v, $dbBuf, [UIntPtr]::new(0xb0), [ref]$dbRd)) { continue }
            if ($dbRd.ToUInt64() -ne 0xb0) { continue }

            $teamCount = I32 $dbBuf 0x9c
            if ($teamCount -lt 2 -or $teamCount -gt 500) { continue }
            $teamVec = U64 $dbBuf 0x90
            if ($teamVec -eq 0) { continue }

            # Check first few team pointers
            $n = if ($teamCount -lt 4) { $teamCount } else { 4 }
            $teamPtrBuf = [byte[]]::new($n * 8)
            [UIntPtr]$tpRd = [UIntPtr]::Zero
            if (-not [KboProbe2]::ReadProcessMemory($h, $teamVec, $teamPtrBuf, [UIntPtr]::new([uint64]($n * 8)), [ref]$tpRd)) { continue }

            $valid = 0
            for ($i = 0; $i -lt $n; $i++) {
                $tp = U64 $teamPtrBuf ($i * 8)
                if ($tp -eq 0) { continue }
                $teamBuf = [byte[]]::new(0x4460)
                [UIntPtr]$tRd = [UIntPtr]::Zero
                if (-not [KboProbe2]::ReadProcessMemory($h, $tp, $teamBuf, [UIntPtr]::new(0x4460), [ref]$tRd)) { continue }
                $tid = U32 $teamBuf 0x4450
                $lid = U32 $teamBuf 0x120
                if ($tid -gt 0 -and $tid -lt 100000 -and $lid -gt 0 -and $lid -lt 100000) { $valid++ }
            }

            if ($valid -eq $n) {
                $rva = $vA + $chunkOff + $off
                Write-Host "FOUND global DB at ptr=0x$($v.ToString('X')) (rva=0x$($rva.ToString('X')))"
                Write-Host "  teams=$teamCount teamVector=0x$($teamVec.ToString('X'))"

                # Find player vector near global DB (offsets 0x20..0x500)
                for ($dbOff = 0x20; $dbOff -le 0x500; $dbOff += 8) {
                    $slotBuf = [byte[]]::new(0x10)
                    [UIntPtr]$sRd = [UIntPtr]::Zero
                    if (-not [KboProbe2]::ReadProcessMemory($h, $v + [uint64]$dbOff, $slotBuf, [UIntPtr]::new(0x10), [ref]$sRd)) { continue }
                    $pVec = U64 $slotBuf 0
                    $pCnt = I32 $slotBuf 0x0c
                    if ($pVec -eq 0 -or $pCnt -le 0 -or $pCnt -gt 300000) { continue }

                    $sample = [Math]::Min($pCnt, 50)
                    $sampleBuf = [byte[]]::new($sample * 8)
                    [UIntPtr]$smRd = [UIntPtr]::Zero
                    if (-not [KboProbe2]::ReadProcessMemory($h, $pVec, $sampleBuf, [UIntPtr]::new([uint64]($sample * 8)), [ref]$smRd)) { continue }

                    $hits = 0
                    for ($pi = 0; $pi -lt $sample; $pi++) {
                        $pp = U64 $sampleBuf ($pi * 8)
                        if ($pp -eq 0) { continue }
                        $pb = [byte[]]::new(0x3c)
                        [UIntPtr]$pRd = [UIntPtr]::Zero
                        if (-not [KboProbe2]::ReadProcessMemory($h, $pp + 0x7c, $pb, [UIntPtr]::new(0x3c), [ref]$pRd)) { continue }
                        $age = [BitConverter]::ToUInt16($pb, 0)
                        $id = [BitConverter]::ToUInt32($pb, (0xb4 - 0x7c))
                        if ($id -gt 0 -and $id -lt 200000000 -and $age -gt 14 -and $age -lt 66) { $hits++ }
                    }
                    if ($hits -gt 5) {
                        Write-Host "  Player vector at db+0x$($dbOff.ToString('X3')): vec=0x$($pVec.ToString('X')) count=$pCnt hits=$hits"
                        break
                    }
                }
                $foundGlobal = $true
                break
            }
        }
    }
    if (-not $foundGlobal) { Write-Host "Global DB not found in this section" }
}

[KboProbe2]::CloseHandle($h) | Out-Null
