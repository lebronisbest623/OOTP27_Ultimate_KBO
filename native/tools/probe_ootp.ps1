$ootpPid = 38288
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KboProbe {
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr OpenProcess(UInt32 access, bool inherit, UInt32 pid);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool ReadProcessMemory(IntPtr h, UInt64 addr, byte[] buf, UIntPtr sz, out UIntPtr rd);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool CloseHandle(IntPtr h);
}
'@

$h = [KboProbe]::OpenProcess(0x0410, $false, [uint32]$ootpPid)
if ($h -eq [IntPtr]::Zero) { Write-Host "OpenProcess failed"; return }

$proc = Get-Process -Id $ootpPid
$base = [uint64]$proc.MainModule.BaseAddress.ToInt64()
$pe = [IO.File]::ReadAllBytes($proc.MainModule.FileName)
$peOff = [BitConverter]::ToInt32($pe, 0x3c)
$secCnt = [BitConverter]::ToUInt16($pe, $peOff + 6)
$optSz = [BitConverter]::ToUInt16($pe, $peOff + 20)
$secBase = $peOff + 24 + $optSz

Write-Host "EXE base: 0x$($base.ToString('X'))"
Write-Host "Total sections: $secCnt"

# List all sections first
for ($si = 0; $si -lt $secCnt; $si++) {
    $so = $secBase + 40 * $si
    $name = [System.Text.Encoding]::ASCII.GetString($pe, $so, 8) -replace "`0.*", ""
    $vSz = [BitConverter]::ToUInt32($pe, $so + 8)
    $vA = [BitConverter]::ToUInt32($pe, $so + 12)
    $chars = [BitConverter]::ToUInt32($pe, $so + 36)
    $hasWrite = ($chars -band 0x80000000) -ne 0
    $hasRead  = ($chars -band 0x40000000) -ne 0
    $hasExec  = ($chars -band 0x20000000) -ne 0
    $eligible = $hasWrite -and $hasRead -and (-not $hasExec)
    Write-Host "  [$si] $name vA=0x$($vA.ToString('X8')) vSz=0x$($vSz.ToString('X8')) W=$hasWrite R=$hasRead E=$hasExec eligible=$eligible"
}

function Read-Bytes([uint64]$Addr, [int]$Sz) {
    $buf = [byte[]]::new($Sz)
    [UIntPtr]$rd = [UIntPtr]::Zero
    if ([KboProbe]::ReadProcessMemory($h, $Addr, $buf, [UIntPtr]::new([uint64]$Sz), [ref]$rd)) {
        if ($rd.ToUInt64() -eq [uint64]$Sz) { return $buf }
    }
    return $null
}

function U64($b, [int]$o) { [BitConverter]::ToUInt64($b, $o) }
function I32($b, [int]$o) { [BitConverter]::ToInt32($b, $o) }
function U32($b, [int]$o) { [BitConverter]::ToUInt32($b, $o) }

# Scan only writable data sections for global DB pointer
$candidates = 0
for ($si = 0; $si -lt $secCnt; $si++) {
    $so = $secBase + 40 * $si
    $chars = [BitConverter]::ToUInt32($pe, $so + 36)
    if (($chars -band 0xC0000000) -ne 0xC0000000 -or ($chars -band 0x20000000) -ne 0) { continue }
    $vA = [BitConverter]::ToUInt32($pe, $so + 12)
    $vSz = [BitConverter]::ToUInt32($pe, $so + 8)
    if ($vSz -lt 8) { continue }
    $name = [System.Text.Encoding]::ASCII.GetString($pe, $so, 8) -replace "`0.*", ""
    Write-Host "Scanning section '$name' vaddr=0x$($vA.ToString('X8')) vsize=0x$($vSz.ToString('X8'))"
    $data = Read-Bytes ($base + [uint64]$vA) ([int]$vSz)
    if ($null -eq $data) { Write-Host "  failed to read"; continue }

    for ($off = 0; $off -le $data.Length - 8; $off += 8) {
        $v = U64 $data $off
        if ($v -lt 0x10000 -or $v -gt 0x7FFFFFFFFFFF) { continue }
        $candidates++

        # Check if it looks like global DB
        $db = Read-Bytes $v 0xb0
        if ($null -eq $db) { continue }
        $teamCount = I32 $db 0x9c
        if ($teamCount -lt 2 -or $teamCount -gt 500) { continue }
        $teamVec = U64 $db 0x90
        if ($teamVec -eq 0) { continue }

        # Validate team pointers
        $ptrs = Read-Bytes $teamVec ($teamCount * 8)
        if ($null -eq $ptrs) { continue }
        $checks = if ($teamCount -lt 5) { $teamCount } else { 5 }
        $validTeams = 0
        for ($i = 0; $i -lt $checks; $i++) {
            $tp = U64 $ptrs ($i * 8)
            if ($tp -eq 0) { continue }
            $team = Read-Bytes $tp 0x4460
            if ($null -eq $team) { continue }
            $tid = U32 $team 0x4450
            $lid = U32 $team 0x120
            if ($tid -gt 0 -and $tid -lt 100000 -and $lid -gt 0 -and $lid -lt 100000) {
                $validTeams++
            }
        }
        if ($validTeams -eq $checks) {
            $rva = $vA + $off
            Write-Host "  FOUND global DB at 0x$($v.ToString('X')) (rva=0x$($rva.ToString('X')))"
            Write-Host "  teams=$teamCount teamVec=0x$($teamVec.ToString('X'))"

            # Now find player vector
            for ($dbOff = 0x28; $dbOff -le 0x200; $dbOff += 8) {
                $slot = Read-Bytes ($v + [uint64]$dbOff) 0x10
                if ($null -eq $slot) { continue }
                $pvec = U64 $slot 0
                $pcnt = I32 $slot 0x0c
                if ($pvec -eq 0 -or $pcnt -le 0 -or $pcnt -gt 300000) { continue }
                $sample = [Math]::Min($pcnt, 100)
                $pptrs = Read-Bytes $pvec ($sample * 8)
                if ($null -eq $pptrs) { continue }
                $hits = 0
                for ($pi = 0; $pi -lt $sample; $pi++) {
                    $pp = U64 $pptrs ($pi * 8)
                    if ($pp -eq 0) { continue }
                    $pb = Read-Bytes ($pp + 0x7c) 0x3c
                    if ($null -eq $pb) { continue }
                    $age = [BitConverter]::ToUInt16($pb, 0)
                    $id = [BitConverter]::ToUInt32($pb, (0xb4 - 0x7c))
                    if ($id -gt 0 -and $id -lt 200000000 -and $age -gt 14 -and $age -lt 66) { $hits++ }
                }
                if ($hits -gt 5) {
                    Write-Host "  Player vector at db+0x$($dbOff.ToString('X')): vec=0x$($pvec.ToString('X')) count=$pcnt hits=$hits"
                    break
                }
            }
            break
        }
    }
}
Write-Host "Pointer candidates tested: $candidates"
[KboProbe]::CloseHandle($h) | Out-Null
