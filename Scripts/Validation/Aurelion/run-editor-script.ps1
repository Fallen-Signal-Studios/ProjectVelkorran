#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ScriptPath,
    [Parameter(Mandatory = $true)][string]$EngineRoot,
    [string]$ProjectPath = (Join-Path $PSScriptRoot '..\..\..\ProjectVelkorran.uproject'),
    [string]$OutputRoot,
    [string]$PipInstallPath,
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Label = 'AurelionQA',
    [ValidateRange(30, 7200)][int]$TimeoutSeconds = 600,
    [switch]$Visible,
    [switch]$KeepEntryOpen,
    [switch]$ContinueE1,
    [switch]$ContinueRoute,
    [switch]$DisableAura,
    [switch]$UseFileSystemCache,
    [ValidateSet('/Game/Aurelion/Maps/L_Aurelion_M12', '/Game/Aurelion/Maps/L_Aurelion_M13', '/Game/Aurelion/ArtReview/L_Aurelion_ArchitectureKit')]
    [string]$Map = '/Game/Aurelion/Maps/L_Aurelion_M12'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($KeepEntryOpen -and -not $Visible) { throw 'Retained entry sessions must be visible.' }
if ($ContinueE1 -and (-not $Visible -or -not $KeepEntryOpen)) { throw 'E1 continuation requires a visible retained session.' }
if ($ContinueRoute -and -not $ContinueE1) { throw 'Route continuation requires the actual E1 driver.' }
if (($KeepEntryOpen -or $ContinueE1 -or $ContinueRoute) -and $Map -ne '/Game/Aurelion/Maps/L_Aurelion_M12') {
    throw 'A fresh route must start in M12; M13 is a read-only inspection target here.'
}
$AurelionProject = (Resolve-Path -LiteralPath $ProjectPath).Path
$AurelionProjectDirectory = Split-Path -Parent $AurelionProject
$AurelionEngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$AurelionEditor = Join-Path $AurelionEngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$AurelionScript = (Resolve-Path -LiteralPath $ScriptPath).Path
if (-not (Test-Path -LiteralPath $AurelionEditor -PathType Leaf)) { throw 'UnrealEditor.exe is missing.' }
if ([IO.Path]::GetExtension($AurelionScript) -ne '.py') { throw 'Select an Unreal Python script.' }
$AurelionVersion = Get-Content -LiteralPath (Join-Path $AurelionEngineRoot 'Engine\Build\Build.version') -Raw | ConvertFrom-Json
if ($AurelionVersion.MajorVersion -ne 5 -or $AurelionVersion.MinorVersion -ne 7) { throw 'UE 5.7 is required.' }
$AurelionDescriptor = Get-Content -LiteralPath $AurelionProject -Raw | ConvertFrom-Json
if ([IO.Path]::GetFileName($AurelionProject) -ne 'ProjectVelkorran.uproject' -or $AurelionDescriptor.EngineAssociation -ne '5.7') {
    throw 'Select ProjectVelkorran associated with UE 5.7.'
}
if (-not $OutputRoot) { $OutputRoot = Join-Path $AurelionProjectDirectory 'Saved\Validation\Aurelion' }
$AurelionOutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$AurelionRun = Join-Path $AurelionOutputRoot ($Label + '-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
if ($PipInstallPath) {
    $PipInstallPath = (Resolve-Path -LiteralPath $PipInstallPath).Path
    if (-not (Test-Path -LiteralPath (Join-Path $PipInstallPath 'pyvenv.cfg') -PathType Leaf)) { throw 'Explicit pip cache is not an existing virtual environment.' }
}
New-Item -ItemType Directory -Path $AurelionRun | Out-Null
$AurelionExecution = '-ExecutePythonScript=' + $AurelionScript.Replace('\', '/')
if ($KeepEntryOpen) {
    # ExecutePythonScript owns a persistent modal while keep_python_script_alive
    # is true. A retained route needs normal UI input (notably Axiom's hand click).
    # UE's deferred PY command accepts an unquoted pathname through its .py suffix.
    if ($AurelionScript.Contains(',')) { throw 'Retained ExecCmds paths cannot contain a comma.' }
    $AurelionExecution = '-ExecCmds=py ' + $AurelionScript.Replace('\', '/')
}
$AurelionArgs = @($AurelionProject, $Map, '-unattended', '-nosound', '-nosplash', '-nop4', '-NoEpicPortal',
    $AurelionExecution,
    ('-UserDir=' + (Join-Path $AurelionRun 'UserData').Replace('\', '/') + '/'),
    ('-AbsLog=' + (Join-Path $AurelionRun 'Editor.log').Replace('\', '/')),
    '-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bRunPipInstallOnStartup=false')
if (-not $Visible) { $AurelionArgs += '-RenderOffscreen' }
if ($DisableAura) { $AurelionArgs += '-DisablePlugins=Aura' }
if ($UseFileSystemCache) { $AurelionArgs += '-ddc=InstalledNoZenLocalFallback' }
foreach ($AurelionArg in $AurelionArgs) {
    if ($AurelionArg -match '["\r\n]' -or $AurelionArg.EndsWith('\')) { throw 'Unsupported argument quoting.' }
}
$AurelionArgLine = ($AurelionArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
$AurelionEnvironment = @{
    SOV_AURELION_RUN_DIRECTORY = $AurelionRun
    SOV_AURELION_ENTRY_KEEP_OPEN = $(if ($KeepEntryOpen) { '1' } else { '0' })
    SOV_AURELION_ENTRY_CONTINUE_E1 = $(if ($ContinueE1) { '1' } else { '0' })
    SOV_AURELION_E1_CONTINUE_ROUTE = $(if ($ContinueRoute) { '1' } else { '0' })
    UE_PIPINSTALL_PATH = $(if ($PipInstallPath) { $PipInstallPath } else { $null })
}
if ($UseFileSystemCache) {
    $AurelionEnvironment['UE-LocalDataCachePath'] = (Join-Path $AurelionProjectDirectory 'DerivedDataCache\ValidationFallback').Replace('\', '/')
}
$AurelionPreviousEnvironment = @{}
try {
    foreach ($AurelionName in $AurelionEnvironment.Keys) {
        $AurelionPreviousEnvironment[$AurelionName] = [Environment]::GetEnvironmentVariable($AurelionName, 'Process')
        [Environment]::SetEnvironmentVariable($AurelionName, $AurelionEnvironment[$AurelionName], 'Process')
    }
    # -Visible controls offscreen rendering only. Keep helper launch behavior
    # hidden; the operator can bring the retained editor to the foreground.
    $AurelionProcess = Start-Process -FilePath $AurelionEditor -ArgumentList $AurelionArgLine -WorkingDirectory $AurelionProjectDirectory -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $AurelionRun 'stdout.log') -RedirectStandardError (Join-Path $AurelionRun 'stderr.log')
    $null = $AurelionProcess.Handle
}
finally {
    foreach ($AurelionName in $AurelionPreviousEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable($AurelionName, $AurelionPreviousEnvironment[$AurelionName], 'Process')
    }
}
$AurelionStart = $AurelionProcess.StartTime
$AurelionLaunch = [ordered]@{
    Status = 'started'; ProcessId = $AurelionProcess.Id; ProcessStartTime = $AurelionStart.ToUniversalTime().ToString('o')
    Script = $AurelionScript; ScriptSHA256 = (Get-FileHash -LiteralPath $AurelionScript -Algorithm SHA256).Hash.ToLowerInvariant()
    Project = $AurelionProject; Map = $Map; RunDirectory = $AurelionRun
    Visible = [bool]$Visible; Retained = [bool]$KeepEntryOpen; ProcessExitIsGameplayPass = $false
    Execution = $(if ($KeepEntryOpen) { 'deferred_console_python' } else { 'python_process_executor' })
    ExcludedEditorPlugins = @($(if ($DisableAura) { 'Aura' }))
}
$AurelionLaunch | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $AurelionRun 'launch.json') -Encoding UTF8
Write-Output "Unreal $Label started; PID $($AurelionProcess.Id); results $AurelionRun"
# The route drivers own their bounded stage deadlines. A retained editor is left
# for the operator on success/failure; launching is never reported as a QA pass.
if ($KeepEntryOpen) { return }
$AurelionTimedOut = -not $AurelionProcess.WaitForExit($TimeoutSeconds * 1000)
if ($AurelionTimedOut) {
    $AurelionActual = Get-Process -Id $AurelionProcess.Id -ErrorAction SilentlyContinue
    if ($AurelionActual -and $AurelionActual.Path -eq $AurelionEditor -and $AurelionActual.StartTime -eq $AurelionStart) {
        Stop-Process -Id $AurelionProcess.Id -Force
        $AurelionProcess.WaitForExit()
    }
}
$AurelionProcess.Refresh()
$AurelionResult = [ordered]@{ ProcessId = $AurelionProcess.Id; TimedOut = $AurelionTimedOut; ExitCode = $null; ProcessExitIsGameplayPass = $false }
if ($AurelionProcess.HasExited) { $AurelionResult.ExitCode = $AurelionProcess.ExitCode }
$AurelionResult | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $AurelionRun 'process-result.json') -Encoding UTF8
Write-Output "Unreal $Label finished; exit $($AurelionResult.ExitCode); timed out $AurelionTimedOut; inspect JSON qualifications in $AurelionRun"
if ($AurelionTimedOut -or $AurelionResult.ExitCode -ne 0) { exit 1 }
