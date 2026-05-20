param(
    [int] $MaxSourceLines = 400,
    [int] $MaxShellLines = 250,
    [int] $MaxDirectSourceFilesPerFolder = 4,
    [int] $MaxInternalHeaderLines = 300,
    [Alias("Rule")]
    [string[]] $OnlyRule,
    [string] $BaselinePath = (Join-Path $PSScriptRoot "native-architecture-baseline.json"),
    [string] $WriteBaselinePath,
    [switch] $IncludeDebt,
    [switch] $Strict,
    [switch] $WarnOnly,
    [switch] $Json
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$NativeRoot = Join-Path $RepoRoot "native"
$NativeSrc = Join-Path $NativeRoot "src"
$NativeShell = Join-Path $NativeRoot "KBOFix.c"
$NativeThirdParty = Join-Path $NativeRoot "third_party"

if (-not (Test-Path -LiteralPath $NativeSrc)) {
    throw "Could not find native source directory: $NativeSrc"
}

$NativeRootFull = (Resolve-Path -LiteralPath $NativeRoot).Path.TrimEnd("\")
$NativeSrcFull = (Resolve-Path -LiteralPath $NativeSrc).Path.TrimEnd("\")
$NativeThirdPartyFull = $null
if (Test-Path -LiteralPath $NativeThirdParty) {
    $NativeThirdPartyFull = (Resolve-Path -LiteralPath $NativeThirdParty).Path.TrimEnd("\")
}
$AllowedNativeIncludeRoots = @($NativeSrcFull)
if ($NativeThirdPartyFull) {
    $AllowedNativeIncludeRoots += $NativeThirdPartyFull
}
$Findings = New-Object System.Collections.Generic.List[object]

function Test-PathUnderRoot {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $Root
    )

    return $Path.Equals($Root, [StringComparison]::OrdinalIgnoreCase) `
        -or $Path.StartsWith($Root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

function Get-RelativePathUnderNativeSrc {
    param([Parameter(Mandatory = $true)][string] $Path)

    $Resolved = [IO.Path]::GetFullPath($Path)
    if (-not (Test-PathUnderRoot -Path $Resolved -Root $NativeSrcFull)) {
        return $null
    }

    return $Resolved.Substring($NativeSrcFull.Length + 1).Replace("\", "/")
}

function Get-NativeInternalHeaderOwnerRoot {
    param([Parameter(Mandatory = $true)][string] $Path)

    $RelativePath = Get-RelativePathUnderNativeSrc -Path $Path
    if ($null -eq $RelativePath) {
        return $null
    }

    $FileName = [IO.Path]::GetFileName($Path)
    if ($FileName -notmatch '(^|_)internal\.h$') {
        return $null
    }

    $HeaderStem = $FileName -replace '_internal\.h$', ''
    if ($HeaderStem -ne $FileName) {
        $Directory = (Resolve-Path -LiteralPath (Split-Path -Parent $Path)).Path
        while (Test-PathUnderRoot -Path $Directory -Root $NativeSrcFull) {
            $PublicHeader = Join-Path $Directory "$HeaderStem.h"
            if (Test-Path -LiteralPath $PublicHeader -PathType Leaf) {
                return Get-RelativePathUnderNativeSrc -Path $Directory
            }
            if ($Directory.Equals($NativeSrcFull, [StringComparison]::OrdinalIgnoreCase)) {
                break
            }
            $Directory = Split-Path -Parent $Directory
        }
    }

    if ($RelativePath -match '^(.*)/internal/[^/]+$') {
        return $Matches[1]
    }

    return (Split-Path -Parent $RelativePath).Replace("\", "/")
}

function Test-NativeRelativePathUnderRoot {
    param(
        [Parameter(Mandatory = $true)][string] $RelativePath,
        [Parameter(Mandatory = $true)][string] $Root
    )

    return $RelativePath.Equals($Root, [StringComparison]::OrdinalIgnoreCase) `
        -or $RelativePath.StartsWith($Root + "/", [StringComparison]::OrdinalIgnoreCase)
}

function Get-RelativeNativePath {
    param([Parameter(Mandatory = $true)][string] $Path)

    $Resolved = (Resolve-Path -LiteralPath $Path).Path
    if ($Resolved.StartsWith($NativeSrcFull, [StringComparison]::OrdinalIgnoreCase)) {
        return $Resolved.Substring($NativeSrcFull.Length + 1).Replace("\", "/")
    }

    return $Resolved.Substring($NativeRootFull.Length + 1).Replace("\", "/")
}

function Get-Text {
    param([Parameter(Mandatory = $true)][string] $Path)

    return Get-Content -LiteralPath $Path -Raw
}

function Get-SourceLineCount {
    param([Parameter(Mandatory = $true)][string] $Text)

    if ([string]::IsNullOrEmpty($Text)) {
        return 0
    }

    return ($Text -split "`r?`n").Count
}

function Add-Finding {
    param(
        [Parameter(Mandatory = $true)][string] $Rule,
        [Parameter(Mandatory = $true)][string] $Severity,
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $Message,
        [string] $Suggestion = "",
        [hashtable] $Data = @{}
    )

    $EffectiveSeverity = $Severity
    if ($Strict -and $Severity -ne "info") {
        $EffectiveSeverity = "error"
    }

    $Findings.Add([pscustomobject]@{
        Rule = $Rule
        Severity = $EffectiveSeverity
        OriginalSeverity = $Severity
        Path = $Path
        Message = $Message
        Suggestion = $Suggestion
        Data = $Data
    }) | Out-Null
}

function Test-IncludeOnlyCFile {
    param([Parameter(Mandatory = $true)][string] $Text)

    $WithoutBlockComments = [regex]::Replace($Text, '(?s)/\*.*?\*/', '')
    $MeaningfulLines = @($WithoutBlockComments -split "`r?`n" |
        ForEach-Object { $_ -replace '//.*$', '' } |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" -and $_ -notmatch '^#\s*include\b' })

    return $MeaningfulLines.Count -eq 0
}

function Remove-CCodeNoise {
    param([Parameter(Mandatory = $true)][AllowEmptyString()][string] $Text)

    # Preserve line numbers while hiding comments and literals from heuristic C scans.
    $Chars = $Text.ToCharArray()
    $InString = $false
    $InChar = $false
    $InLineComment = $false
    $InBlockComment = $false
    $Escaped = $false

    for ($Index = 0; $Index -lt $Chars.Length; $Index++) {
        $Ch = $Chars[$Index]
        $Next = [char]0
        if ($Index + 1 -lt $Chars.Length) {
            $Next = $Chars[$Index + 1]
        }

        if ($InLineComment) {
            if ($Ch -eq "`r" -or $Ch -eq "`n") {
                $InLineComment = $false
            }
            else {
                $Chars[$Index] = ' '
            }
            continue
        }

        if ($InBlockComment) {
            if ($Ch -eq '*' -and $Next -eq '/') {
                $Chars[$Index] = ' '
                $Chars[$Index + 1] = ' '
                $Index++
                $InBlockComment = $false
            }
            elseif ($Ch -ne "`r" -and $Ch -ne "`n") {
                $Chars[$Index] = ' '
            }
            continue
        }

        if ($InString) {
            if ($Escaped) {
                $Escaped = $false
            }
            elseif ($Ch -eq '\') {
                $Escaped = $true
            }
            elseif ($Ch -eq '"') {
                $InString = $false
            }

            if ($Ch -ne "`r" -and $Ch -ne "`n") {
                $Chars[$Index] = ' '
            }
            continue
        }

        if ($InChar) {
            if ($Escaped) {
                $Escaped = $false
            }
            elseif ($Ch -eq '\') {
                $Escaped = $true
            }
            elseif ($Ch -eq "'") {
                $InChar = $false
            }

            if ($Ch -ne "`r" -and $Ch -ne "`n") {
                $Chars[$Index] = ' '
            }
            continue
        }

        if ($Ch -eq '/' -and $Next -eq '/') {
            $Chars[$Index] = ' '
            $Chars[$Index + 1] = ' '
            $Index++
            $InLineComment = $true
        }
        elseif ($Ch -eq '/' -and $Next -eq '*') {
            $Chars[$Index] = ' '
            $Chars[$Index + 1] = ' '
            $Index++
            $InBlockComment = $true
        }
        elseif ($Ch -eq '"') {
            $InString = $true
            $Chars[$Index] = ' '
        }
        elseif ($Ch -eq "'") {
            $InChar = $true
            $Chars[$Index] = ' '
        }
    }

    return -join $Chars
}

function Get-CFunctionDefinitions {
    param([Parameter(Mandatory = $true)][string] $Text)

    $Definitions = New-Object System.Collections.Generic.List[object]
    $Lines = @($Text -split "`r?`n")
    $NamePattern = [regex]'(?s)^\s*(?:__declspec\([^)]+\)\s+)?(?:static\s+)?(?:const\s+)?(?:[A-Za-z_][A-Za-z0-9_]*\s+|[A-Za-z_][A-Za-z0-9_]*\s*\*\s*|\*\s*)+([A-Za-z_][A-Za-z0-9_]*)\s*\('
    $ControlNames = @{
        "if" = $true
        "for" = $true
        "while" = $true
        "switch" = $true
        "return" = $true
        "sizeof" = $true
    }

    for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
        $Line = $Lines[$Index]
        if ($Line -match '^\s*#' -or $Line -match '\btypedef\b' -or $Line -notmatch '\(') {
            continue
        }

        $Signature = $Line
        $Lookahead = $Index
        while ($Signature -notmatch '[{;]' -and $Lookahead + 1 -lt $Lines.Count -and $Lookahead -lt $Index + 8) {
            $Lookahead++
            $Signature += "`n" + $Lines[$Lookahead]
        }

        $SignatureText = [string]$Signature
        $OpenIndex = $SignatureText.IndexOf("{")
        $SemicolonIndex = $SignatureText.IndexOf(";")
        if ($OpenIndex -lt 0 -or ($SemicolonIndex -ge 0 -and $SemicolonIndex -lt $OpenIndex)) {
            continue
        }

        $Match = $NamePattern.Match($SignatureText)
        if (-not $Match.Success) {
            continue
        }

        $Name = $Match.Groups[1].Value
        if ($ControlNames.ContainsKey($Name)) {
            continue
        }

        $Definitions.Add([pscustomobject]@{
            Name = $Name
            Line = $Index + 1
        }) | Out-Null
    }

    return $Definitions.ToArray()
}

function Get-TopLevelReturnDeadCode {
    param([Parameter(Mandatory = $true)][string] $Text)

    $Findings = New-Object System.Collections.Generic.List[object]
    $Lines = @($Text -split "`r?`n")
    $Depth = 0
    $ReturnLine = 0
    $AfterTopLevelReturn = $false
    $PreprocDepth = 0
    $PreprocDepthAtReturn = 0
    $SuppressedByPreprocElse = $false

    for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
        $Code = $Lines[$Index]
        $Trimmed = $Code.Trim()

        # Track preprocessor conditional depth so that #else/#elif sibling branches
        # of a return are not mistakenly flagged as dead code.
        if ($Trimmed -match '^#\s*(if|ifdef|ifndef)\b') {
            $PreprocDepth++
        }
        elseif ($Trimmed -match '^#\s*(else|elif)\b') {
            if ($AfterTopLevelReturn -and -not $SuppressedByPreprocElse -and $PreprocDepth -eq $PreprocDepthAtReturn) {
                $SuppressedByPreprocElse = $true
            }
        }
        elseif ($Trimmed -match '^#\s*endif\b') {
            if ($SuppressedByPreprocElse -and $PreprocDepth -eq $PreprocDepthAtReturn) {
                $SuppressedByPreprocElse = $false
                $AfterTopLevelReturn = $false
                $ReturnLine = 0
            }
            if ($PreprocDepth -gt 0) {
                $PreprocDepth--
            }
        }

        if ($AfterTopLevelReturn -and -not $SuppressedByPreprocElse -and $Trimmed -ne "" -and $Trimmed -notmatch '^\s*#') {
            if ($Depth -eq 1 -and $Trimmed -notmatch '^\}') {
                $Findings.Add([pscustomobject]@{
                    ReturnLine = $ReturnLine
                    FirstDeadLine = $Index + 1
                }) | Out-Null
                $AfterTopLevelReturn = $false
            }
            elseif ($Depth -gt 1) {
                $Findings.Add([pscustomobject]@{
                    ReturnLine = $ReturnLine
                    FirstDeadLine = $Index + 1
                }) | Out-Null
                $AfterTopLevelReturn = $false
            }
        }

        if ($Depth -eq 1 -and $Trimmed -match '^return(?:\s+[^;]+)?;\s*$') {
            $AfterTopLevelReturn = $true
            $ReturnLine = $Index + 1
            $PreprocDepthAtReturn = $PreprocDepth
        }

        $OpenCount = ([regex]::Matches($Code, '\{')).Count
        $CloseCount = ([regex]::Matches($Code, '\}')).Count
        $Depth += $OpenCount - $CloseCount
        if ($Depth -le 0) {
            $Depth = 0
            $AfterTopLevelReturn = $false
            $SuppressedByPreprocElse = $false
            $ReturnLine = 0
        }
    }

    return $Findings.ToArray()
}

function Get-BaselineKeys {
    param([string] $Path)

    $Keys = @{}
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $Keys
    }

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Baseline file not found: $Path"
    }

    $Items = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    foreach ($Item in @($Items)) {
        if (-not $Item.rule -or -not $Item.path) {
            throw "Baseline entries must contain rule and path."
        }
        $Keys["$($Item.rule):$($Item.path)"] = $true
    }

    return $Keys
}

$SourceFiles = @(Get-ChildItem -LiteralPath $NativeSrc -Recurse -File |
    Where-Object { $_.Extension -in @(".c", ".h") })

$SourceFacts = @($SourceFiles | ForEach-Object {
    $Text = Get-Text -Path $_.FullName
    [pscustomobject]@{
        FullName = $_.FullName
        Name = $_.Name
        Extension = $_.Extension
        Directory = $_.Directory.FullName
        Path = Get-RelativeNativePath -Path $_.FullName
        Text = $Text
        Lines = Get-SourceLineCount -Text $Text
    }
})

foreach ($Fact in $SourceFacts) {
    if ($Fact.Lines -gt $MaxSourceLines) {
        Add-Finding `
            -Rule "native.source.max-lines" `
            -Severity "error" `
            -Path $Fact.Path `
            -Message "$($Fact.Lines) lines exceeds the $MaxSourceLines-line native source checkpoint." `
            -Suggestion "Split by lifecycle, table, scanner, policy, parser, or install phase." `
            -Data @{ lines = $Fact.Lines; max = $MaxSourceLines }
    }

    if ($Fact.Extension -eq ".h" -and $Fact.Name -match '(^|_)internal\.h$' -and $Fact.Lines -gt $MaxInternalHeaderLines) {
        Add-Finding `
            -Rule "native.header.internal-too-broad" `
            -Severity "warn" `
            -Path $Fact.Path `
            -Message "$($Fact.Name) is $($Fact.Lines) lines; broad private contracts slow down subsystem cleanup." `
            -Suggestion "Move declarations into smaller owned headers when a responsibility boundary is clear." `
            -Data @{ lines = $Fact.Lines; max = $MaxInternalHeaderLines }
    }

    if ($Fact.Name -match '(^|[_\.-])part\d+\.(c|h)$|_part\d+\.(c|h)$') {
        Add-Finding `
            -Rule "native.naming.no-mechanical-parts" `
            -Severity "error" `
            -Path $Fact.Path `
            -Message "Mechanically named source fragments are not a stable architecture boundary." `
            -Suggestion "Rename the file after the responsibility it owns." `
            -Data @{ file = $Fact.Name }
    }

    if ($Fact.Extension -eq ".c" -and (Test-IncludeOnlyCFile -Text $Fact.Text)) {
        Add-Finding `
            -Rule "native.source.no-include-only-c" `
            -Severity "error" `
            -Path $Fact.Path `
            -Message "This .c file only includes headers and does not own behavior." `
            -Suggestion "Remove the shim or move the owned behavior into this translation unit." `
            -Data @{ file = $Fact.Name }
    }

    $IncludeMatches = [regex]::Matches($Fact.Text, '(?m)^\s*#\s*include\s*"([^"]+)"')
    foreach ($IncludeMatch in $IncludeMatches) {
        $IncludeText = $IncludeMatch.Groups[1].Value
        $Line = 1 + (($Fact.Text.Substring(0, $IncludeMatch.Index) -split "`r?`n").Count - 1)
        if ([IO.Path]::IsPathRooted($IncludeText)) {
            Add-Finding `
                -Rule "native.include.no-absolute-local-include" `
                -Severity "error" `
                -Path $Fact.Path `
                -Message "Local include at line $Line uses an absolute path: $IncludeText." `
                -Suggestion "Use a repo-relative relationship from the including file's folder." `
                -Data @{ include = $IncludeText; line = $Line }
            continue
        }

        $IncludeCandidate = [IO.Path]::GetFullPath((Join-Path $Fact.Directory ($IncludeText -replace '/', [IO.Path]::DirectorySeparatorChar)))
        $InsideAllowedIncludeRoot = $false
        foreach ($AllowedRoot in $AllowedNativeIncludeRoots) {
            if (Test-PathUnderRoot -Path $IncludeCandidate -Root $AllowedRoot) {
                $InsideAllowedIncludeRoot = $true
                break
            }
        }

        if (-not $InsideAllowedIncludeRoot) {
            Add-Finding `
                -Rule "native.include.outside-owned-root" `
                -Severity "error" `
                -Path $Fact.Path `
                -Message "Local include at line $Line resolves outside native/src or native/third_party: $IncludeText." `
                -Suggestion "Keep native module includes inside owned native source roots or switch to a documented external include path." `
                -Data @{ include = $IncludeText; line = $Line; resolved = $IncludeCandidate; allowedRoots = $AllowedNativeIncludeRoots }
            continue
        }

        if (-not (Test-Path -LiteralPath $IncludeCandidate -PathType Leaf)) {
            Add-Finding `
                -Rule "native.include.local-path-exists" `
                -Severity "error" `
                -Path $Fact.Path `
                -Message "Local include at line $Line does not resolve to a file: $IncludeText." `
                -Suggestion "Update the relative path after moving files, or include the owner header from the correct folder." `
                -Data @{ include = $IncludeText; line = $Line; resolved = $IncludeCandidate }
            continue
        }

        $InternalHeaderOwnerRoot = Get-NativeInternalHeaderOwnerRoot -Path $IncludeCandidate
        if ($null -ne $InternalHeaderOwnerRoot `
                -and -not (Test-NativeRelativePathUnderRoot -RelativePath $Fact.Path -Root $InternalHeaderOwnerRoot)) {
            Add-Finding `
                -Rule "native.include.no-cross-module-internal-header" `
                -Severity "warn" `
                -Path $Fact.Path `
                -Message "Local include at line $Line reaches private header '$IncludeText' owned by '$InternalHeaderOwnerRoot'." `
                -Suggestion "Include the owner module's public header or move the needed declaration behind a narrow public API." `
                -Data @{ include = $IncludeText; line = $Line; ownerRoot = $InternalHeaderOwnerRoot; resolved = $IncludeCandidate }
        }
    }

    if ($Fact.Extension -eq ".c") {
        $CodeWithoutNoise = Remove-CCodeNoise -Text $Fact.Text

        $Definitions = @(Get-CFunctionDefinitions -Text $CodeWithoutNoise)
        foreach ($DefinitionGroup in @($Definitions | Group-Object Name | Where-Object Count -gt 1)) {
            $Lines = @($DefinitionGroup.Group | ForEach-Object { $_.Line }) -join ", "
            Add-Finding `
                -Rule "native.source.no-duplicate-function-definition" `
                -Severity "error" `
                -Path $Fact.Path `
                -Message "Function '$($DefinitionGroup.Name)' appears to be defined more than once in this translation unit (lines $Lines)." `
                -Suggestion "Remove the duplicated block or split the owned behavior into one responsibility-named file." `
                -Data @{ function = $DefinitionGroup.Name; lines = @($DefinitionGroup.Group | ForEach-Object { $_.Line }) }
        }

        foreach ($DeadCode in @(Get-TopLevelReturnDeadCode -Text $CodeWithoutNoise)) {
            Add-Finding `
                -Rule "native.source.no-dead-code-after-top-level-return" `
                -Severity "error" `
                -Path $Fact.Path `
                -Message "Code continues at line $($DeadCode.FirstDeadLine) after an unconditional function-level return at line $($DeadCode.ReturnLine)." `
                -Suggestion "Remove the unreachable block or move it behind an explicit conditional path." `
                -Data @{ returnLine = $DeadCode.ReturnLine; firstDeadLine = $DeadCode.FirstDeadLine }
        }
    }

    if ($Fact.Path -match '^core/' -and $Fact.Text -match '\b(foreign|military|asian_games|allstar|fa_compensation|fa_market|amateur)\b') {
        Add-Finding `
            -Rule "native.core.no-feature-policy-leak" `
            -Severity "warn" `
            -Path $Fact.Path `
            -Message "Core source mentions feature-domain terms; core should stay shared infrastructure." `
            -Suggestion "Keep KBO feature policy in the owning subsystem unless this is a neutral shared API." `
            -Data @{ pattern = "foreign|military|asian_games|allstar|fa_compensation|fa_market|amateur" }
    }

    if ($Fact.Text -match '\bCreateThread\s*\(' -and $Fact.Text -notmatch 'kbo_runtime_threads_should_continue|kbo_runtime_sleep_should_continue|kbo_register_runtime_thread') {
        Add-Finding `
            -Rule "native.thread.lifecycle-guard" `
            -Severity "warn" `
            -Path $Fact.Path `
            -Message "This file starts a thread but does not reference the runtime stop/sleep helpers." `
            -Suggestion "Make the thread owner observe kbo_runtime_threads_should_continue or kbo_runtime_sleep_should_continue." `
            -Data @{ helper = "kbo_runtime_threads_should_continue" }
    }

    if ($Fact.Text -match '\bkbo_rule_audit_emitf\s*\(' -or $Fact.Text -match '\bkbo_rule_audit_emit\s*\(') {
        Add-Finding `
            -Rule "native.logging.no-raw-rule-audit-api" `
            -Severity "error" `
            -Path $Fact.Path `
            -Message "Rule audit events must use typed KboLogFields builders instead of raw JSON format helpers." `
            -Suggestion "Build KboLogFields with kbo_log_field_str/u32/i32/etc. and call kbo_rule_audit_emit_fields." `
            -Data @{ helper = "kbo_rule_audit_emit_fields" }
    }

    if ($Fact.Text -match '\bappend_logf\s*\(' -or $Fact.Text -match '\bappend_log_line\s*\(') {
        Add-Finding `
            -Rule "native.logging.no-legacy-runtime-log-api" `
            -Severity "error" `
            -Path $Fact.Path `
            -Message "Runtime logs must not use the legacy append_log* API." `
            -Suggestion "Use kbo_log_runtimef or kbo_log_runtime_line so runtime.ndjson entries include source metadata." `
            -Data @{ helper = "kbo_log_runtimef" }
    }

    $UsesRawLogApi = $Fact.Text -match '\bkbo_log_event_emit_raw\s*\(' `
        -or $Fact.Text -match '\bkbo_log_field_raw_json\s*\('
    if ($Fact.Path -ne "core/logging/event/log_event.c" -and $UsesRawLogApi) {
        Add-Finding `
            -Rule "native.logging.no-raw-json-log-api" `
            -Severity "error" `
            -Path $Fact.Path `
            -Message "Structured logging callers must not inject raw JSON fields." `
            -Suggestion "Use KboLogFields builders or kbo_log_fields_merge for already-built field sets." `
            -Data @{ helper = "kbo_log_field_str" }
    }
}

if (Test-Path -LiteralPath $NativeShell) {
    $ShellText = Get-Text -Path $NativeShell
    $ShellLines = Get-SourceLineCount -Text $ShellText
    $ShellRelativePath = Get-RelativeNativePath -Path $NativeShell

    if ($ShellLines -gt $MaxShellLines) {
        Add-Finding `
            -Rule "native.shell.thin-kbofix" `
            -Severity "error" `
            -Path $ShellRelativePath `
            -Message "$ShellLines lines exceeds the $MaxShellLines-line native shell checkpoint." `
            -Suggestion "Keep native/KBOFix.c focused on preprocessor setup, public includes, startup ordering, and patch installation ordering." `
            -Data @{ lines = $ShellLines; max = $MaxShellLines }
    }
}
else {
    Add-Finding `
        -Rule "native.shell.required-file" `
        -Severity "error" `
        -Path "KBOFix.c" `
        -Message "The native shell entry file is missing." `
        -Suggestion "Restore native/KBOFix.c or update the architecture checker if the shell moved." `
        -Data @{ required = $true }
}

$Folders = @(Get-ChildItem -LiteralPath $NativeSrc -Recurse -Directory)
foreach ($Folder in $Folders) {
    $DirectSourceFiles = @(Get-ChildItem -LiteralPath $Folder.FullName -File |
        Where-Object { $_.Extension -in @(".c", ".h") })

    if ($DirectSourceFiles.Count -gt $MaxDirectSourceFilesPerFolder) {
        Add-Finding `
            -Rule "native.folder.max-direct-source-files" `
            -Severity "error" `
            -Path (Get-RelativeNativePath -Path $Folder.FullName) `
            -Message "$($DirectSourceFiles.Count) direct .c/.h files exceeds the folder checkpoint of $MaxDirectSourceFilesPerFolder." `
            -Suggestion "Introduce a responsibility-named child folder." `
            -Data @{ directSourceFiles = $DirectSourceFiles.Count; max = $MaxDirectSourceFilesPerFolder }
    }
}

$NamespaceRootRules = @(
    @{ Path = "custom_events"; Allowed = @(); Message = "custom_events root should not accumulate scanner, dispatcher, or domain behavior." },
    @{ Path = "patch_installers"; Allowed = @(); Message = "patch_installers root should remain a namespace only." },
    @{ Path = "military_service"; Allowed = @("military_service.h"); Message = "military_service root should expose only its public facade." }
)

foreach ($NamespaceRule in $NamespaceRootRules) {
    $Folder = Join-Path $NativeSrc $NamespaceRule.Path
    if (-not (Test-Path -LiteralPath $Folder)) {
        continue
    }

    $DirectSourceFiles = @(Get-ChildItem -LiteralPath $Folder -File |
        Where-Object { $_.Extension -in @(".c", ".h") -and $NamespaceRule.Allowed -notcontains $_.Name })

    foreach ($File in $DirectSourceFiles) {
        Add-Finding `
            -Rule "native.folder.namespace-root-clean" `
            -Severity "error" `
            -Path (Get-RelativeNativePath -Path $File.FullName) `
            -Message $NamespaceRule.Message `
            -Suggestion "Move the file into the child folder that owns the responsibility." `
            -Data @{ folder = $NamespaceRule.Path }
    }
}

$GeneratedFiles = @(
    @{ Path = "build_verify/supported_builds.generated.h"; Suggestion = "Run tools/generate-supported-builds.ps1." },
    @{ Path = "build_verify/supported_builds.generated.c"; Suggestion = "Run tools/generate-supported-builds.ps1." },
    @{ Path = "core/core_flags/keys/runtime_flag_keys.generated.h"; Suggestion = "Run tools/generate-runtime-flags.ps1." },
    @{ Path = "hotkey_window/views/mod/runtime_flags/runtime_flags.generated.h"; Suggestion = "Run tools/generate-runtime-flags.ps1." },
    @{ Path = "hotkey_window/views/mod/runtime_flags/runtime_flags.generated.c"; Suggestion = "Run tools/generate-runtime-flags.ps1." }
)

foreach ($GeneratedFile in $GeneratedFiles) {
    $GeneratedPath = $GeneratedFile.Path
    $FullPath = Join-Path $NativeSrc ($GeneratedPath -replace '/', '\')
    if (-not (Test-Path -LiteralPath $FullPath)) {
        Add-Finding `
            -Rule "native.generated.required-file" `
            -Severity "error" `
            -Path $GeneratedPath `
            -Message "Required generated native source is missing." `
            -Suggestion $GeneratedFile.Suggestion `
            -Data @{ generated = $true }
        continue
    }

    $Text = Get-Text -Path $FullPath
    if ($Text -notmatch '<auto-generated') {
        Add-Finding `
            -Rule "native.generated.auto-generated-marker" `
            -Severity "error" `
            -Path $GeneratedPath `
            -Message "Generated native source is missing the auto-generated marker." `
            -Suggestion $GeneratedFile.Suggestion `
            -Data @{ generated = $true }
    }
}

if ($IncludeDebt) {
    $DebtFiles = @()

    foreach ($DebtFile in $DebtFiles) {
        $FullPath = Join-Path $NativeSrc ($DebtFile -replace '/', '\')
        if (Test-Path -LiteralPath $FullPath) {
            Add-Finding `
                -Rule "native.debt.tracked-broad-contract" `
                -Severity "info" `
                -Path $DebtFile `
                -Message "Tracked architecture debt remains present." `
                -Suggestion "Narrow this contract when touching the owning subsystem." `
                -Data @{ trackedDebt = $true }
        }
    }
}

$BaselineKeys = Get-BaselineKeys -Path $BaselinePath
$FilteredFindings = @($Findings | Where-Object {
    $OnlyRule.Count -eq 0 -or $OnlyRule -contains $_.Rule
})

$ActiveFindings = @($FilteredFindings | Where-Object {
    -not $BaselineKeys.ContainsKey("$($_.Rule):$($_.Path)")
})
$SuppressedFindings = @($FilteredFindings | Where-Object {
    $BaselineKeys.ContainsKey("$($_.Rule):$($_.Path)")
})

if (-not [string]::IsNullOrWhiteSpace($WriteBaselinePath)) {
    $BaselineItems = @($ActiveFindings |
        Sort-Object Rule, Path |
        ForEach-Object {
            [pscustomobject]@{
                rule = $_.Rule
                path = $_.Path
                severity = $_.Severity
                message = $_.Message
            }
        })

    $BaselineJson = $BaselineItems | ConvertTo-Json -Depth 6
    if ($BaselineItems.Count -eq 1) {
        $BaselineJson = "[$BaselineJson]"
    }
    elseif ($BaselineItems.Count -eq 0) {
        $BaselineJson = "[]"
    }

    Set-Content -LiteralPath $WriteBaselinePath -Value $BaselineJson -Encoding UTF8
}

$ErrorCount = @($ActiveFindings | Where-Object Severity -eq "error").Count
$WarnCount = @($ActiveFindings | Where-Object Severity -eq "warn").Count
$InfoCount = @($ActiveFindings | Where-Object Severity -eq "info").Count

$Summary = [pscustomobject]@{
    sourceRoot = "native/src"
    nativeShell = "native/KBOFix.c"
    sourceFiles = $SourceFiles.Count
    activeFindings = $ActiveFindings.Count
    suppressedFindings = $SuppressedFindings.Count
    errors = $ErrorCount
    warnings = $WarnCount
    info = $InfoCount
    strict = [bool]$Strict
    warnOnly = [bool]$WarnOnly
}

if ($Json) {
    [pscustomobject]@{
        summary = $Summary
        findings = $ActiveFindings
        suppressed = $SuppressedFindings
    } | ConvertTo-Json -Depth 8
}
else {
    Write-Host "Native architecture check"
    Write-Host "Source root: $($Summary.sourceRoot)"
    Write-Host "Native shell: $($Summary.nativeShell)"
    Write-Host "Source files: $($Summary.sourceFiles)"
    Write-Host "Findings: $($Summary.activeFindings) active, $($Summary.suppressedFindings) suppressed"
    Write-Host "Errors: $ErrorCount  Warnings: $WarnCount  Info: $InfoCount"
    if (-not [string]::IsNullOrWhiteSpace($WriteBaselinePath)) {
        Write-Host "Baseline written: $WriteBaselinePath"
    }
    Write-Host ""

    foreach ($Group in @($ActiveFindings | Sort-Object Severity, Rule, Path | Group-Object Rule)) {
        $Severity = (@($Group.Group | Select-Object -First 1).Severity).ToUpperInvariant()
        Write-Host "[$Severity] $($Group.Name) ($($Group.Count))"
        foreach ($Finding in @($Group.Group | Sort-Object Path)) {
            Write-Host "  - $($Finding.Path)"
            Write-Host "    $($Finding.Message)"
            if (-not [string]::IsNullOrWhiteSpace($Finding.Suggestion)) {
                Write-Host "    Suggestion: $($Finding.Suggestion)"
            }
        }
        Write-Host ""
    }

    if ($ActiveFindings.Count -eq 0) {
        Write-Host "All native architecture checks passed."
    }
}

if ($ErrorCount -eq 0 -or $WarnOnly) {
    if ($ErrorCount -gt 0 -and $WarnOnly -and -not $Json) {
        Write-Warning "Native architecture checks found $ErrorCount error(s), but -WarnOnly was set."
    }
    exit 0
}

if (-not $Json) {
    Write-Error "Native architecture checks found $ErrorCount error(s)."
}
exit 1
