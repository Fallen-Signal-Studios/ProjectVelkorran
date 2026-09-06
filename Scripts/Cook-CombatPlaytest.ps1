#requires -Version 5.1
<#
.SYNOPSIS
Build and archive the Tarrik and Selene combat maps for Development Win64.
.EXAMPLE
.\Scripts\Cook-CombatPlaytest.ps1 -EngineRoot 'D:\UE_5.7' -ExcludeMetaHumanAuthoringData
.EXAMPLE
.\Scripts\Cook-CombatPlaytest.ps1 -EngineRoot 'D:\UE_5.7' -SkipBuild -ExcludeMetaHumanAuthoringData
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $EngineRoot,
    [string] $ProjectPath = (Join-Path $PSScriptRoot '..\ProjectVelkorran.uproject'),
    [string] $ArchiveDirectory,
    [string[]] $MapPackages = @('/Game/Maps/Development/L_TarrikCombat', '/Game/Maps/Development/L_SeleneCombat'),
    [switch] $SkipBuild,
    [switch] $ExcludeMetaHumanAuthoringData
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$ProjectDirectory = Split-Path -Parent $ProjectPath
if (-not $MapPackages -or $MapPackages.Count -eq 0) { throw 'Supply at least one project map package.' }
foreach ($MapPackage in $MapPackages) {
    if ($MapPackage -notmatch '^/Game/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*$') { throw "Invalid project map package: $MapPackage" }
    $MapFile = Join-Path $ProjectDirectory ('Content\' + $MapPackage.Substring(6).Replace('/', '\') + '.umap')
    if (-not (Test-Path -LiteralPath $MapFile -PathType Leaf)) { throw "Missing authored map: $MapPackage" }
}
$ProjectDescriptor = Get-Content -LiteralPath $ProjectPath -Raw -Encoding UTF8 | ConvertFrom-Json
$EngineVersion = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine\Build\Build.version') -Raw -Encoding UTF8 | ConvertFrom-Json
if ([IO.Path]::GetFileName($ProjectPath) -ne 'ProjectVelkorran.uproject' -or $ProjectDescriptor.EngineAssociation -ne '5.7') {
    throw 'Select ProjectVelkorran.uproject associated with UE 5.7.'
}
if ($EngineVersion.MajorVersion -ne 5 -or $EngineVersion.MinorVersion -ne 7) { throw 'The engine must be UE 5.7.' }
if (-not $ArchiveDirectory) { $ArchiveDirectory = Join-Path $ProjectDirectory 'Saved\WorkPCPlaytest' }
$ArchiveDirectory = [IO.Path]::GetFullPath($ArchiveDirectory)
foreach ($ArgumentPath in @($EngineRoot, $ProjectPath, $ArchiveDirectory)) {
    if ($ArgumentPath -match '[%"\r\n]') { throw 'Batch-file paths cannot contain percent signs, quotes, or line breaks.' }
}

$RunDirectory = Join-Path $ProjectDirectory ('Saved\Validation\PlaytestCook-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $RunDirectory -Force | Out-Null
Write-Output "Playtest cook logs: $RunDirectory"
$BuildScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$AutomationScript = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'

Push-Location -LiteralPath $ProjectDirectory
try {
    if (-not $SkipBuild) {
        foreach ($Target in @('ProjectVelkorranEditor', 'ProjectVelkorran')) {
            & $BuildScript $Target Win64 Development "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE "-Log=$RunDirectory\$Target.log"
            if ($LASTEXITCODE -ne 0) { throw "$Target build failed with exit code $LASTEXITCODE. See $RunDirectory." }
        }
    }
    foreach ($RequiredBinary in @('UnrealEditor-ProjectVelkorran.dll', 'ProjectVelkorran.exe')) {
        if (-not (Test-Path -LiteralPath (Join-Path $ProjectDirectory "Binaries\Win64\$RequiredBinary"))) {
            throw "Missing $RequiredBinary. Build both targets before using -SkipBuild."
        }
    }
    $CookArguments = @(
        'BuildCookRun', "-project=$ProjectPath", '-nop4', '-unattended', '-utf8output', '-installed',
        '-skipbuild', '-nocompile', '-nocompileuat', '-cook', '-stage', '-pak', '-archive',
        '-targetplatform=Win64', '-clientconfig=Development',
        ('-map=' + ($MapPackages -join '+')),
        "-archivedirectory=$ArchiveDirectory"
    )
    # GroomComponent dynamically loads both built-in solvers, while the groom's
    # dependency list normally includes only its selected solver.
    $AdditionalCookerOptions = @('-PACKAGE=/HairStrands/Emitters/StableRodsSystem')
    if ($ExcludeMetaHumanAuthoringData) {
        # These two data directories require plugins restricted to Editor by this project.
        # Leave runtime mesh/material/groom/RigLogic content and dependency traversal intact.
        $AdditionalCookerOptions += '-NeverCookDir=/MetaHumanCharacter/BuildPipeline+/MetaHumanCoreTech/RealtimeMono'
    }
    $CookArguments += '-AdditionalCookerOptions=' + ($AdditionalCookerOptions -join ' ')
    $CookArguments | Set-Content -LiteralPath (Join-Path $RunDirectory 'arguments.txt') -Encoding UTF8
    & $AutomationScript @CookArguments 2>&1 | Tee-Object -FilePath (Join-Path $RunDirectory 'BuildCookRun.log')
    $CookExitCode = $LASTEXITCODE
    $CookExitCode | Set-Content -LiteralPath (Join-Path $RunDirectory 'exit-code.txt') -Encoding UTF8
    if ($CookExitCode -ne 0) { throw "Cook/archive failed with exit code $CookExitCode. See $RunDirectory." }
    Write-Output "Playtest archive: $ArchiveDirectory"
}
finally { Pop-Location }
