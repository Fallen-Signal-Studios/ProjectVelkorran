#requires -Version 5.1
<#
.SYNOPSIS
Run the UE 5.7 validation stages and retain a source-bound machine-readable manifest.
.EXAMPLE
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildOnly
.EXAMPLE
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -Mode candidate -ConfigPath 'D:\VelkorranValidation\campaign.json'
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string] $EngineRoot,
    [string] $ProjectPath = (Join-Path $PSScriptRoot '..\ProjectVelkorran.uproject'),
    [string] $OutputDirectory,
    [string] $PythonExecutable,
    [ValidateSet('diagnostic', 'candidate')][string] $Mode = 'diagnostic',
    [string] $ConfigPath,
    [ValidatePattern('^ProjectVelkorran(?:\.[A-Za-z0-9_]+)*$')][string] $TestFilter = 'ProjectVelkorran',
    [ValidateRange(30, 86400)][int] $AutomationTimeoutSeconds = 1200,
    [ValidateRange(30, 86400)][int] $BuildTimeoutSeconds = 7200,
    [ValidateRange(30, 86400)][int] $RouteTimeoutSeconds = 7200,
    [switch] $BuildOnly,
    [switch] $SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
try {
    if ($env:OS -ne 'Windows_NT') { throw 'Engine validation requires Windows and UE 5.7.' }
    if ([string]::IsNullOrWhiteSpace($PythonExecutable)) {
        $bundled = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
        if (Test-Path -LiteralPath $bundled -PathType Leaf) { $PythonExecutable = $bundled }
        else {
            $command = Get-Command python.exe -ErrorAction SilentlyContinue
            if ($null -eq $command) { throw 'Provide -PythonExecutable (Python 3.10 or newer).' }
            $PythonExecutable = $command.Source
        }
    }
    $arguments = @((Join-Path $PSScriptRoot 'Validate-Unreal.py'), '--engine-root', $EngineRoot,
        '--project', $ProjectPath, '--mode', $Mode, '--filter', $TestFilter,
        '--automation-timeout', [string]$AutomationTimeoutSeconds,
        '--build-timeout', [string]$BuildTimeoutSeconds, '--route-timeout', [string]$RouteTimeoutSeconds)
    if ($OutputDirectory) { $arguments += @('--output', $OutputDirectory) }
    if ($ConfigPath) { $arguments += @('--config', $ConfigPath) }
    if ($BuildOnly) { $arguments += '--build-only' }
    if ($SkipBuild) { $arguments += '--skip-build' }
    # Array invocation preserves literal argument boundaries; Python owns process
    # trees, fresh reports, timeouts and candidate-vs-diagnostic qualification.
    & $PythonExecutable @arguments
    exit $LASTEXITCODE
}
catch {
    Write-Error -Message $_.Exception.Message -ErrorAction Continue
    exit 2
}
