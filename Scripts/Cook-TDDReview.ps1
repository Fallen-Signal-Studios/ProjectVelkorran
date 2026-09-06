#requires -Version 5.1
<#
.SYNOPSIS
Build and package the two technical TDD review segments. This does not qualify the full campaign.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $EngineRoot,
    [string] $ProjectPath = (Join-Path $PSScriptRoot '..\ProjectVelkorran.uproject'),
    [string] $ArchiveDirectory,
    [switch] $SkipBuild,
    [switch] $ExcludeMetaHumanAuthoringData
)
$ErrorActionPreference = 'Stop'
if (-not $ArchiveDirectory) {
    $ArchiveDirectory = Join-Path (Split-Path -Parent (Resolve-Path -LiteralPath $ProjectPath).Path) 'Saved\TDDReviewPlaytest'
}
& (Join-Path $PSScriptRoot 'Cook-CombatPlaytest.ps1') -EngineRoot $EngineRoot -ProjectPath $ProjectPath `
    -ArchiveDirectory $ArchiveDirectory -SkipBuild:$SkipBuild -ExcludeMetaHumanAuthoringData:$ExcludeMetaHumanAuthoringData `
    -MapPackages @('/Game/Maps/Development/TDDReview/L_TarrikReview', '/Game/Maps/Development/TDDReview/L_SeleneReview')
