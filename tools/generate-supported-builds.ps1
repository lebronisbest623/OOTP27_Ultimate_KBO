$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ManifestPath = Join-Path $RepoRoot "config\ootp-supported-builds.json"
$RvaManifestPath = Join-Path $RepoRoot "config\ootp-build-rvas.json"
$CSharpPath = Join-Path $RepoRoot "src\KBOLauncher\Infrastructure\OotpSupportedBuilds.Generated.cs"
$NativeHeaderPath = Join-Path $RepoRoot "native\src\build_verify\supported_builds.generated.h"
$NativeSourcePath = Join-Path $RepoRoot "native\src\build_verify\supported_builds.generated.c"
$NativeRvaHeaderPath = Join-Path $RepoRoot "native\src\build_verify\build_rvas.generated.h"
$NativeRvaSourcePath = Join-Path $RepoRoot "native\src\build_verify\build_rvas.generated.c"
$BootstrapRvaHeaderPath = Join-Path $RepoRoot "native\src\bootstrap\abi\ootp_rvas.generated.h"

function Convert-HexUInt32([string]$Value, [string]$Name) {
    if ($Value -notmatch '^0x[0-9a-fA-F]{1,8}$') {
        throw "$Name must be a uint32 hex string, got '$Value'"
    }
    return [Convert]::ToUInt32($Value.Substring(2), 16)
}

function Convert-CIdentifier([string]$Value) {
    $Identifier = ($Value -replace '[^A-Za-z0-9_]', '_').ToUpperInvariant()
    if ($Identifier -notmatch '^[A-Z_]') {
        $Identifier = "BUILD_$Identifier"
    }
    return $Identifier
}

$Manifest = Get-Content -Raw -Encoding UTF8 $ManifestPath | ConvertFrom-Json
if ($null -eq $Manifest.builds -or $Manifest.builds.Count -eq 0) {
    throw "Supported build manifest must contain at least one build."
}

$SeenIds = @{}
$SeenBuilds = @{}
$Builds = @()
foreach ($Build in $Manifest.builds) {
    if ([string]::IsNullOrWhiteSpace($Build.id)) {
        throw "Supported build id is required."
    }
    if ([string]::IsNullOrWhiteSpace($Build.label)) {
        throw "Supported build label is required."
    }
    if ($SeenIds.ContainsKey($Build.id)) {
        throw "Duplicate supported build id '$($Build.id)'."
    }
    $SeenIds[$Build.id] = $true

    $Timestamp = Convert-HexUInt32 $Build.timestamp "timestamp"
    $SizeOfImage = Convert-HexUInt32 $Build.sizeOfImage "sizeOfImage"
    $BuildKey = "{0:X8}/{1:X8}" -f $Timestamp, $SizeOfImage
    if ($SeenBuilds.ContainsKey($BuildKey)) {
        throw "Duplicate supported build timestamp/size pair '$BuildKey'."
    }
    $SeenBuilds[$BuildKey] = $true

    $Builds += [pscustomobject]@{
        Id = [string]$Build.id
        CId = Convert-CIdentifier ([string]$Build.id)
        Label = [string]$Build.label
        Timestamp = $Timestamp
        SizeOfImage = $SizeOfImage
        ExperimentalSignature = [bool]($Build.PSObject.Properties.Name -contains "experimentalSignature" -and $Build.experimentalSignature)
        NativePatchesSupported = [bool]($Build.PSObject.Properties.Name -contains "nativePatchesSupported" -and $Build.nativePatchesSupported)
    }
}

$BuildsById = @{}
foreach ($Build in $Builds) {
    $BuildsById[$Build.Id] = $Build
}

$NativePatchBuilds = @($Builds | Where-Object { $_.NativePatchesSupported })
$RvaRows = @()
if (Test-Path -LiteralPath $RvaManifestPath) {
    $RvaManifest = Get-Content -Raw -Encoding UTF8 $RvaManifestPath | ConvertFrom-Json
    if ([string]::IsNullOrWhiteSpace($RvaManifest.canonicalBuildId)) {
        throw "RVA manifest canonicalBuildId is required."
    }
    if (-not $BuildsById.ContainsKey([string]$RvaManifest.canonicalBuildId)) {
        throw "RVA manifest canonicalBuildId '$($RvaManifest.canonicalBuildId)' is not listed in supported builds."
    }
    if ($null -eq $RvaManifest.rvas -or $RvaManifest.rvas.Count -eq 0) {
        throw "RVA manifest must contain at least one RVA row."
    }

    $SeenRvaNames = @{}
    foreach ($Rva in $RvaManifest.rvas) {
        $Name = [string]$Rva.name
        if ($Name -notmatch '^OOTP27_[A-Z0-9_]+_RVA$') {
            throw "RVA manifest name must be an OOTP27_*_RVA C macro, got '$Name'."
        }
        if ($SeenRvaNames.ContainsKey($Name)) {
            throw "Duplicate RVA manifest name '$Name'."
        }
        $SeenRvaNames[$Name] = $true

        $CanonicalRva = Convert-HexUInt32 ([string]$Rva.canonicalRva) "$Name canonicalRva"
        if ($null -eq $Rva.builds) {
            throw "RVA manifest row '$Name' must contain build mappings."
        }

        $BuildValues = @{}
        foreach ($Build in $NativePatchBuilds) {
            $BuildProperty = $Rva.builds.PSObject.Properties[$Build.Id]
            if ($null -eq $BuildProperty) {
                throw "RVA manifest row '$Name' is missing build mapping '$($Build.Id)' for native patch support."
            }
            $BuildValues[$Build.Id] = Convert-HexUInt32 ([string]$BuildProperty.Value) "$Name builds.$($Build.Id)"
        }

        $RvaRows += [pscustomobject]@{
            Name = $Name
            CanonicalRva = $CanonicalRva
            BuildValues = $BuildValues
        }
    }
}
elseif ($NativePatchBuilds.Count -gt 0) {
    throw "RVA manifest is required when any build has nativePatchesSupported=true: $RvaManifestPath"
}

$CsLines = @(
    "// <auto-generated />",
    "// Source: config/ootp-supported-builds.json",
    "",
    "internal static class OotpSupportedBuilds",
    "{",
    "    public static readonly OotpSupportedBuild[] All =",
    "    ["
)
foreach ($Build in $Builds) {
    $ExperimentalSignature = if ($Build.ExperimentalSignature) { "true" } else { "false" }
    $NativePatchesSupported = if ($Build.NativePatchesSupported) { "true" } else { "false" }
    $CsLines += '        new(0x{0:X8}u, 0x{1:X8}u, "{2}", {3}, {4}),' -f $Build.Timestamp, $Build.SizeOfImage, ($Build.Label -replace '"', '\"'), $ExperimentalSignature, $NativePatchesSupported
}
$CsLines += @(
    "    ];",
    "}"
)

$NativeHeaderLines = @(
    "/* <auto-generated />",
    "   Source: config/ootp-supported-builds.json */",
    "",
    "#ifndef KBOFIX_BUILD_VERIFY_SUPPORTED_BUILDS_GENERATED_H_",
    "#define KBOFIX_BUILD_VERIFY_SUPPORTED_BUILDS_GENERATED_H_",
    "",
    "#include `"build_verify.h`"",
    "",
    "#define KBO_SUPPORTED_OOTP_BUILD_COUNT $($Builds.Count)"
)
foreach ($Build in $Builds) {
    $NativeHeaderLines += "#define KBO_SUPPORTED_OOTP_BUILD_$($Build.CId)_TIMESTAMP 0x$('{0:X8}' -f $Build.Timestamp)u"
    $NativeHeaderLines += "#define KBO_SUPPORTED_OOTP_BUILD_$($Build.CId)_SIZE_OF_IMAGE 0x$('{0:X8}' -f $Build.SizeOfImage)u"
}
$NativeHeaderLines += @(
    "",
    "extern const OotpSupportedBuild KBO_SUPPORTED_OOTP_BUILDS[KBO_SUPPORTED_OOTP_BUILD_COUNT];",
    "",
    "#endif"
)

$NativeSourceLines = @(
    "/* <auto-generated />",
    "   Source: config/ootp-supported-builds.json */",
    "",
    "#include `"supported_builds.generated.h`"",
    "",
    "const OotpSupportedBuild KBO_SUPPORTED_OOTP_BUILDS[KBO_SUPPORTED_OOTP_BUILD_COUNT] = {"
)
foreach ($Build in $Builds) {
    $NativePatchesSupported = if ($Build.NativePatchesSupported) { "1" } else { "0" }
    $NativeSourceLines += '    {{0x{0:X8}u, 0x{1:X8}u, "{2}", {3}}},' -f $Build.Timestamp, $Build.SizeOfImage, ($Build.Label -replace '"', '\"'), $NativePatchesSupported
}
$NativeSourceLines += @(
    "};"
)

$NativeRvaCount = $RvaRows.Count * $NativePatchBuilds.Count
$NativeRvaHeaderLines = @(
    "/* <auto-generated />",
    "   Source: config/ootp-build-rvas.json */",
    "",
    "#ifndef KBOFIX_BUILD_VERIFY_BUILD_RVAS_GENERATED_H_",
    "#define KBOFIX_BUILD_VERIFY_BUILD_RVAS_GENERATED_H_",
    "",
    "#include `"build_verify.h`"",
    "",
    "#define KBO_BUILD_RVA_COUNT $NativeRvaCount",
    "",
    "extern const OotpBuildRva KBO_BUILD_RVAS[KBO_BUILD_RVA_COUNT];",
    "",
    "#endif"
)

$NativeRvaSourceLines = @(
    "/* <auto-generated />",
    "   Source: config/ootp-build-rvas.json */",
    "",
    "#include `"build_rvas.generated.h`"",
    "",
    "const OotpBuildRva KBO_BUILD_RVAS[KBO_BUILD_RVA_COUNT] = {"
)
foreach ($Build in $NativePatchBuilds) {
    foreach ($Rva in $RvaRows) {
        $NativeRvaSourceLines += '    {{0x{0:X8}u, 0x{1:X8}u, 0x{2:X8}u, 0x{3:X8}u, "{4}"}},' -f `
            $Build.Timestamp, `
            $Build.SizeOfImage, `
            $Rva.CanonicalRva, `
            $Rva.BuildValues[$Build.Id], `
            ($Rva.Name -replace '"', '\"')
    }
}
$NativeRvaSourceLines += @(
    "};"
)

$BootstrapRvaHeaderLines = @(
    "/* <auto-generated />",
    "   Source: config/ootp-build-rvas.json */",
    "",
    "#ifndef KBOFIX_BOOTSTRAP_OOTP_RVAS_GENERATED_H_",
    "#define KBOFIX_BOOTSTRAP_OOTP_RVAS_GENERATED_H_",
    ""
)
foreach ($Rva in $RvaRows) {
    $BootstrapRvaHeaderLines += "#define $($Rva.Name) 0x$('{0:X8}' -f $Rva.CanonicalRva)u"
}
$BootstrapRvaHeaderLines += @(
    "",
    "#endif"
)

Set-Content -Encoding UTF8 -Path $CSharpPath -Value ($CsLines -join "`r`n")
Set-Content -Encoding UTF8 -Path $NativeHeaderPath -Value ($NativeHeaderLines -join "`r`n")
Set-Content -Encoding UTF8 -Path $NativeSourcePath -Value ($NativeSourceLines -join "`r`n")
Set-Content -Encoding UTF8 -Path $NativeRvaHeaderPath -Value ($NativeRvaHeaderLines -join "`r`n")
Set-Content -Encoding UTF8 -Path $NativeRvaSourcePath -Value ($NativeRvaSourceLines -join "`r`n")
Set-Content -Encoding UTF8 -Path $BootstrapRvaHeaderPath -Value ($BootstrapRvaHeaderLines -join "`r`n")

Write-Host "Generated $CSharpPath"
Write-Host "Generated $NativeHeaderPath"
Write-Host "Generated $NativeSourcePath"
Write-Host "Generated $NativeRvaHeaderPath"
Write-Host "Generated $NativeRvaSourcePath"
Write-Host "Generated $BootstrapRvaHeaderPath"
