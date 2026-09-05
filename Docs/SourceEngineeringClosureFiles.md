# Source closure changed files

Baseline: `0378febff90b2cb04ac7887812ba4efe1b87072d`. Branch: `codex/source-engineering-closure`.

142 files added or modified. No project binary assets were authored or deleted. Paths are repository-relative; this inventory includes itself. See [source closure](SourceEngineeringClosure-2026-09-05.md) for systems, rationale, dependencies and risks, and [validation](SourceEngineeringClosureValidation.md) for executed versus pending checks.

## Project source and test fixtures (78)

- `Source/ProjectVelkorran/Private/Accessibility/SovAccessibleNarrationSubsystem.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovCampaignMassProxy.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovEncounterCoordinationComponent.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovEncounterDirector.cpp`
- `Source/ProjectVelkorran/Private/Campaign/SovEncounterDirectorMass.cpp`
- `Source/ProjectVelkorran/Private/Cinematics/SovCampaignCinematicComponent.cpp`
- `Source/ProjectVelkorran/Private/Cinematics/SovCinematicInventoryTransactions.cpp`
- `Source/ProjectVelkorran/Private/Components/SovWeakPointComponent.cpp`
- `Source/ProjectVelkorran/Private/Feedback/SovPlatformOutputTypes.cpp`
- `Source/ProjectVelkorran/Private/Framework/SovPlayerController.cpp`
- `Source/ProjectVelkorran/Private/Narrative/SovNarrativeCue.cpp`
- `Source/ProjectVelkorran/Private/Narrative/SovNarrativeCueComponent.cpp`
- `Source/ProjectVelkorran/Private/Narrative/SovNarrativeValidationLibrary.cpp`
- `Source/ProjectVelkorran/Private/Platform/SovOnlinePlatformServicesAdapter.cpp`
- `Source/ProjectVelkorran/Private/Platform/SovPlatformServicesAdapter.h`
- `Source/ProjectVelkorran/Private/Platform/SovPlatformServicesPolicy.h`
- `Source/ProjectVelkorran/Private/Platform/SovPlatformServicesSubsystem.cpp`
- `Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp`
- `Source/ProjectVelkorran/Private/Settings/SovDisplayCalibration.cpp`
- `Source/ProjectVelkorran/Private/Settings/SovDisplayPolicy.h`
- `Source/ProjectVelkorran/Private/Settings/SovGameUserSettings.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovAccessibilityFrontendRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovAudioSettingsRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovAudioSettingsTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovCampaignMassRoundTripFixtures.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovCampaignMassRoundTripFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovCampaignMassRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovCinematicInventoryRuntimeTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovCinematicInventoryRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovDialogueNarrationRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovDialogueRuntimeTestFixtures.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovDialogueRuntimeTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovDisplayCalibrationAdapterTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovFrontendIntegrationRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovFrontendRuntimeTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovPlatformOutputRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovPlatformOutputTestFixtures.h`
- `Source/ProjectVelkorran/Private/Tests/SovPlatformServicesRuntimeTests.cpp`
- `Source/ProjectVelkorran/Private/Tests/SovPlatformServicesTestFixtures.h`
- `Source/ProjectVelkorran/Private/UI/Dialogue/SovDialogueChoiceWidget.cpp`
- `Source/ProjectVelkorran/Private/UI/Dialogue/SovDialoguePresentationComponent.cpp`
- `Source/ProjectVelkorran/Private/UI/Dialogue/SovDialoguePresentationState.h`
- `Source/ProjectVelkorran/Private/UI/Dialogue/SovDialoguePressurePolicy.h`
- `Source/ProjectVelkorran/Private/UI/SovAccessibilityPresentation.cpp`
- `Source/ProjectVelkorran/Private/UI/SovAccessibilitySettingsMenu.cpp`
- `Source/ProjectVelkorran/Private/UI/SovAccessibleRecordMenu.cpp`
- `Source/ProjectVelkorran/Private/UI/SovFrontendComponent.cpp`
- `Source/ProjectVelkorran/Private/UI/SovNativeGameplayHUD.cpp`
- `Source/ProjectVelkorran/Private/Validation/SovValidateCampaignCommandlet.cpp`
- `Source/ProjectVelkorran/Private/Weapons/SovTransformingWeaponVisual.cpp`
- `Source/ProjectVelkorran/ProjectVelkorran.Build.cs`
- `Source/ProjectVelkorran/Public/Accessibility/SovAccessibleNarrationSubsystem.h`
- `Source/ProjectVelkorran/Public/Campaign/SovCampaignMassPolicy.h`
- `Source/ProjectVelkorran/Public/Campaign/SovCampaignMassProxy.h`
- `Source/ProjectVelkorran/Public/Campaign/SovCampaignMassTypes.h`
- `Source/ProjectVelkorran/Public/Campaign/SovEncounterCoordinationComponent.h`
- `Source/ProjectVelkorran/Public/Campaign/SovEncounterDirector.h`
- `Source/ProjectVelkorran/Public/Campaign/SovEvidenceDefinition.h`
- `Source/ProjectVelkorran/Public/Cinematics/SovCampaignCinematicComponent.h`
- `Source/ProjectVelkorran/Public/Components/SovWeakPointComponent.h`
- `Source/ProjectVelkorran/Public/Feedback/SovPlatformOutputTypes.h`
- `Source/ProjectVelkorran/Public/Framework/SovPlayerController.h`
- `Source/ProjectVelkorran/Public/Narrative/SovNarrativeCue.h`
- `Source/ProjectVelkorran/Public/Narrative/SovNarrativeCueComponent.h`
- `Source/ProjectVelkorran/Public/Platform/SovPlatformServicesSubsystem.h`
- `Source/ProjectVelkorran/Public/Save/SovSaveSubsystem.h`
- `Source/ProjectVelkorran/Public/Settings/SovGameUserSettings.h`
- `Source/ProjectVelkorran/Public/UI/Dialogue/SovDialogueChoiceWidget.h`
- `Source/ProjectVelkorran/Public/UI/Dialogue/SovDialoguePresentationComponent.h`
- `Source/ProjectVelkorran/Public/UI/SovAccessibilityPolicy.h`
- `Source/ProjectVelkorran/Public/UI/SovAccessibilityPresentation.h`
- `Source/ProjectVelkorran/Public/UI/SovAccessibilitySettingsMenu.h`
- `Source/ProjectVelkorran/Public/UI/SovAccessibleRecordMenu.h`
- `Source/ProjectVelkorran/Public/UI/SovFrontendComponent.h`
- `Source/ProjectVelkorran/Public/UI/SovNativeGameplayHUD.h`
- `Source/ProjectVelkorran/Public/Weapons/SovTransformingWeaponVisual.h`

## Existing Narrative plugin (38)

- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/Mass/Peds/MassPedSpawnerSubsystem.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/AI/Mass/Peds/NarrativeMassParticipantBridge.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Character/NarrativeCharacterVisual.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Components/EquipmentComponent.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Items/EquippableItem.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Items/InventoryComponent.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Items/NarrativeCinematicTransaction.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Items/NarrativeItem.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/TalesComponent.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tests/NarrativeMassParticipantBridgeTestFixtures.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tests/NarrativeMassParticipantBridgeTests.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeAnimInstance.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeCharacter.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeGameUserSettings.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Weapons/WeaponVisual.cpp`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/Mass/Peds/MassPedSpawnerSubsystem.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/Mass/Peds/NarrativeMassParticipantBridge.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/AI/Mass/Peds/NarrativePedFragments.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/ArsenalSettings.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Character/NarrativeCharacterVisual.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Components/EquipmentComponent.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Items/EquippableItem.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Items/InventoryComponent.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Items/NarrativeCinematicTransaction.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Items/NarrativeCinematicTransactionPolicy.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Items/NarrativeItem.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Items/WeaponItem.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/Dialogue.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/DialogueSM.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/TalesComponent.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeAnimInstance.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeAudioSettingsPolicy.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeCharacter.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeGameUserSettings.h`
- `Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Weapons/WeaponVisual.h`

## Portable tests and descriptor (8)

- `ProjectVelkorran.uproject`
- `Scripts/Tests/AccessibilityPolicyTests.cpp`
- `Tests/Portable/NarrativeAudioSettingsPolicyTests.cpp`
- `Tests/Portable/SovCampaignMassPolicyTests.cpp`
- `Tests/Portable/SovCinematicInventoryPolicyTests.cpp`
- `Tests/Portable/SovDialoguePressurePolicyTests.cpp`
- `Tests/Portable/SovDisplayPolicyTests.cpp`
- `Tests/Portable/SovPlatformServicesPolicyTests.cpp`

## Documentation (18)

- `Docs/CampaignMassEngineering.md`
- `Docs/CinematicEngineering.md`
- `Docs/CinematicInventoryTransactions.md`
- `Docs/DialogueNarrationEngineering.md`
- `Docs/FrontendAccessibilityEngineering.md`
- `Docs/HearingAudioSettingsEngineering.md`
- `Docs/NarrativeCueEngineering.md`
- `Docs/NativeAccessibilityFrontendClosure.md`
- `Docs/PartitionedCinematicEngineering.md`
- `Docs/PlatformOutputEngineering.md`
- `Docs/PlatformServicesEngineering.md`
- `Docs/SaveSlotEngineering.md`
- `Docs/SourceEngineeringClosure-2026-09-05.md`
- `Docs/SourceEngineeringClosureFiles.md`
- `Docs/SourceEngineeringClosurePortableTests.txt`
- `Docs/SourceEngineeringClosureValidation.md`
- `Docs/TDDRemainingEngineering-2026-09-05.md`
- `Docs/ThreatMemoryEngineering.md`
