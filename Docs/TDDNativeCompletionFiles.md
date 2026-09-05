# Native completion changed-file inventory

Delta from `fe31c66e69226d73d92a3dea38ed6a6fef3aa1dd` on `codex/campaign-engineering`. This inventory includes source, tests, configuration, documentation and execution evidence. It excludes unchanged files and unavailable editor assets.

**280 files: 181 added, 99 modified.** No tracked file is deleted.

| Group | Files |
|---|---:|
| Configuration and build tooling | 3 |
| Documentation and execution evidence | 23 |
| Narrative Save System integration | 8 |
| Narrative runtime integration | 42 |
| Portable production-policy tests | 18 |
| Project native gameplay and campaign code | 149 |
| Unreal automation sources and fixtures | 37 |

## Configuration and build tooling

| Change | Exact repository path |
|---|---|
| Modified | [Config/DefaultEngine.ini](../Config/DefaultEngine.ini) |
| Modified | [Scripts/Validate-Unreal.ps1](../Scripts/Validate-Unreal.ps1) |
| Modified | [Source/ProjectVelkorran/ProjectVelkorran.Build.cs](../Source/ProjectVelkorran/ProjectVelkorran.Build.cs) |

## Documentation and execution evidence

| Change | Exact repository path |
|---|---|
| Modified | [Docs/CampaignEngineeringDelivery.md](../Docs/CampaignEngineeringDelivery.md) |
| Modified | [Docs/CampaignStateIntegration.md](../Docs/CampaignStateIntegration.md) |
| Added | [Docs/CinematicEngineering.md](../Docs/CinematicEngineering.md) |
| Added | [Docs/ConvergenceEngineering.md](../Docs/ConvergenceEngineering.md) |
| Modified | [Docs/CorruptionEngineering.md](../Docs/CorruptionEngineering.md) |
| Added | [Docs/EncounterCoordinationEngineering.md](../Docs/EncounterCoordinationEngineering.md) |
| Added | [Docs/ExertionEngineering.md](../Docs/ExertionEngineering.md) |
| Added | [Docs/FieldRecoveryEngineering.md](../Docs/FieldRecoveryEngineering.md) |
| Added | [Docs/MeleeEngineering.md](../Docs/MeleeEngineering.md) |
| Added | [Docs/NarrativeCueEngineering.md](../Docs/NarrativeCueEngineering.md) |
| Added | [Docs/NarrativeStateEngineering.md](../Docs/NarrativeStateEngineering.md) |
| Added | [Docs/RemainingCombatEngineering.md](../Docs/RemainingCombatEngineering.md) |
| Added | [Docs/SaveSlotEngineering.md](../Docs/SaveSlotEngineering.md) |
| Added | [Docs/SettingsRecoveryAndDiagnostics.md](../Docs/SettingsRecoveryAndDiagnostics.md) |
| Added | [Docs/TDDNativeCompletion.md](../Docs/TDDNativeCompletion.md) |
| Added | [Docs/TDDNativeCompletionEnvironment.txt](../Docs/TDDNativeCompletionEnvironment.txt) |
| Added | [Docs/TDDNativeCompletionFiles.md](../Docs/TDDNativeCompletionFiles.md) |
| Added | [Docs/TDDNativeCompletionPortableTests.txt](../Docs/TDDNativeCompletionPortableTests.txt) |
| Added | [Docs/TDDNativeCompletionSourceChecks.txt](../Docs/TDDNativeCompletionSourceChecks.txt) |
| Added | [Docs/TDDNativeCompletionValidation.md](../Docs/TDDNativeCompletionValidation.md) |
| Added | [Docs/TechniqueAugmentEngineering.md](../Docs/TechniqueAugmentEngineering.md) |
| Modified | [Docs/UnrealValidation.md](../Docs/UnrealValidation.md) |
| Added | [Docs/WorldTransitEngineering.md](../Docs/WorldTransitEngineering.md) |

## Narrative Save System integration

| Change | Exact repository path |
|---|---|
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/NarrativeSavableActor.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/NarrativeSavableActor.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/NarrativeSavableComponent.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/NarrativeSavableComponent.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Private/Subsystems/NarrativeSaveSubsystem.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavableActor.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavableActor.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavableComponent.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavableComponent.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSave.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSave.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavePhases.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/NarrativeSavePhases.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/Subsystems/NarrativeSaveSubsystem.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeSaveSystem/Public/Subsystems/NarrativeSaveSubsystem.h) |

## Narrative runtime integration

| Change | Exact repository path |
|---|---|
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Camera/NarrativePlayerCameraManager.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Camera/NarrativePlayerCameraManager.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Cinematics/NarrativeLevelSequenceActor.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Cinematics/NarrativeLevelSequenceActor.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeAbilitySystemComponent.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeAbilitySystemComponent.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeAttributeSetBase.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeAttributeSetBase.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeBotAttackSelection.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeBotAttackSelection.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeCombatAbility.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeCombatAbility.cpp) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeCombatInputBuffer.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeCombatInputBuffer.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeDamageExecCalc.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/GAS/NarrativeDamageExecCalc.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Interaction/InteractableComponent.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Interaction/InteractableComponent.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Interaction/PlayerInteractionComponent.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Interaction/PlayerInteractionComponent.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Settings/NarrativeInputSettings.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Settings/NarrativeInputSettings.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Sovereign/SovGameplayTags.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Sovereign/SovGameplayTags.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/Dialogue.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/TalesComponent.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/Tales/TalesComponent.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeCharacter.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeCharacter.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeGameUserSettings.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativeGameUserSettings.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativePlayerCharacter.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativePlayerCharacter.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativePlayerController.cpp](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Private/UnrealFramework/NarrativePlayerController.cpp) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Cinematics/NarrativeLevelSequenceActor.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Cinematics/NarrativeLevelSequenceActor.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeAbilityInputMapping.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeAbilityInputMapping.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeAbilitySystemComponent.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeAbilitySystemComponent.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeBotAttackSelection.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeBotAttackSelection.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeCombatAbility.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeCombatAbility.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeDamageExecCalc.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/NarrativeDamageExecCalc.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovBotAttackCoordinator.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovBotAttackCoordinator.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovCombatInputPolicy.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovCombatInputPolicy.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovCombatTypes.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovCombatTypes.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovDamageChannelPolicy.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovDamageChannelPolicy.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovDamageSourcePolicy.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovDamageSourcePolicy.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovExertionProvider.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/GAS/SovExertionProvider.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Interaction/InteractableComponent.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Interaction/InteractableComponent.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Interaction/PlayerInteractionComponent.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Interaction/PlayerInteractionComponent.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Settings/NarrativeInputSettings.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Settings/NarrativeInputSettings.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/SkillTrees/SkillTreeComponent.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/SkillTrees/SkillTreeComponent.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Sovereign/SovGameplayTags.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Sovereign/SovGameplayTags.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Sovereign/SovLookInputPolicy.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Sovereign/SovLookInputPolicy.h) |
| Added | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Sovereign/SovMovementAssistPolicy.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Sovereign/SovMovementAssistPolicy.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/Dialogue.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/Dialogue.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/TalesComponent.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/Tales/TalesComponent.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeCharacter.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeCharacter.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeGameUserSettings.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeGameUserSettings.h) |
| Modified | [Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativePlayerController.h](../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativePlayerController.h) |

## Portable production-policy tests

| Change | Exact repository path |
|---|---|
| Added | [Scripts/Tests/SovSettingsPolicyTests.cpp](../Scripts/Tests/SovSettingsPolicyTests.cpp) |
| Added | [Tests/Portable/CombatInputPolicyTests.cpp](../Tests/Portable/CombatInputPolicyTests.cpp) |
| Added | [Tests/Portable/ExertionPolicyTests.cpp](../Tests/Portable/ExertionPolicyTests.cpp) |
| Added | [Tests/Portable/SovAccessibilityAssistPolicyTests.cpp](../Tests/Portable/SovAccessibilityAssistPolicyTests.cpp) |
| Added | [Tests/Portable/SovCampaignDependencyPolicyTests.cpp](../Tests/Portable/SovCampaignDependencyPolicyTests.cpp) |
| Added | [Tests/Portable/SovCarryPolicyTests.cpp](../Tests/Portable/SovCarryPolicyTests.cpp) |
| Added | [Tests/Portable/SovCinematicPolicyTests.cpp](../Tests/Portable/SovCinematicPolicyTests.cpp) |
| Modified | [Tests/Portable/SovCorruptionMathTests.cpp](../Tests/Portable/SovCorruptionMathTests.cpp) |
| Added | [Tests/Portable/SovEncounterCoordinationPolicyTests.cpp](../Tests/Portable/SovEncounterCoordinationPolicyTests.cpp) |
| Added | [Tests/Portable/SovEvidenceNarrativePolicyTests.cpp](../Tests/Portable/SovEvidenceNarrativePolicyTests.cpp) |
| Added | [Tests/Portable/SovFieldRecoveryPolicyTests.cpp](../Tests/Portable/SovFieldRecoveryPolicyTests.cpp) |
| Added | [Tests/Portable/SovMeleePolicyTests.cpp](../Tests/Portable/SovMeleePolicyTests.cpp) |
| Added | [Tests/Portable/SovNarrativeCuePolicyTests.cpp](../Tests/Portable/SovNarrativeCuePolicyTests.cpp) |
| Added | [Tests/Portable/SovRecoveryPolicyTests.cpp](../Tests/Portable/SovRecoveryPolicyTests.cpp) |
| Added | [Tests/Portable/SovRemainingCombatPolicyTests.cpp](../Tests/Portable/SovRemainingCombatPolicyTests.cpp) |
| Added | [Tests/Portable/SovResonancePolicyTests.cpp](../Tests/Portable/SovResonancePolicyTests.cpp) |
| Added | [Tests/Portable/SovSavePolicyTests.cpp](../Tests/Portable/SovSavePolicyTests.cpp) |
| Added | [Tests/Portable/SovWorldMotionPolicyTests.cpp](../Tests/Portable/SovWorldMotionPolicyTests.cpp) |

## Project native gameplay and campaign code

| Change | Exact repository path |
|---|---|
| Added | [Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_Finisher.cpp](../Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_Finisher.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_TarrikEcho.cpp](../Source/ProjectVelkorran/Private/Abilities/SovGameplayAbility_TarrikEcho.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Campaign/SovCampaignDefinition.cpp](../Source/ProjectVelkorran/Private/Campaign/SovCampaignDefinition.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovCampaignHandoffAnchor.cpp](../Source/ProjectVelkorran/Private/Campaign/SovCampaignHandoffAnchor.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovCampaignNarrativeQueries.cpp](../Source/ProjectVelkorran/Private/Campaign/SovCampaignNarrativeQueries.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Campaign/SovCampaignStateComponent.cpp](../Source/ProjectVelkorran/Private/Campaign/SovCampaignStateComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovEncounterCoordinationComponent.cpp](../Source/ProjectVelkorran/Private/Campaign/SovEncounterCoordinationComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovEncounterCoordinationPolicy.h](../Source/ProjectVelkorran/Private/Campaign/SovEncounterCoordinationPolicy.h) |
| Modified | [Source/ProjectVelkorran/Private/Campaign/SovEncounterDirector.cpp](../Source/ProjectVelkorran/Private/Campaign/SovEncounterDirector.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Campaign/SovEncounterSnapshotLibrary.cpp](../Source/ProjectVelkorran/Private/Campaign/SovEncounterSnapshotLibrary.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovEvidenceDefinition.cpp](../Source/ProjectVelkorran/Private/Campaign/SovEvidenceDefinition.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovEvidencePolicy.h](../Source/ProjectVelkorran/Private/Campaign/SovEvidencePolicy.h) |
| Modified | [Source/ProjectVelkorran/Private/Campaign/SovEvidenceSourceComponent.cpp](../Source/ProjectVelkorran/Private/Campaign/SovEvidenceSourceComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Campaign/SovNarrativeTypes.cpp](../Source/ProjectVelkorran/Private/Campaign/SovNarrativeTypes.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Characters/SovPlayerCharacterBase.cpp](../Source/ProjectVelkorran/Private/Characters/SovPlayerCharacterBase.cpp) |
| Added | [Source/ProjectVelkorran/Private/Cinematics/SovCampaignCinematicComponent.cpp](../Source/ProjectVelkorran/Private/Cinematics/SovCampaignCinematicComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Cinematics/SovCinematicPolicy.h](../Source/ProjectVelkorran/Private/Cinematics/SovCinematicPolicy.h) |
| Modified | [Source/ProjectVelkorran/Private/Combat/SovEchoAttackReceipt.cpp](../Source/ProjectVelkorran/Private/Combat/SovEchoAttackReceipt.cpp) |
| Added | [Source/ProjectVelkorran/Private/Combat/SovFinisherTargetComponent.cpp](../Source/ProjectVelkorran/Private/Combat/SovFinisherTargetComponent.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Combat/SovProtectionInterceptReceipt.cpp](../Source/ProjectVelkorran/Private/Combat/SovProtectionInterceptReceipt.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Combat/SovSelenePayload.cpp](../Source/ProjectVelkorran/Private/Combat/SovSelenePayload.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Companions/SovCoActionAnchor.cpp](../Source/ProjectVelkorran/Private/Companions/SovCoActionAnchor.cpp) |
| Added | [Source/ProjectVelkorran/Private/Companions/SovCompanionCommandActivity.cpp](../Source/ProjectVelkorran/Private/Companions/SovCompanionCommandActivity.cpp) |
| Added | [Source/ProjectVelkorran/Private/Companions/SovCompanionCommands.cpp](../Source/ProjectVelkorran/Private/Companions/SovCompanionCommands.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Companions/SovCompanionComponent.cpp](../Source/ProjectVelkorran/Private/Companions/SovCompanionComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Companions/SovConvergenceCompanionState.cpp](../Source/ProjectVelkorran/Private/Companions/SovConvergenceCompanionState.cpp) |
| Added | [Source/ProjectVelkorran/Private/Companions/SovProtagonistCompanionCharacter.cpp](../Source/ProjectVelkorran/Private/Companions/SovProtagonistCompanionCharacter.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Components/SovCorruptionComponent.cpp](../Source/ProjectVelkorran/Private/Components/SovCorruptionComponent.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Components/SovEchoComponent.cpp](../Source/ProjectVelkorran/Private/Components/SovEchoComponent.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Components/SovGuardComponent.cpp](../Source/ProjectVelkorran/Private/Components/SovGuardComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Corruption/SovCorruptionInteractableComponent.cpp](../Source/ProjectVelkorran/Private/Corruption/SovCorruptionInteractableComponent.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Corruption/SovCorruptionMath.h](../Source/ProjectVelkorran/Private/Corruption/SovCorruptionMath.h) |
| Modified | [Source/ProjectVelkorran/Private/Corruption/SovCorruptionProfile.cpp](../Source/ProjectVelkorran/Private/Corruption/SovCorruptionProfile.cpp) |
| Added | [Source/ProjectVelkorran/Private/Corruption/SovCorruptionSourceComponent.cpp](../Source/ProjectVelkorran/Private/Corruption/SovCorruptionSourceComponent.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Corruption/SovCorruptionSourceVolume.cpp](../Source/ProjectVelkorran/Private/Corruption/SovCorruptionSourceVolume.cpp) |
| Added | [Source/ProjectVelkorran/Private/Diagnostics/SovDiagnosticsPolicy.h](../Source/ProjectVelkorran/Private/Diagnostics/SovDiagnosticsPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/Diagnostics/SovDiagnosticsSubsystem.cpp](../Source/ProjectVelkorran/Private/Diagnostics/SovDiagnosticsSubsystem.cpp) |
| Added | [Source/ProjectVelkorran/Private/Diagnostics/SovLogChannels.cpp](../Source/ProjectVelkorran/Private/Diagnostics/SovLogChannels.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Effects/SovGameplayEffect_CorruptionBand.cpp](../Source/ProjectVelkorran/Private/Effects/SovGameplayEffect_CorruptionBand.cpp) |
| Added | [Source/ProjectVelkorran/Private/Exertion/SovExertionComponent.cpp](../Source/ProjectVelkorran/Private/Exertion/SovExertionComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Exertion/SovExertionPolicy.h](../Source/ProjectVelkorran/Private/Exertion/SovExertionPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/Exertion/SovGameplayAbility_Exertion.cpp](../Source/ProjectVelkorran/Private/Exertion/SovGameplayAbility_Exertion.cpp) |
| Added | [Source/ProjectVelkorran/Private/FieldRecovery/SovFieldRecoveryComponent.cpp](../Source/ProjectVelkorran/Private/FieldRecovery/SovFieldRecoveryComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/FieldRecovery/SovFieldRecoveryPolicy.h](../Source/ProjectVelkorran/Private/FieldRecovery/SovFieldRecoveryPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/FieldRecovery/SovFieldRecoveryStation.cpp](../Source/ProjectVelkorran/Private/FieldRecovery/SovFieldRecoveryStation.cpp) |
| Added | [Source/ProjectVelkorran/Private/FieldRecovery/SovGameplayAbility_FieldRecovery.cpp](../Source/ProjectVelkorran/Private/FieldRecovery/SovGameplayAbility_FieldRecovery.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Framework/SovCampaignGameMode.cpp](../Source/ProjectVelkorran/Private/Framework/SovCampaignGameMode.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Framework/SovPlayerController.cpp](../Source/ProjectVelkorran/Private/Framework/SovPlayerController.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Framework/SovPlayerState.cpp](../Source/ProjectVelkorran/Private/Framework/SovPlayerState.cpp) |
| Added | [Source/ProjectVelkorran/Private/Melee/SovAbilityTask_MeleeSweep.cpp](../Source/ProjectVelkorran/Private/Melee/SovAbilityTask_MeleeSweep.cpp) |
| Added | [Source/ProjectVelkorran/Private/Melee/SovGameplayAbility_Melee.cpp](../Source/ProjectVelkorran/Private/Melee/SovGameplayAbility_Melee.cpp) |
| Added | [Source/ProjectVelkorran/Private/Melee/SovMeleeAttackDefinition.cpp](../Source/ProjectVelkorran/Private/Melee/SovMeleeAttackDefinition.cpp) |
| Added | [Source/ProjectVelkorran/Private/Narrative/SovCampaignNarrativeAdapters.cpp](../Source/ProjectVelkorran/Private/Narrative/SovCampaignNarrativeAdapters.cpp) |
| Added | [Source/ProjectVelkorran/Private/Narrative/SovNarrativeCue.cpp](../Source/ProjectVelkorran/Private/Narrative/SovNarrativeCue.cpp) |
| Added | [Source/ProjectVelkorran/Private/Narrative/SovNarrativeCueComponent.cpp](../Source/ProjectVelkorran/Private/Narrative/SovNarrativeCueComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Narrative/SovNarrativeGraphPolicy.h](../Source/ProjectVelkorran/Private/Narrative/SovNarrativeGraphPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/Narrative/SovNarrativeValidationLibrary.cpp](../Source/ProjectVelkorran/Private/Narrative/SovNarrativeValidationLibrary.cpp) |
| Added | [Source/ProjectVelkorran/Private/Narrative/SovViewmakerLibrary.cpp](../Source/ProjectVelkorran/Private/Narrative/SovViewmakerLibrary.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Progression/SovTechniqueComponent.cpp](../Source/ProjectVelkorran/Private/Progression/SovTechniqueComponent.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Progression/SovTechniqueSafePoint.cpp](../Source/ProjectVelkorran/Private/Progression/SovTechniqueSafePoint.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Progression/SovTechniqueTypes.cpp](../Source/ProjectVelkorran/Private/Progression/SovTechniqueTypes.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Projectiles/SovReformationDroneRocketProjectile.cpp](../Source/ProjectVelkorran/Private/Projectiles/SovReformationDroneRocketProjectile.cpp) |
| Added | [Source/ProjectVelkorran/Private/Recovery/SovFatalRecoveryComponent.cpp](../Source/ProjectVelkorran/Private/Recovery/SovFatalRecoveryComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Recovery/SovRecoveryExclusionVolume.cpp](../Source/ProjectVelkorran/Private/Recovery/SovRecoveryExclusionVolume.cpp) |
| Added | [Source/ProjectVelkorran/Private/Resonance/SovResonanceAbility.cpp](../Source/ProjectVelkorran/Private/Resonance/SovResonanceAbility.cpp) |
| Added | [Source/ProjectVelkorran/Private/Resonance/SovResonanceComponent.cpp](../Source/ProjectVelkorran/Private/Resonance/SovResonanceComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Resonance/SovResonanceTargetComponent.cpp](../Source/ProjectVelkorran/Private/Resonance/SovResonanceTargetComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Save/SovCampaignSaveGame.cpp](../Source/ProjectVelkorran/Private/Save/SovCampaignSaveGame.cpp) |
| Added | [Source/ProjectVelkorran/Private/Save/SovSavePolicy.h](../Source/ProjectVelkorran/Private/Save/SovSavePolicy.h) |
| Added | [Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp](../Source/ProjectVelkorran/Private/Save/SovSaveSubsystem.cpp) |
| Added | [Source/ProjectVelkorran/Private/Settings/SovGameUserSettings.cpp](../Source/ProjectVelkorran/Private/Settings/SovGameUserSettings.cpp) |
| Added | [Source/ProjectVelkorran/Private/Settings/SovSettingsPolicy.h](../Source/ProjectVelkorran/Private/Settings/SovSettingsPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/Targeting/SovAimAssist.cpp](../Source/ProjectVelkorran/Private/Targeting/SovAimAssist.cpp) |
| Added | [Source/ProjectVelkorran/Private/Targeting/SovAimAssistPolicy.h](../Source/ProjectVelkorran/Private/Targeting/SovAimAssistPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/Targeting/SovTargetingComponent.cpp](../Source/ProjectVelkorran/Private/Targeting/SovTargetingComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/Validation/SovCampaignDependencyPolicy.h](../Source/ProjectVelkorran/Private/Validation/SovCampaignDependencyPolicy.h) |
| Modified | [Source/ProjectVelkorran/Private/Validation/SovValidateCampaignCommandlet.cpp](../Source/ProjectVelkorran/Private/Validation/SovValidateCampaignCommandlet.cpp) |
| Added | [Source/ProjectVelkorran/Private/World/SovCarryPolicy.h](../Source/ProjectVelkorran/Private/World/SovCarryPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/World/SovCarryTargetComponent.cpp](../Source/ProjectVelkorran/Private/World/SovCarryTargetComponent.cpp) |
| Added | [Source/ProjectVelkorran/Private/World/SovRescueDestination.cpp](../Source/ProjectVelkorran/Private/World/SovRescueDestination.cpp) |
| Added | [Source/ProjectVelkorran/Private/World/SovTraversalAnchor.cpp](../Source/ProjectVelkorran/Private/World/SovTraversalAnchor.cpp) |
| Added | [Source/ProjectVelkorran/Private/World/SovWorldMotionPolicy.h](../Source/ProjectVelkorran/Private/World/SovWorldMotionPolicy.h) |
| Added | [Source/ProjectVelkorran/Private/World/SovWorldTransitActor.cpp](../Source/ProjectVelkorran/Private/World/SovWorldTransitActor.cpp) |
| Added | [Source/ProjectVelkorran/Public/Abilities/SovGameplayAbility_Finisher.h](../Source/ProjectVelkorran/Public/Abilities/SovGameplayAbility_Finisher.h) |
| Modified | [Source/ProjectVelkorran/Public/Campaign/SovCampaignDefinition.h](../Source/ProjectVelkorran/Public/Campaign/SovCampaignDefinition.h) |
| Added | [Source/ProjectVelkorran/Public/Campaign/SovCampaignHandoffAnchor.h](../Source/ProjectVelkorran/Public/Campaign/SovCampaignHandoffAnchor.h) |
| Added | [Source/ProjectVelkorran/Public/Campaign/SovCampaignProtagonistProfile.h](../Source/ProjectVelkorran/Public/Campaign/SovCampaignProtagonistProfile.h) |
| Modified | [Source/ProjectVelkorran/Public/Campaign/SovCampaignStateComponent.h](../Source/ProjectVelkorran/Public/Campaign/SovCampaignStateComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Campaign/SovEncounterCoordinationComponent.h](../Source/ProjectVelkorran/Public/Campaign/SovEncounterCoordinationComponent.h) |
| Modified | [Source/ProjectVelkorran/Public/Campaign/SovEncounterDirector.h](../Source/ProjectVelkorran/Public/Campaign/SovEncounterDirector.h) |
| Modified | [Source/ProjectVelkorran/Public/Campaign/SovEncounterTypes.h](../Source/ProjectVelkorran/Public/Campaign/SovEncounterTypes.h) |
| Added | [Source/ProjectVelkorran/Public/Campaign/SovEvidenceDefinition.h](../Source/ProjectVelkorran/Public/Campaign/SovEvidenceDefinition.h) |
| Modified | [Source/ProjectVelkorran/Public/Campaign/SovEvidenceSourceComponent.h](../Source/ProjectVelkorran/Public/Campaign/SovEvidenceSourceComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Campaign/SovNarrativeTypes.h](../Source/ProjectVelkorran/Public/Campaign/SovNarrativeTypes.h) |
| Modified | [Source/ProjectVelkorran/Public/Characters/SovPlayerCharacterBase.h](../Source/ProjectVelkorran/Public/Characters/SovPlayerCharacterBase.h) |
| Added | [Source/ProjectVelkorran/Public/Cinematics/SovCampaignCinematicComponent.h](../Source/ProjectVelkorran/Public/Cinematics/SovCampaignCinematicComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Combat/SovFinisherPolicy.h](../Source/ProjectVelkorran/Public/Combat/SovFinisherPolicy.h) |
| Added | [Source/ProjectVelkorran/Public/Combat/SovFinisherTargetComponent.h](../Source/ProjectVelkorran/Public/Combat/SovFinisherTargetComponent.h) |
| Modified | [Source/ProjectVelkorran/Public/Combat/SovProtectionInterceptReceipt.h](../Source/ProjectVelkorran/Public/Combat/SovProtectionInterceptReceipt.h) |
| Added | [Source/ProjectVelkorran/Public/Companions/SovCompanionCommandActivity.h](../Source/ProjectVelkorran/Public/Companions/SovCompanionCommandActivity.h) |
| Modified | [Source/ProjectVelkorran/Public/Companions/SovCompanionComponent.h](../Source/ProjectVelkorran/Public/Companions/SovCompanionComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Companions/SovConvergenceCompanionState.h](../Source/ProjectVelkorran/Public/Companions/SovConvergenceCompanionState.h) |
| Added | [Source/ProjectVelkorran/Public/Companions/SovProtagonistCompanionCharacter.h](../Source/ProjectVelkorran/Public/Companions/SovProtagonistCompanionCharacter.h) |
| Modified | [Source/ProjectVelkorran/Public/Components/SovCorruptionComponent.h](../Source/ProjectVelkorran/Public/Components/SovCorruptionComponent.h) |
| Modified | [Source/ProjectVelkorran/Public/Components/SovGuardComponent.h](../Source/ProjectVelkorran/Public/Components/SovGuardComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Corruption/SovCorruptionInteractableComponent.h](../Source/ProjectVelkorran/Public/Corruption/SovCorruptionInteractableComponent.h) |
| Modified | [Source/ProjectVelkorran/Public/Corruption/SovCorruptionProfile.h](../Source/ProjectVelkorran/Public/Corruption/SovCorruptionProfile.h) |
| Added | [Source/ProjectVelkorran/Public/Corruption/SovCorruptionSourceComponent.h](../Source/ProjectVelkorran/Public/Corruption/SovCorruptionSourceComponent.h) |
| Modified | [Source/ProjectVelkorran/Public/Corruption/SovCorruptionSourceVolume.h](../Source/ProjectVelkorran/Public/Corruption/SovCorruptionSourceVolume.h) |
| Added | [Source/ProjectVelkorran/Public/Diagnostics/SovDiagnosticsSubsystem.h](../Source/ProjectVelkorran/Public/Diagnostics/SovDiagnosticsSubsystem.h) |
| Added | [Source/ProjectVelkorran/Public/Diagnostics/SovLogChannels.h](../Source/ProjectVelkorran/Public/Diagnostics/SovLogChannels.h) |
| Added | [Source/ProjectVelkorran/Public/Exertion/SovExertionComponent.h](../Source/ProjectVelkorran/Public/Exertion/SovExertionComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Exertion/SovGameplayAbility_Exertion.h](../Source/ProjectVelkorran/Public/Exertion/SovGameplayAbility_Exertion.h) |
| Added | [Source/ProjectVelkorran/Public/FieldRecovery/SovFieldRecoveryComponent.h](../Source/ProjectVelkorran/Public/FieldRecovery/SovFieldRecoveryComponent.h) |
| Added | [Source/ProjectVelkorran/Public/FieldRecovery/SovFieldRecoveryStation.h](../Source/ProjectVelkorran/Public/FieldRecovery/SovFieldRecoveryStation.h) |
| Added | [Source/ProjectVelkorran/Public/FieldRecovery/SovGameplayAbility_FieldRecovery.h](../Source/ProjectVelkorran/Public/FieldRecovery/SovGameplayAbility_FieldRecovery.h) |
| Modified | [Source/ProjectVelkorran/Public/Framework/SovPlayerController.h](../Source/ProjectVelkorran/Public/Framework/SovPlayerController.h) |
| Added | [Source/ProjectVelkorran/Public/Melee/SovAbilityTask_MeleeSweep.h](../Source/ProjectVelkorran/Public/Melee/SovAbilityTask_MeleeSweep.h) |
| Added | [Source/ProjectVelkorran/Public/Melee/SovGameplayAbility_Melee.h](../Source/ProjectVelkorran/Public/Melee/SovGameplayAbility_Melee.h) |
| Added | [Source/ProjectVelkorran/Public/Melee/SovMeleeAttackDefinition.h](../Source/ProjectVelkorran/Public/Melee/SovMeleeAttackDefinition.h) |
| Added | [Source/ProjectVelkorran/Public/Melee/SovMeleePolicy.h](../Source/ProjectVelkorran/Public/Melee/SovMeleePolicy.h) |
| Added | [Source/ProjectVelkorran/Public/Narrative/SovCampaignNarrativeAdapters.h](../Source/ProjectVelkorran/Public/Narrative/SovCampaignNarrativeAdapters.h) |
| Added | [Source/ProjectVelkorran/Public/Narrative/SovNarrativeCue.h](../Source/ProjectVelkorran/Public/Narrative/SovNarrativeCue.h) |
| Added | [Source/ProjectVelkorran/Public/Narrative/SovNarrativeCueComponent.h](../Source/ProjectVelkorran/Public/Narrative/SovNarrativeCueComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Narrative/SovNarrativeCuePolicy.h](../Source/ProjectVelkorran/Public/Narrative/SovNarrativeCuePolicy.h) |
| Added | [Source/ProjectVelkorran/Public/Narrative/SovNarrativeValidationLibrary.h](../Source/ProjectVelkorran/Public/Narrative/SovNarrativeValidationLibrary.h) |
| Added | [Source/ProjectVelkorran/Public/Narrative/SovViewmakerLibrary.h](../Source/ProjectVelkorran/Public/Narrative/SovViewmakerLibrary.h) |
| Modified | [Source/ProjectVelkorran/Public/Progression/SovTechniqueComponent.h](../Source/ProjectVelkorran/Public/Progression/SovTechniqueComponent.h) |
| Modified | [Source/ProjectVelkorran/Public/Progression/SovTechniqueSafePoint.h](../Source/ProjectVelkorran/Public/Progression/SovTechniqueSafePoint.h) |
| Modified | [Source/ProjectVelkorran/Public/Progression/SovTechniqueTypes.h](../Source/ProjectVelkorran/Public/Progression/SovTechniqueTypes.h) |
| Added | [Source/ProjectVelkorran/Public/Projectiles/SovProjectileDefensePolicy.h](../Source/ProjectVelkorran/Public/Projectiles/SovProjectileDefensePolicy.h) |
| Modified | [Source/ProjectVelkorran/Public/Projectiles/SovReformationDroneRocketProjectile.h](../Source/ProjectVelkorran/Public/Projectiles/SovReformationDroneRocketProjectile.h) |
| Added | [Source/ProjectVelkorran/Public/Recovery/SovFatalRecoveryComponent.h](../Source/ProjectVelkorran/Public/Recovery/SovFatalRecoveryComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Recovery/SovRecoveryExclusionVolume.h](../Source/ProjectVelkorran/Public/Recovery/SovRecoveryExclusionVolume.h) |
| Added | [Source/ProjectVelkorran/Public/Recovery/SovRecoveryPolicy.h](../Source/ProjectVelkorran/Public/Recovery/SovRecoveryPolicy.h) |
| Added | [Source/ProjectVelkorran/Public/Resonance/SovResonanceAbility.h](../Source/ProjectVelkorran/Public/Resonance/SovResonanceAbility.h) |
| Added | [Source/ProjectVelkorran/Public/Resonance/SovResonanceComponent.h](../Source/ProjectVelkorran/Public/Resonance/SovResonanceComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Resonance/SovResonancePolicy.h](../Source/ProjectVelkorran/Public/Resonance/SovResonancePolicy.h) |
| Added | [Source/ProjectVelkorran/Public/Resonance/SovResonanceTargetComponent.h](../Source/ProjectVelkorran/Public/Resonance/SovResonanceTargetComponent.h) |
| Added | [Source/ProjectVelkorran/Public/Resonance/SovResonanceTypes.h](../Source/ProjectVelkorran/Public/Resonance/SovResonanceTypes.h) |
| Added | [Source/ProjectVelkorran/Public/Save/SovCampaignSaveGame.h](../Source/ProjectVelkorran/Public/Save/SovCampaignSaveGame.h) |
| Added | [Source/ProjectVelkorran/Public/Save/SovSaveSubsystem.h](../Source/ProjectVelkorran/Public/Save/SovSaveSubsystem.h) |
| Added | [Source/ProjectVelkorran/Public/Settings/SovGameUserSettings.h](../Source/ProjectVelkorran/Public/Settings/SovGameUserSettings.h) |
| Added | [Source/ProjectVelkorran/Public/Targeting/SovAimAssist.h](../Source/ProjectVelkorran/Public/Targeting/SovAimAssist.h) |
| Added | [Source/ProjectVelkorran/Public/Targeting/SovTargetingComponent.h](../Source/ProjectVelkorran/Public/Targeting/SovTargetingComponent.h) |
| Added | [Source/ProjectVelkorran/Public/World/SovCarryTargetComponent.h](../Source/ProjectVelkorran/Public/World/SovCarryTargetComponent.h) |
| Added | [Source/ProjectVelkorran/Public/World/SovRescueDestination.h](../Source/ProjectVelkorran/Public/World/SovRescueDestination.h) |
| Added | [Source/ProjectVelkorran/Public/World/SovTraversalAnchor.h](../Source/ProjectVelkorran/Public/World/SovTraversalAnchor.h) |
| Added | [Source/ProjectVelkorran/Public/World/SovWorldTransitActor.h](../Source/ProjectVelkorran/Public/World/SovWorldTransitActor.h) |

## Unreal automation sources and fixtures

| Change | Exact repository path |
|---|---|
| Added | [Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovCinematicLifecycleRuntimeTests.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Tests/SovCombatRoutingRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovCombatRoutingRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTestFixtures.cpp](../Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTestFixtures.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovCoordinationRuntimeTests.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Tests/SovCorruptionRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovCorruptionRuntimeTestFixtures.h) |
| Modified | [Source/ProjectVelkorran/Private/Tests/SovCorruptionRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovCorruptionRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovExertionRuntimeTestFixtures.cpp](../Source/ProjectVelkorran/Private/Tests/SovExertionRuntimeTestFixtures.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovExertionRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovExertionRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovExertionRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovExertionRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovFieldRecoveryRuntimeTestFixtures.cpp](../Source/ProjectVelkorran/Private/Tests/SovFieldRecoveryRuntimeTestFixtures.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovFieldRecoveryRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovFieldRecoveryRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovFieldRecoveryRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovFieldRecoveryRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovFinisherProjectileRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovFinisherProjectileRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovInputRoutingRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovInputRoutingRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovMeleeRuntimeTestFixtures.cpp](../Source/ProjectVelkorran/Private/Tests/SovMeleeRuntimeTestFixtures.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovMeleeRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovMeleeRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovMeleeRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovMeleeRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeFixtures.cpp](../Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeFixtures.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovNarrativeCueRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovNarrativeRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovNarrativeRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovNarrativeStateRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovNarrativeStateRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovRecoveryRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovRecoveryRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovRecoveryRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovRecoveryRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovResonanceRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovResonanceRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovRestoreCallbackRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovRestoreCallbackRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovSaveRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovSaveRuntimeTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovSaveRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovSaveRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovSettingsRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovSettingsRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovSettingsTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovSettingsTestFixtures.h) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovTargetingRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovTargetingRuntimeTests.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTestFixtures.cpp](../Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTestFixtures.cpp) |
| Modified | [Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTestFixtures.h](../Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTestFixtures.h) |
| Modified | [Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovTechniqueRuntimeTests.cpp) |
| Added | [Source/ProjectVelkorran/Private/Tests/SovWorldRuntimeTests.cpp](../Source/ProjectVelkorran/Private/Tests/SovWorldRuntimeTests.cpp) |
