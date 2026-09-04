#requires -Version 5.1
<#
.SYNOPSIS
Build Project Velkorran and verify its native campaign automation report with UE 5.7.
.EXAMPLE
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.7'
.EXAMPLE
.\Scripts\Validate-Unreal.ps1 -EngineRoot 'D:\UE_5.7' -BuildOnly
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $EngineRoot,
    [string] $ProjectPath = (Join-Path $PSScriptRoot '..\ProjectVelkorran.uproject'),
    [string] $OutputDirectory,
    [ValidatePattern('^ProjectVelkorran\.Campaign(?:\.[A-Za-z0-9_]+)*$')]
    [string] $TestFilter = 'ProjectVelkorran.Campaign',
    [ValidateRange(30, 86400)]
    [int] $AutomationTimeoutSeconds = 1200,
    [switch] $BuildOnly,
    [switch] $SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:RunDirectory = $null

function Get-JsonProperty {
    param($Object, [string] $Name)
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Quote-WindowsArgument {
    param([string] $Value)
    # Windows process arguments require doubled backslashes before quotes/end quotes.
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}

function Invoke-LoggedProcess {
    param(
        [string] $Executable,
        [string[]] $Arguments,
        [string] $LogName,
        [int] $TimeoutSeconds = 0,
        [switch] $BatchFile
    )
    $argumentLine = ($Arguments | ForEach-Object { Quote-WindowsArgument $_ }) -join ' '
    $processPath = $Executable
    if ($BatchFile) {
        # cmd expands percent expressions even inside quotes. Reject these unusual
        # paths explicitly instead of launching a different path after expansion.
        if (($Executable + $argumentLine) -match '[%\r\n]') {
            throw 'Build.bat paths must not contain percent signs or line breaks.'
        }
        $processPath = $env:ComSpec
        $argumentLine = '/d /v:off /s /c "' + (Quote-WindowsArgument $Executable) + ' ' + $argumentLine + '"'
    }
    $stdoutPath = Join-Path $script:RunDirectory ($LogName + '.stdout.log')
    $stderrPath = Join-Path $script:RunDirectory ($LogName + '.stderr.log')
    Write-Host "Starting $LogName. Logs: $stdoutPath"
    $process = Start-Process -FilePath $processPath -ArgumentList $argumentLine `
        -WorkingDirectory $script:ProjectDirectory -NoNewWindow -PassThru `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    # Hold a process handle so Windows preserves the native exit code after exit.
    $null = $process.Handle
    if ($TimeoutSeconds -gt 0) {
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            $process.WaitForExit()
            Write-Warning "$LogName exceeded $TimeoutSeconds seconds. Inspect the saved logs."
            return 124
        }
    }
    else {
        $process.WaitForExit()
    }
    $process.Refresh()
    return [int] $process.ExitCode
}

function Find-PluginDescriptor {
    param([string] $Name, [string[]] $Roots)
    foreach ($pluginRoot in $Roots) {
        if (-not (Test-Path -LiteralPath $pluginRoot -PathType Container)) { continue }
        $found = Get-ChildItem -LiteralPath $pluginRoot -Filter ($Name + '.uplugin') `
            -Recurse -File -ErrorAction Stop | Select-Object -First 1
        if ($null -ne $found) { return $found.FullName }
    }
    return $null
}

function Assert-ContentAvailable {
    param([string] $Directory, [string] $Label)
    if (Test-Path -LiteralPath $Directory -PathType Container) {
        $asset = Get-ChildItem -LiteralPath $Directory -Recurse -File |
            Where-Object { $_.Extension -in @('.uasset', '.umap') } | Select-Object -First 1
        if ($null -ne $asset) { return }
    }
    throw "$Label assets are unavailable at '$Directory'. Restore the working project's content before automation, or use -BuildOnly for compilation."
}

try {
    if ($env:OS -ne 'Windows_NT') { throw 'This runner requires Windows and a UE 5.7 Win64 installation.' }
    if ($BuildOnly -and $SkipBuild) { throw '-BuildOnly and -SkipBuild cannot be combined.' }
    if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) { throw "Project file not found: $ProjectPath" }
    $ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
    $script:ProjectDirectory = Split-Path -Parent $ProjectPath
    $project = Get-Content -LiteralPath $ProjectPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ([IO.Path]::GetFileNameWithoutExtension($ProjectPath) -ne 'ProjectVelkorran') {
        throw '-ProjectPath must identify ProjectVelkorran.uproject; this runner builds ProjectVelkorranEditor.'
    }
    if ([string](Get-JsonProperty $project 'EngineAssociation') -ne '5.7') {
        throw 'Project EngineAssociation must be 5.7. Resolve the project engine version before validation.'
    }
    if (-not (Test-Path -LiteralPath $EngineRoot -PathType Container)) { throw "Engine root not found: $EngineRoot" }
    $EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
    $engineDirectory = Join-Path $EngineRoot 'Engine'
    $versionPath = Join-Path $engineDirectory 'Build\Build.version'
    if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
        throw "UE Build.version not found: $versionPath. -EngineRoot must be the installation root containing Engine."
    }
    $version = Get-Content -LiteralPath $versionPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 7) { throw 'The supplied engine must be Unreal Engine 5.7.' }
    $buildScript = Join-Path $engineDirectory 'Build\BatchFiles\Build.bat'
    $editorExecutable = Join-Path $engineDirectory 'Binaries\Win64\UnrealEditor-Cmd.exe'
    if (-not $SkipBuild -and -not (Test-Path -LiteralPath $buildScript -PathType Leaf)) { throw "Build.bat not found: $buildScript" }

    $pluginRoots = @((Join-Path $script:ProjectDirectory 'Plugins'), (Join-Path $engineDirectory 'Plugins'))
    foreach ($additional in @(Get-JsonProperty $project 'AdditionalPluginDirectories')) {
        if ([string]::IsNullOrWhiteSpace($additional)) { continue }
        if ([IO.Path]::IsPathRooted($additional)) { $pluginRoots += $additional }
        else { $pluginRoots += Join-Path $script:ProjectDirectory $additional }
    }
    $narrativeDescriptor = Find-PluginDescriptor -Name 'NarrativePro' -Roots $pluginRoots
    if ($null -eq $narrativeDescriptor) { throw 'NarrativePro.uplugin is unavailable in the project or engine plugin directories.' }
    $zenDynEnabled = @($project.Plugins | Where-Object { $_.Name -eq 'ZenDyn' -and $_.Enabled }).Count -gt 0
    if ($zenDynEnabled -and $null -eq (Find-PluginDescriptor -Name 'ZenDyn' -Roots $pluginRoots)) {
        throw 'ZenDyn is enabled in ProjectVelkorran.uproject but ZenDyn.uplugin is unavailable. Install the matching UE 5.7 plugin before compilation or automation.'
    }

    if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
        $OutputDirectory = Join-Path $script:ProjectDirectory 'Saved\Validation'
    }
    elseif (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
        $OutputDirectory = Join-Path $PWD.Path $OutputDirectory
    }
    # Always use a new folder. A stale successful report must never pass a new run.
    $runName = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
    $script:RunDirectory = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) $runName
    $null = New-Item -ItemType Directory -Path $script:RunDirectory -Force
    $summary = [ordered]@{
        project = $ProjectPath; engine = $EngineRoot; testFilter = $TestFilter
        build = 'not run'; automation = 'not run'; logs = $script:RunDirectory
    }

    if (-not $SkipBuild) {
        $buildExit = Invoke-LoggedProcess -Executable $buildScript -BatchFile -LogName 'Build' -Arguments @(
            'ProjectVelkorranEditor', 'Win64', 'Development', "-Project=$ProjectPath", '-WaitMutex'
        )
        $summary.build = "exit $buildExit"
        $summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $script:RunDirectory 'summary.json') -Encoding UTF8
        if ($buildExit -ne 0) { Write-Warning "Unreal build failed with exit code $buildExit."; exit $buildExit }
    }
    if ($BuildOnly) { Write-Host "Build succeeded. Automation was not run. Logs: $script:RunDirectory"; exit 0 }

    if (-not (Test-Path -LiteralPath $editorExecutable -PathType Leaf)) { throw "UnrealEditor-Cmd.exe not found: $editorExecutable" }
    Assert-ContentAvailable -Directory (Join-Path $script:ProjectDirectory 'Content') -Label 'Project'
    Assert-ContentAvailable -Directory (Join-Path (Split-Path -Parent $narrativeDescriptor) 'Content') -Label 'NarrativePro'
    if ($SkipBuild) { Write-Warning 'Build skipped. You are responsible for ensuring editor binaries contain the current source and tests.' }

    $reportDirectory = Join-Path $script:RunDirectory 'AutomationReport'
    $editorLog = Join-Path $script:RunDirectory 'UnrealEditor.log'
    $editorExit = Invoke-LoggedProcess -Executable $editorExecutable -LogName 'Automation' `
        -TimeoutSeconds $AutomationTimeoutSeconds -Arguments @(
            $ProjectPath, '-unattended', '-nop4', '-NullRHI', '-nosplash', '-stdout', '-FullStdOutLogOutput',
            "-ExecCmds=Automation RunTests $TestFilter", '-TestExit=Automation Test Queue Empty',
            "-ReportExportPath=$reportDirectory", "-AbsLog=$editorLog"
        )
    $summary.automation = "process exit $editorExit; report not yet validated"
    $summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $script:RunDirectory 'summary.json') -Encoding UTF8
    if ($editorExit -ne 0) { Write-Warning "Unreal automation process failed with exit code $editorExit."; exit $editorExit }

    $reportPath = Join-Path $reportDirectory 'index.json'
    if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw "Automation did not produce a JSON report: $reportPath. A zero process exit code alone is not a test pass." }
    $report = Get-Content -LiteralPath $reportPath -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($field in @('succeeded', 'succeededWithWarnings', 'failed', 'notRun', 'inProcess', 'tests')) {
        if ($null -eq $report.PSObject.Properties[$field]) { throw "Automation report is missing required field '$field'." }
    }
    $totalSucceeded = [int]$report.succeeded + [int]$report.succeededWithWarnings
    if ($totalSucceeded -le 0 -or [int]$report.failed -ne 0 -or [int]$report.notRun -ne 0 -or [int]$report.inProcess -ne 0) {
        throw "Automation did not complete successfully: passed=$totalSucceeded failed=$($report.failed) notRun=$($report.notRun) inProcess=$($report.inProcess)."
    }
    $selectedTests = @($report.tests | Where-Object {
        $testPath = [string](Get-JsonProperty $_ 'fullTestPath')
        $testPath.Equals($TestFilter, [StringComparison]::OrdinalIgnoreCase) -or
            $testPath.StartsWith($TestFilter + '.', [StringComparison]::OrdinalIgnoreCase)
    })
    if ($selectedTests.Count -eq 0) { throw "The report contains no tests matching '$TestFilter'." }
    foreach ($test in $selectedTests) {
        if ((Get-JsonProperty $test 'state') -ne 'Success' -or [int](Get-JsonProperty $test 'errors') -gt 0) {
            throw "Automation test did not pass: $($test.fullTestPath) (state=$($test.state))."
        }
    }
    $summary.automation = "passed $($selectedTests.Count) matching tests; warnings=$($report.succeededWithWarnings)"
    $summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $script:RunDirectory 'summary.json') -Encoding UTF8
    Write-Host "Validation passed: $($selectedTests.Count) matching automation tests. Report: $reportPath"
    exit 0
}
catch {
    $message = $_.Exception.Message
    if ($null -ne $script:RunDirectory) {
        $message | Set-Content -LiteralPath (Join-Path $script:RunDirectory 'validation-error.txt') -Encoding UTF8
    }
    Write-Error -Message $message -ErrorAction Continue
    exit 2
}
