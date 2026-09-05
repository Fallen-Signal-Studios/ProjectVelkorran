// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSettingsTestFixtures.h"
#include "Sovereign/SovGameplayTags.h"
#include "NarrativeGameplayTags.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInputRoutingWorldTest, "ProjectVelkorran.Campaign.Settings.SemanticToggleChordAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovInputRoutingWorldTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!World) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false));
	auto* Controller = World->SpawnActor<ASovInputRoutingTestController>();
	if (!Controller) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } return false; }
	auto* Probe = NewObject<USovInputRoutingProbe>(Controller);
	Controller->OnSemanticInputChanged.AddDynamic(Probe, &USovInputRoutingProbe::OnInput);
	const FGameplayTag Modifier = FSovGameplayTags::Get().Input_AbilityModifier;
	const FGameplayTag Attack = FNarrativeGameplayTags::Get().Narrative_Input_Attack;
	const FGameplayTag Ability = FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	Controller->TestToggleTags.Add(Modifier);
	auto* Mappings = NewObject<UNarrativeAbilityInputMapping>(Controller);
	FAbilityInputMappingData Mapping; Mapping.InputTag = Attack; Mapping.RequiredModifierTag = Modifier; Mapping.ModifiedInputTag = Ability;
	Mappings->InputAbilities.Add(Mapping); Controller->SetTestMappings(Mappings);
	Controller->AbilityInputPressed(Modifier); Controller->AbilityInputReleased(Modifier);
	TestTrue(TEXT("Toggle remains held after physical release"), Controller->PressedAbilityInputTags.Contains(Modifier));
	Controller->AbilityInputPressed(Attack);
	TestTrue(TEXT("Modifier routes semantic ability press"), Controller->PressedAbilityInputTags.Contains(Ability));
	TestFalse(TEXT("Original attack not simultaneously invoked"), Controller->PressedAbilityInputTags.Contains(Attack));
	Controller->AbilityInputReleased(Attack);
	TestFalse(TEXT("Release follows route captured on press"), Controller->PressedAbilityInputTags.Contains(Ability));
	Controller->AbilityInputPressed(Modifier); Controller->AbilityInputReleased(Modifier);
	TestFalse(TEXT("Second press toggles off"), Controller->PressedAbilityInputTags.Contains(Modifier));
	TestEqual(TEXT("Physical release does not duplicate semantic release"), Probe->Releases.Num(), 2);
	Controller->AbilityInputPressed(Modifier);
	Controller->AbilityInputCanceled(Modifier);
	TestFalse(TEXT("Canceled context always clears latched input"), Controller->PressedAbilityInputTags.Contains(Modifier));
	Controller->AbilityInputPressed(Modifier); Controller->AbilityInputPressed(Attack);
	Controller->SuppressTestInputs();
	TestTrue(TEXT("Modal/focus suppression clears all semantic holds"), Controller->PressedAbilityInputTags.IsEmpty());
	TestTrue(TEXT("Modal/focus suppression clears route history"), Controller->SovInputRoutes.IsEmpty());
	TestTrue(TEXT("Modal/focus suppression clears toggle latches"), Controller->SovLatchedInputTags.IsEmpty());
	// Two source routes share an effective action: releasing either alone must not release the other.
	const FGameplayTag SecondRoute = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	FAbilityInputMappingData Other = Mapping; Other.InputTag = SecondRoute; Mappings->InputAbilities.Add(Other);
	Controller->AbilityInputPressed(Modifier); Controller->AbilityInputPressed(Attack); Controller->AbilityInputPressed(SecondRoute);
	Controller->AbilityInputReleased(Attack);
	TestTrue(TEXT("Second route retains semantic hold"), Controller->PressedAbilityInputTags.Contains(Ability));
	Controller->AbilityInputReleased(SecondRoute);
	TestFalse(TEXT("Final route releases semantic action"), Controller->PressedAbilityInputTags.Contains(Ability));
	Controller->SuppressTestInputs();
	// Real GAS spec InputPressed proves outer release cannot undo a fresh same-ASC press from the event callback.
	Controller->TestASC = NewObject<UNarrativeAbilitySystemComponent>(Controller);
	Controller->TestASC->RegisterComponent(); Controller->TestASC->InitAbilityActorInfo(Controller, Controller);
	FGameplayAbilitySpec Spec(UGameplayAbility::StaticClass(), 1); Spec.GetDynamicSpecSourceTags().AddTag(Attack);
	const FGameplayAbilitySpecHandle Handle = Controller->TestASC->GiveAbility(Spec);
	Controller->AbilityInputPressed(Attack);
	Probe->RepressController = Controller; Probe->RepressTag = Attack; Probe->bRepressOnce = true;
	Controller->AbilityInputReleased(Attack);
	TestTrue(TEXT("Reentrant press retains current held state"), Controller->PressedAbilityInputTags.Contains(Attack));
	const FGameplayAbilitySpec* LiveSpec = Controller->TestASC->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Outer release cannot release new same-ASC input"), LiveSpec && LiveSpec->InputPressed);
	Controller->SuppressTestInputs();
	Controller->AbilityInputPressed(Attack); Probe->bRepressOnce = true;
	Controller->SuppressTestInputs();
	LiveSpec = Controller->TestASC->FindAbilitySpecFromHandle(Handle);
	TestTrue(TEXT("Bulk release callback preserves the fresh same-ASC press"), Controller->PressedAbilityInputTags.Contains(Attack) && LiveSpec && LiveSpec->InputPressed);
	Controller->SuppressTestInputs();
	Controller->TestToggleTags.Add(Attack); Controller->AbilityInputPressed(Attack); Controller->AbilityInputReleased(Attack);
	Controller->TestASC->CancelAllAbilities();
	Probe->bSuppressOnce = true;
	Controller->AbilityInputPressed(Attack);
	TestFalse(TEXT("Suppression during stale-toggle release cannot start another outer press"), Controller->PressedAbilityInputTags.Contains(Attack));
	Controller->SuppressTestInputs();
	const FGameplayTag Sprint = FNarrativeGameplayTags::Get().Narrative_Input_Sprint;
	Controller->TestToggleTags.Add(Sprint);
	Controller->SetAutomaticSprintHeld(true);
	TestTrue(TEXT("Automatic request uses the real semantic sprint hold"), Controller->PressedAbilityInputTags.Contains(Sprint));
	TestFalse(TEXT("Automatic request never creates a toggle latch"), Controller->SovLatchedInputTags.Contains(Sprint));
	Controller->AbilityInputPressed(Sprint);
	Controller->SetAutomaticSprintHeld(false);
	TestTrue(TEXT("Manual sprint route survives automatic stop"), Controller->PressedAbilityInputTags.Contains(Sprint));
	Controller->AbilityInputReleased(Sprint);
	TestFalse(TEXT("Final manual release stops the shared semantic hold"), Controller->PressedAbilityInputTags.Contains(Sprint));
	Controller->SetAutomaticSprintHeld(true); Controller->SuppressTestInputs();
	TestFalse(TEXT("Modal cancellation clears automatic ownership"), Controller->bSovAutomaticSprintHeld);
	TestFalse(TEXT("Modal cancellation releases automatic sprint"), Controller->PressedAbilityInputTags.Contains(Sprint));
	World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } return true;
}
#endif
