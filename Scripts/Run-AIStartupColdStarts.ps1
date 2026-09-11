#requires -Version 5.1
<#
.SYNOPSIS
Run repeated packaged cold starts with the AI startup trace armed, and extract each timeline.
.DESCRIPTION
The intermittent AI startup stall recorded in WorkPCAIStartupObservation-2026-09-06.md is a
suspected ordering race: an NPC perceives the player before the player's factions are
published, the attack predicate fails on attitude, and UE 5.7 suppresses the same-state
Sight notification so no second callback ever arrives.

Reproducing an intermittent race needs repetition, not one run. Each run gets a fresh
UserDir so no save or config carries between them.

sov.AIStartupTrace arms at OnPreWorldInitialization, so it MUST be set before the world
exists. -ExecCmds runs too late and captures nothing even though the variable reads 1.
-dpcvars applies during engine PreInit and does capture. Do not change this without
re-verifying that events appear.

Read-only: launches the packaged build, waits, terminates, and extracts logs.
.EXAMPLE
.\Scripts\Run-AIStartupColdStarts.ps1 -ArchiveDirectory 'F:\ProjectVelkorran\Saved\SeleneTraceProbe\Windows' -Runs 8
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $ArchiveDirectory,
    [string] $Map = '/Game/Maps/Development/L_SeleneCombat',
    [ValidateRange(1, 100)] [int] $Runs = 8,
    [ValidateRange(10, 300)] [int] $HoldSeconds = 45,
    [string] $OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Executable = Join-Path $ArchiveDirectory 'ProjectVelkorran\Binaries\Win64\ProjectVelkorran.exe'
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Missing packaged executable under $ArchiveDirectory."
}
$Extractor = Join-Path $PSScriptRoot 'Extract-AIStartupTrace.py'
if (-not (Test-Path -LiteralPath $Extractor -PathType Leaf)) { throw 'Missing Extract-AIStartupTrace.py.' }
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) ('Saved\Diagnostics\AIStartup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
Write-Output "Cold-start runs: $Runs   map: $Map   hold: ${HoldSeconds}s"
Write-Output "Output: $OutputDirectory"

$Summary = @()
for ($Index = 1; $Index -le $Runs; $Index++) {
    $RunRoot = Join-Path $OutputDirectory ("run-{0:D2}" -f $Index)
    $UserDirectory = Join-Path $RunRoot 'UserData'
    New-Item -ItemType Directory -Path $UserDirectory -Force | Out-Null
    $LogPath = Join-Path $RunRoot 'Game.log'
    $Arguments = @(
        $Map, '-windowed', '-ResX=1280', '-ResY=720',
        ('-UserDir="' + $UserDirectory.Replace('\', '/') + '/"'),
        ('-abslog="' + $LogPath + '"'),
        '-dpcvars="sov.AIStartupTrace=1"'
    )
    $Process = Start-Process -FilePath $Executable -ArgumentList ($Arguments -join ' ') `
        -PassThru -WorkingDirectory (Split-Path -Parent $Executable)
    Start-Sleep -Seconds $HoldSeconds
    if (-not $Process.HasExited) { Stop-Process -Id $Process.Id -Force; Start-Sleep -Seconds 2 }

    $JsonPath = Join-Path $RunRoot 'trace.json'
    $Events = 0
    if (Test-Path -LiteralPath $LogPath) {
        $Events = (Select-String -Path $LogPath -Pattern 'LogSovAIStartup' -ErrorAction SilentlyContinue |
            Measure-Object).Count
        if ($Events -gt 0) {
            & python $Extractor $LogPath --output $JsonPath 2>&1 | Out-Null
        }
    }
    $Summary += [PSCustomObject]@{
        Run = $Index
        TraceLines = $Events
        Extracted = (Test-Path -LiteralPath $JsonPath)
        Log = $LogPath
        Json = $JsonPath
    }
    Write-Output ("  run {0:D2}: {1} trace lines, extracted={2}" -f $Index, $Events, (Test-Path -LiteralPath $JsonPath))
}

$Summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'runs.json') -Encoding UTF8
Write-Output "Runs recorded in $(Join-Path $OutputDirectory 'runs.json')"
