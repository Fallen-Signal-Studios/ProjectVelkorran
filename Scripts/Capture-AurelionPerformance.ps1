<#
.SYNOPSIS
Unattended packaged M12 frame-time and memory capture on the approved PC target.

.DESCRIPTION
Launches the extracted packaged build rendered (never -nullrhi) with the opt-in capture armed. The game
captures SteadySeconds in each world, reopens M12 ReloadCount times, writes one report per world and exits.
Reports are collected with the game log into a run directory. This covers startup, streaming, steady play
and the three-reload memory trend. It cannot drive combat; worst-case combat frames need a separate
played or autoplayed capture.
#>
#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $ArchiveDirectory,
    [string] $OutputRoot = (Join-Path $PSScriptRoot '..\Saved\Validation\Performance'),
    [ValidateRange(0, 10)][int] $ReloadCount = 3,
    [ValidateRange(10, 1800)][int] $SteadySeconds = 60,
    [ValidateRange(8.0, 100.0)][double] $TargetMilliseconds = 16.6667,
    [ValidateRange(60, 7200)][int] $TimeoutSeconds = 1200
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Archive = (Resolve-Path -LiteralPath $ArchiveDirectory).Path
$Executable = Join-Path $Archive 'ProjectVelkorran\Binaries\Win64\ProjectVelkorran.exe'
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw 'Select an extracted Windows archive containing ProjectVelkorran.exe.' }
$Run = Join-Path ([IO.Path]::GetFullPath($OutputRoot)) ('PackagedCapture-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$UserDirectory = Join-Path $Run 'UserData'
New-Item -ItemType Directory -Path $UserDirectory -Force | Out-Null
$Log = Join-Path $Run 'Game.log'
$Target = $TargetMilliseconds.ToString([Globalization.CultureInfo]::InvariantCulture)
$Cvars = "sov.PerfCapture=1,sov.PerfCapture.ExportOnEnd=1,sov.PerfCapture.TargetMs=$Target,sov.PerfCapture.ReloadCount=$ReloadCount,sov.PerfCapture.ReloadAfterSeconds=$SteadySeconds,sov.PerfCapture.QuitAfterReloads=1"
$Arguments = @('/Game/Aurelion/Maps/L_Aurelion_M12', '-windowed', '-ResX=1920', '-ResY=1080', '-nosound',
    ('-UserDir="' + $UserDirectory.Replace('\', '/') + '/"'), ('-abslog="' + $Log + '"'), "-dpcvars=$Cvars")
$Started = Get-Date
$Process = Start-Process -FilePath $Executable -ArgumentList ($Arguments -join ' ') -WorkingDirectory (Split-Path -Parent $Executable) -PassThru
$Exited = $Process.WaitForExit($TimeoutSeconds * 1000)
if (-not $Exited) { Stop-Process -Id $Process.Id -Force }
$Reports = @(Get-ChildItem -Path $Archive, $UserDirectory -Recurse -Filter 'PerfCapture-*.json' -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $Started })
foreach ($Report in $Reports) { Copy-Item -LiteralPath $Report.FullName -Destination $Run }
$Summary = [ordered]@{
    archive = $Archive; executable = $Executable; started = $Started.ToString('o'); exited = $Exited
    exit_code = if ($Exited) { $Process.ExitCode } else { $null }; timed_out = -not $Exited
    reload_count = $ReloadCount; steady_seconds = $SteadySeconds; target_ms = $TargetMilliseconds
    reports = @($Reports | ForEach-Object { $_.Name }); log = $Log
    scope = 'Approved PC target, packaged, rendered; startup, streaming, steady play and reload memory only. Not combat.'
}
$Summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $Run 'capture-summary.json') -Encoding UTF8
Write-Output "Packaged capture finished; exited $Exited; $($Reports.Count) reports; results $Run"
if (-not $Exited -or $Reports.Count -eq 0) { exit 1 }
