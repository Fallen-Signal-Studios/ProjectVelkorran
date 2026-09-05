# Remaining engineering: changed files

5 September 2026. Exact source/documentation delta from `144544d5d97fa4a3c1720b8420dba822a961bce1` on `codex/tdd-engineering-gaps`. Generated from the reviewed working tree; includes this inventory.

See [audit and roadmap](TDDRemainingEngineering-2026-09-05.md) for reasons, risk and acceptance criteria, and [validation record](TDDRemainingEngineeringValidation.md) for executed and unavailable checks.

Total: **96 files**.

## Game module (49)

- `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_DominionHound.cpp`
- `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_ReformationDrone.cpp`
- `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_SeleneStillpointGrenade.cpp`
- `Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_TarrikEcho.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovEncounterCoordinationComponent.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovEncounterDirector.cpp`
- `Source/ProjectVelkorran/Private/Characters/SovDominionHandler.cpp`
- `Source/ProjectVelkorran/Private/Cinematics/SovCampaignCinematicComponent.cpp`
- `Source/ProjectVelkorran/Private/Cinematics/SovCinematicPolicy.h`
- `Source/ProjectVelkorran/Private/Combat/SovThreatTargeting.h`
- `Source/ProjectVelkorran/Private/Feedback/SovHapticFeedbackComponent.cpp`
- `Source/ProjectVelkorran/Private/Feedback/SovPlatformOutputTypes.cpp`
- `Source/ProjectVelkorran/Private/Framework/SovPlayerController.cpp`
- `Source/ProjectVelkorran/Private/Framework/SovPlayerState.cpp`
- `Source/ProjectVelkorran/Private/Projectiles/SovReformationDroneRocketProjectile.cpp`
- `Source/ProjectVelkorran/Private/Projectiles/SovSeleneCombatProjectile.cpp`
- `Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp`
- `Source/ProjectVelkorran/Private/Settings/SovGameUserSettings.cpp`
- `Source/ProjectVelkorran/Private/Targeting/SovAimAssist.cpp`
- `Source/ProjectVelkorran/Private/Targeting/SovAimAssistPolicy.h`
- `Source/ProjectVelkorran/Private/Tests/SovBallisticAssistRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovNarrativeSerializerRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovNarrativeSerializerTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovPlatformOutputRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovPlatformOutputTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovSaveRuntimeTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovSaveRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovSaveWorldLoadRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovThreatAttackRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovThreatAttackTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovThreatMemoryRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/World/SovWorldTransitActor.cpp`
- `Source/ProjectVelkorran/Public/Campaign/SovEncounterCoordinationComponent.h`
- `Source/ProjectVelkorran/Public/Campaign/SovEncounterDirector.h`
- `Source/ProjectVelkorran/Public/Cinematics/SovCampaignCinematicComponent.h`
- `Source/ProjectVelkorran/Public/Feedback/SovHapticFeedbackComponent.h`
- `Source/ProjectVelkorran/Public/Feedback/SovHapticPolicy.h`
- `Source/ProjectVelkorran/Public/Feedback/SovPlatformOutputTypes.h`
- `Source/ProjectVelkorran/Public/Framework/SovPlayerController.h`
- `Source/ProjectVelkorran/Public/Projectiles/SovCinderStickyGrenadeProjectile.h`
- `Source/ProjectVelkorran/Public/Projectiles/SovReformationDroneRocketProjectile.h`
- `Source/ProjectVelkorran/Public/Projectiles/SovSeleneCombatProjectile.h`
- `Source/ProjectVelkorran/Public/Save/SovSaveSubsystem.h`
- `Source/ProjectVelkorran/Public/Settings/SovGameUserSettings.h`
- `Source/ProjectVelkorran/Public/Targeting/SovAimAssist.h`
- `Source/ProjectVelkorran/Public/World/SovWorldTransitActor.h`

## Existing Narrative modules (21)

- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/NarrativeNPCController.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/NarrativeThreatMemory.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeBotAttackSelection.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tests/NarrativeFrontendRuntimeTests.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tests/NarrativeFrontendTestFixtures.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tests/NarrativeFrontendTestFixtures.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tests/NarrativeMassRepresentationRuntimeTests.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Widgets/NarrativeMenu.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/NarrativeNPCController.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/NarrativeThreatMemory.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/NarrativeThreatPolicy.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Interaction/InteractionComponent.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Widgets/NarrativeMenu.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeCommonUI/Private/NarrativeActivatableWidget.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeCommonUI/Private/Widgets/NarrativeCommonButtonBase.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeCommonUI/Public/NarrativeActivatableWidget.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeCommonUI/Public/Widgets/NarrativeCommonButtonBase.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSave.h`

## Validation tooling (3)

- `Scripts/Check-UnrealReport.py`
- `Scripts/Tests/TestUnrealReport.py`
- `Scripts/Validate-Unreal.ps1`

## Portable regression suites (4)

- `Tests/Portable/NarrativeThreatPolicyTests.cpp`
- `Tests/Portable/SovBallisticAssistPolicyTests.cpp`
- `Tests/Portable/SovCinematicPolicyTests.cpp`
- `Tests/Portable/SovHapticPolicyTests.cpp`

## Documentation and captured evidence (19)

- `Docs/BallisticAssistEngineering.md`
- `Docs/CinematicEngineering.md`
- `Docs/EncounterCoordinationEngineering.md`
- `Docs/FrontendAccessibilityEngineering.md`
- `Docs/NativeThreatAttackIntegration.md`
- `Docs/PartitionedCinematicEngineering.md`
- `Docs/PlatformOutputEngineering.md`
- `Docs/SaveSlotEngineering.md`
- `Docs/SettingsRecoveryAndDiagnostics.md`
- `Docs/TDDNativeCompletion.md`
- `Docs/TDDRemainingEngineering-2026-09-05.md`
- `Docs/TDDRemainingEngineeringEnvironment.txt`
- `Docs/TDDRemainingEngineeringFiles.md`
- `Docs/TDDRemainingEngineeringPortableTests.txt`
- `Docs/TDDRemainingEngineeringReportTests.txt`
- `Docs/TDDRemainingEngineeringSourceChecks.txt`
- `Docs/TDDRemainingEngineeringValidation.md`
- `Docs/ThreatMemoryEngineering.md`
- `Docs/UnrealValidation.md`
