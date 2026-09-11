# Cargo ships editor binaries without C++ build rules. Keep it disabled in the
# project descriptor for builds; enable its installed connector for importing.
# https://help.kitbash3d.com/en/articles/12689467-exclude-kitbash3d-cargo-plugin-from-unreal-engine-builds
[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference = 'Stop'
$cargoProject = Join-Path (Split-Path $PSScriptRoot -Parent) 'ProjectVelkorran.uproject'
$cargoEditor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$cargoPlugin = Join-Path $EngineRoot 'Engine\Plugins\Marketplace\Cargo\Cargo.uplugin'
foreach ($cargoPath in @($cargoProject, $cargoEditor, $cargoPlugin)) {
    if (-not (Test-Path -LiteralPath $cargoPath)) { throw "Required file missing: $cargoPath" }
}
if (@(Get-Process -Name UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue).Count) {
    throw 'Close the existing Unreal editor before opening the Cargo import session.'
}
if ($cargoProject.Contains('"')) { throw 'Unsupported project path quoting.' }
$cargoProcess = Start-Process -FilePath $cargoEditor -ArgumentList @(('"' + $cargoProject + '"'), '-EnablePlugins=Cargo') -WindowStyle Hidden -PassThru
Write-Output "Cargo-enabled editor started (PID $($cargoProcess.Id)). Project build settings were not changed."
