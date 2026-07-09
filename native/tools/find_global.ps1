# Quick global DB finder - tries multiple known patterns
$ootpPid = 38288
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KboFind {
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern IntPtr OpenProcess(UInt32 a, bool b, UInt32 c);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool ReadProcessMemory(IntPtr h, UInt64 addr, byte[] buf, UIntPtr sz, out UIntPtr rd);
  [DllImport("kernel32.dll", SetLastError=true)]
  public static extern bool CloseHandle(IntPtr h);
}
'@

$h = [KboFind]::OpenProcess(0x0410, $false, [uint32]$ootpPid)
if ($h -eq [IntPtr]::Zero) { Write-Host "OpenProcess failed"; return }

$proc = Get-Process -Id $ootpPid
$base = [uint64]$proc.MainModule.BaseAddress.ToInt64()

function U64($b, [int]$o) { [BitConverter]::ToUInt64($b, $o) }
function U32($b, [int]$o) { [BitConverter]::ToUInt32($b, $o) }
function I32($b, [int]$o) { [BitConverter]::ToInt32($b, $o) }

function ReadMem([uint64]$addr, [int]$sz) {
    $buf = [byte[]]::new($sz)
    [UIntPtr]$rd = [UIntPtr]::Zero
    if ([KboFind]::ReadProcessMemory($h, $addr, $buf, [UIntPtr]::new([uint64]$sz), [ref]$rd)) {
        if ($rd.ToUInt64() -eq [uint64]$sz) { return $buf }
    }
    return $null
}

# Strategy 1: Look for a pointer in .data that points to a large heap object with many team-like pointers
# Strategy 2: Scan heap for the team vector pattern directly

Write-Host "Strategy: looking for team-count/team-vector pattern in heap..."

# Read .data section
$pe = [IO.File]::ReadAllBytes($proc.MainModule.FileName)
$peOff = [BitConverter]::ToInt32($pe, 0x3c)
$secCnt = [BitConverter]::ToUInt16($pe, $peOff + 6)
$optSz = [BitConverter]::ToUInt16($pe, $peOff + 20)
$secBase = $peOff + 24 + $optSz

# Find .data section
for ($si = 0; $si -lt $secCnt; $si++) {
    $so = $secBase + 40 * $si
    $chars = [BitConverter]::ToUInt32($pe, $so + 36)
    if (($chars -band 0xC0000000) -ne 0xC0000000 -or ($chars -band 0x20000000) -ne 0) { continue }
    $vA = [BitConverter]::ToUInt32($pe, $so + 12)
    $vSz = [BitConverter]::ToUInt32($pe, $so + 8)

    $firstChunk = 1MB
    $buf = ReadMem ($base + [uint64]$vA) $firstChunk
    if ($null -eq $buf) { Write-Host "Cannot read .data"; continue }
    Write-Host "Scanning first 1MB of .data..."

    $found = 0
    for ($off = 0; $off -le $firstChunk - 8; $off += 8) {
        $v = U64 $buf $off
        if ($v -lt 0x10000 -or $v -gt 0x7FFFFFFFFFFF) { continue }

        # Quick check: read 0xb0 bytes at candidate
        $db = ReadMem $v 0xb0
        if ($null -eq $db) { continue }

        # Try team count at different offsets (0x9c is expected, also try 0xa0, 0x98)
        foreach ($tcOff in @(0x9c, 0xa0, 0x98, 0xa4, 0xa8)) {
            $tc = I32 $db $tcOff
            if ($tc -lt 2 -or $tc -gt 500) { continue }

            # Try team vector at nearby offsets (0x90 is expected, also try 0x88, 0x98)
            foreach ($tvOff in @(0x90, 0x88, 0x98, 0x80, 0xa0)) {
                $tv = U64 $db $tvOff
                if ($tv -eq 0) { continue }

                # Try to read team pointers
                $n = if ($tc -lt 3) { $tc } else { 3 }
                $tp = ReadMem $tv ($n * 8)
                if ($null -eq $tp) { continue }

                # Try team ID at different offsets (0x4450 expected, also try nearby)
                $validTeams = 0
                foreach ($tidOff in @(0x4450, 0x4448, 0x4458, 0x4440)) {
                    foreach ($lidOff in @(0x120, 0x118, 0x128, 0x110)) {
                        $valid = 0
                        for ($i = 0; $i -lt $n; $i++) {
                            $teamPtr = U64 $tp ($i * 8)
                            if ($teamPtr -eq 0) { continue }
                            $team = ReadMem $teamPtr 0x4460
                            if ($null -eq $team) { continue }
                            $tid = U32 $team $tidOff
                            $lid = U32 $team $lidOff
                            if ($tid -gt 0 -and $tid -lt 100000 -and $lid -gt 0 -and $lid -lt 100000) {
                                $valid++
                            }
                        }
                        if ($valid -ge $n) {
                            $validTeams = $valid
                            $foundTeamIdOff = $tidOff
                            $foundLeagueIdOff = $lidOff
                            break
                        }
                    }
                    if ($validTeams -ge $n) { break }
                }

                if ($validTeams -ge $n) {
                    $rva = $vA + $off
                    Write-Host "FOUND! ptr=0x$($v.ToString('X')) rva=0x$($rva.ToString('X'))"
                    Write-Host "  teamCount@0x$($tcOff.ToString('X'))=$tc teamVec@0x$($tvOff.ToString('X'))=0x$($tv.ToString('X'))"
                    Write-Host "  teamId@0x$($foundTeamIdOff.ToString('X')) leagueId@0x$($foundLeagueIdOff.ToString('X'))"
                    $found++
                }
            }
        }
    }
    Write-Host "Found $found candidate(s)"
}

[KboFind]::CloseHandle($h) | Out-Null
