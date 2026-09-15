#requires -Version 5.1
<#
.SYNOPSIS
Open the Aurelion Development playtest at its M12 entrance.
.DESCRIPTION
Candidate launcher: place beside the archived Windows/ProjectVelkorran directory.
Each launch keeps its own save and log folder. It does not alter an existing profile.
#>
[CmdletBinding()]
param([string] $ArchiveDirectory = $PSScriptRoot, [switch] $PerfCapture)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ArchiveDirectory = (Resolve-Path -LiteralPath $ArchiveDirectory).Path
$AurelionExecutable = Join-Path $ArchiveDirectory 'ProjectVelkorran\Binaries\Win64\ProjectVelkorran.exe'
if (-not (Test-Path -LiteralPath $AurelionExecutable -PathType Leaf)) {
    throw 'Select the extracted Windows archive containing ProjectVelkorran\Binaries\Win64\ProjectVelkorran.exe.'
}
$AurelionProfileRoot = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'ProjectVelkorran\AurelionPlaytest'
$AurelionRunDirectory = Join-Path $AurelionProfileRoot ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$AurelionUserDirectory = Join-Path $AurelionRunDirectory 'UserData'
$AurelionLogPath = Join-Path $AurelionRunDirectory 'Game.log'
New-Item -ItemType Directory -Path $AurelionUserDirectory -Force | Out-Null

# Quote path values explicitly for UE's command-line parser. Forward slash at
# the end avoids a trailing backslash immediately preceding a closing quote.
$AurelionArguments = @(
    '/Game/Aurelion/Maps/L_Aurelion_M12', '-windowed', '-ResX=1600', '-ResY=900',
    ('-UserDir="' + $AurelionUserDirectory.Replace('\', '/') + '/"'),
    ('-abslog="' + $AurelionLogPath + '"')
)
if ($PerfCapture) {
    # Opt-in local frame-time and memory capture; one report is written when each world ends.
    $AurelionArguments += '-dpcvars=sov.PerfCapture=1,sov.PerfCapture.ExportOnEnd=1,sov.PerfCapture.ExportIntervalSeconds=30'
}
$AurelionStart = New-Object System.Diagnostics.ProcessStartInfo
$AurelionStart.FileName = $AurelionExecutable
$AurelionStart.WorkingDirectory = Split-Path -Parent $AurelionExecutable
$AurelionStart.Arguments = $AurelionArguments -join ' '
$AurelionStart.UseShellExecute = $false
$AurelionStart.CreateNoWindow = $false
$AurelionProcess = [System.Diagnostics.Process]::Start($AurelionStart)
if ($null -eq $AurelionProcess) { throw 'The playtest process did not start.' }
[PSCustomObject]@{
    Executable = $AurelionExecutable
    Map = '/Game/Aurelion/Maps/L_Aurelion_M12'
    ProcessId = $AurelionProcess.Id
    UserDirectory = $AurelionUserDirectory
    Log = $AurelionLogPath
    PerfCapture = [bool]$PerfCapture
    StartedLocal = (Get-Date).ToString('o')
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $AurelionRunDirectory 'launch.json') -Encoding UTF8
Write-Output "Aurelion opened at M12. This run's saves and log: $AurelionRunDirectory"
