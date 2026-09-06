// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovStatusCheckpointTestFixtures.h"

#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "Components/SovStatusComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeSave.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovStatusCheckpointLifecycleTests
{
	struct FWorld
	{
		UWorld* World = nullptr;
		uint64 TimerFrame = GFrameCounter;
		FWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
				World->GetTimerManager().Tick(0.f);
			}
		}
		~FWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		void AdvanceTimers()
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++TimerFrame);
			World->GetTimerManager().Tick(.01f);
		}
	};

	bool SeedPersistentRecord(AActor* Actor, USovStatusComponent* Status, FNarrativeSaveComponent& Record)
	{
		const FGameplayTag Tag = FSovGameplayTags::Get().Status_Apply_Exposed;
		USovStatusDefinition* Definition = Status ? Status->GetStatusDefinition(Tag) : nullptr;
		if (!Definition) { return false; }
		Definition->CheckpointBehavior = ESovStatusCheckpointBehavior::PersistRemainingDuration;
		if (Status->ApplyStatusByTag(Tag, Actor, 2.f, 20.f, 3.f) != ESovStatusApplicationResult::Applied)
		{
			return false;
		}
		return USovEncounterSnapshotLibrary::CaptureComponent(Status, Record)
			&& Status->RemoveStatus(Tag, false);
	}

	struct FNarrativeActorFixture : FWorld
	{
		ASovAxiomRuntimeTestCharacter* Actor = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		USovStatusComponent* Status = nullptr;
		FNarrativeActorFixture()
		{
			Actor = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>() : nullptr;
			if (!Actor) { return; }
			Actor->InitializeTestCombat(1);
			ASC = Actor->GetNarrativeAbilitySystemComponent();
			Status = NewObject<USovStatusComponent>(Actor, TEXT("CheckpointLifecycleStatus"));
			Actor->AddInstanceComponent(Status);
			Status->RegisterComponent();
			Status->InitializeWithAbilitySystem(ASC);
		}
	};
}

using namespace SovStatusCheckpointLifecycleTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointManagedReadinessTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.ManagedPlayerReadinessBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointManagedReadinessTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Controller = Fixture.World->SpawnActor<ASovHandoffRuntimeTestController>();
	auto* Pawn = Fixture.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	auto* PlayerState = Fixture.World->SpawnActor<ASovPlayerState>();
	if (!Controller || !Pawn || !PlayerState) { AddError(TEXT("Readiness fixture creation failed")); return false; }
	auto* Definition = NewObject<UPlayerDefinition>(Controller);
	Controller->KeepAlive.Add(Definition);
	if (!TestTrue(TEXT("Managed definition stages before possession"), Pawn->PrepareCampaignInitialization(Definition))) { return false; }
	Controller->SetTestPlayerState(PlayerState);
	Controller->Possess(Pawn);
	if (!TestTrue(TEXT("Production project components bind before public readiness"), Pawn->StageTestReadiness(PlayerState, true))) { return false; }
	USovStatusComponent* Status = Pawn->FindComponentByClass<USovStatusComponent>();
	UNarrativeAbilitySystemComponent* ASC = Pawn->GetNarrativeAbilitySystemComponent();
	if (!Status || !ASC) { AddError(TEXT("Bound production status/ASC missing")); return false; }
	TestTrue(TEXT("Status initialization does not circularly require player readiness"), Status->IsInitialized());
	TestTrue(TEXT("Data gate remains open while public readiness is pending"), Pawn->IsCampaignDataReadyToApply());
	TestFalse(TEXT("Public gameplay readiness has not yet published"), Pawn->IsCharacterReady());
	FNarrativeSaveComponent Record;
	if (!TestTrue(TEXT("Persistent record uses real component serialization"), SeedPersistentRecord(Pawn, Status, Record))) { return false; }
	if (!TestTrue(TEXT("Native component load accepts the delayed snapshot"), USovEncounterSnapshotLibrary::RestoreComponent(Status, Record)
		&& Status->WasSaveRecordLoadAccepted())) { return false; }
	const FGameplayTag Tag = FSovGameplayTags::Get().Status_Apply_Exposed;
	TestFalse(TEXT("Binding alone cannot install status before saved resources"), Status->HasActiveStatus(Tag));
	TestEqual(TEXT("Save before readiness retains the full queued snapshot"), Status->CaptureCheckpointState().Statuses.Num(), 1);
	FSovCombatResourceSnapshot Resources;
	if (!USovEncounterSnapshotLibrary::CaptureResources(ASC, Resources)) { AddError(TEXT("Resource capture failed")); return false; }
	Resources.Health = 77.f;
	TestTrue(TEXT("Authored currents update explicit snapshot bases"), USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(ASC, Resources));
	TestTrue(TEXT("Resource boundary accepts the queue awaiting managed readiness"), USovEncounterSnapshotLibrary::RestoreResources(ASC, Resources));
	TestFalse(TEXT("Resource restoration does not publish public readiness"), Pawn->IsCharacterReady());
	TestFalse(TEXT("Status remains pending until the actual readiness event"), Status->HasActiveStatus(Tag));
	if (!TestTrue(TEXT("Real campaign completion publishes readiness"), Pawn->CompleteCampaignDataInitialization(false))) { return false; }
	TestTrue(TEXT("Ready event commits the queued status synchronously"), Status->HasActiveStatus(Tag));
	TestEqual(TEXT("Status restore follows the saved Health value"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 77.f);
	TestTrue(TEXT("Repeated completion is idempotent"), Status->CompletePendingCheckpointRestore());
	TestEqual(TEXT("Repeated completion keeps one semantic status"), Status->GetActiveStatusPresentation().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointNativeResourceBoundaryTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.NarrativeResourceBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointNativeResourceBoundaryTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FNarrativeActorFixture Fixture;
	if (!Fixture.Actor || !Fixture.ASC || !Fixture.Status) { AddError(TEXT("Narrative fixture creation failed")); return false; }
	FNarrativeSaveComponent Record;
	if (!TestTrue(TEXT("Persistent record captured"), SeedPersistentRecord(Fixture.Actor, Fixture.Status, Record))) { return false; }
	FSovCombatResourceSnapshot Resources;
	if (!USovEncounterSnapshotLibrary::CaptureResources(Fixture.ASC, Resources)) { AddError(TEXT("Resource capture failed")); return false; }
	Resources.Health = 73.f;
	TestTrue(TEXT("Authored currents update explicit snapshot bases"), USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(Fixture.ASC, Resources));
	Fixture.ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 11.f);
	TestTrue(TEXT("Native Narrative status record queues"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Status, Record)
		&& Fixture.Status->WasSaveRecordLoadAccepted());
	const auto& Tags = FSovGameplayTags::Get();
	TestFalse(TEXT("Saved effect is absent before the resource boundary"), Fixture.Status->HasActiveStatus(Tags.Status_Apply_Exposed));
	int32 AddedEffects = 0;
	float HealthAtApplication = 0.f;
	const FDelegateHandle Applied = Fixture.ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&](UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle)
		{
			if (Spec.DynamicGrantedTags.HasTagExact(Tags.State_Status_Exposed))
			{
				++AddedEffects;
				HealthAtApplication = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
			}
		});
	TestTrue(TEXT("Production resource restore flushes pending status before returning"), USovEncounterSnapshotLibrary::RestoreResources(Fixture.ASC, Resources));
	TestTrue(TEXT("Saved constraints are active before actor release"), Fixture.Status->HasActiveStatus(Tags.Status_Apply_Exposed));
	TestEqual(TEXT("Status effect saw restored Health, not pre-load Health"), HealthAtApplication, 73.f);
	Fixture.AdvanceTimers();
	Fixture.AdvanceTimers();
	TestEqual(TEXT("Cancelled fallback cannot duplicate the explicit restore"), AddedEffects, 1);
	TestEqual(TEXT("One semantic status remains"), Fixture.Status->GetActiveStatusPresentation().Num(), 1);
	Fixture.ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Applied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointGenericNarrativeFallbackTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.GenericNarrativeOneShotBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointGenericNarrativeFallbackTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FNarrativeActorFixture Fixture;
	if (!Fixture.Actor || !Fixture.ASC || !Fixture.Status) { AddError(TEXT("Narrative fixture creation failed")); return false; }
	FNarrativeSaveComponent Record;
	if (!SeedPersistentRecord(Fixture.Actor, Fixture.Status, Record)) { AddError(TEXT("Persistent record capture failed")); return false; }
	TestTrue(TEXT("Generic native load queues without a campaign coordinator"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Status, Record)
		&& Fixture.Status->WasSaveRecordLoadAccepted());
	const FGameplayTag Tag = FSovGameplayTags::Get().Status_Apply_Exposed;
	TestFalse(TEXT("Record remains staged during the synchronous generic load"), Fixture.Status->HasActiveStatus(Tag));
	Fixture.AdvanceTimers();
	Fixture.AdvanceTimers();
	TestTrue(TEXT("One-shot generic boundary completes after the load call stack"), Fixture.Status->HasActiveStatus(Tag));
	TestTrue(TEXT("Repeated completion is harmless"), Fixture.Status->CompletePendingCheckpointRestore());
	TestEqual(TEXT("Fallback and explicit completion cannot double the record"), Fixture.Status->GetActiveStatusPresentation().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointClearModifierBoundaryTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.ClearDefaultModifierPrecedesResourceWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointClearModifierBoundaryTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FNarrativeActorFixture Fixture;
	if (!Fixture.Actor || !Fixture.ASC || !Fixture.Status) { AddError(TEXT("Narrative fixture creation failed")); return false; }
	const FGameplayTag Tag = FSovGameplayTags::Get().Status_Apply_Chill;
	USovStatusDefinition* Definition = Fixture.Status->GetStatusDefinition(Tag);
	if (!Definition) { AddError(TEXT("Built-in definition missing")); return false; }
	Definition->EffectClass = USovStatusCheckpointContinuousResourceEffect::StaticClass();
	Fixture.ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 50.f);
	if (!TestEqual(TEXT("Clear-default continuous debuff applies normally"),
		Fixture.Status->ApplyStatusByTag(Tag, Fixture.Actor, 1.f, 20.f), ESovStatusApplicationResult::Applied)) { return false; }
	TestEqual(TEXT("Continuous effect modifies resolved Stamina"), Fixture.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), 40.f);
	FNarrativeSaveComponent Record;
	FSovCombatResourceSnapshot Resources;
	if (!USovEncounterSnapshotLibrary::CaptureComponent(Fixture.Status, Record)
		|| !USovEncounterSnapshotLibrary::CaptureResources(Fixture.ASC, Resources))
	{
		AddError(TEXT("Real component/resource capture failed"));
		return false;
	}
	TestEqual(TEXT("Resources use the explicit base/current schema"), Resources.SchemaVersion, 2);
	TestEqual(TEXT("Capture retains underlying Stamina separately"), Resources.BaseStamina, 50.f);
	TestEqual(TEXT("Capture records the debuffed presentation current"), Resources.Stamina, 40.f);
	TestEqual(TEXT("Clear-default effect is not persisted"), Fixture.Status->CaptureCheckpointState().Statuses.Num(), 0);
	TestTrue(TEXT("Native status record stages its empty state"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Status, Record)
		&& Fixture.Status->WasSaveRecordLoadAccepted());
	TestFalse(TEXT("Old continuous GE leaves before saved resource setters"), Fixture.Status->HasActiveStatus(Tag));
	TestEqual(TEXT("Staging removes the old modifier, exposing unmodified base"), Fixture.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), 50.f);
	TestTrue(TEXT("Production resource equality check succeeds without stale modifier"), USovEncounterSnapshotLibrary::RestoreResources(Fixture.ASC, Resources));
	// V2 restores the saved underlying resource. A clear-default penalty must
	// not be baked permanently into its base after the GE is removed.
	TestEqual(TEXT("Cleared transient penalty leaves the saved base current"), Fixture.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), 50.f);
	TestEqual(TEXT("Resource base remains stable after status cleanup"), Fixture.ASC->GetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute()), 50.f);
	TestFalse(TEXT("Clear-default debuff cannot return at completion"), Fixture.Status->HasActiveStatus(Tag));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointNativeStageOwnershipTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.NativeStageCleanupRejectsRetiredOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointNativeStageOwnershipTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FNarrativeActorFixture Fixture;
	if (!Fixture.Actor || !Fixture.ASC || !Fixture.Status) { AddError(TEXT("Narrative fixture creation failed")); return false; }
	auto* Replacement = Fixture.World->SpawnActor<ASovAxiomRuntimeTestCharacter>();
	if (!Replacement) { AddError(TEXT("Replacement creation failed")); return false; }
	Replacement->InitializeTestCombat(1);
	const FGameplayTag Tag = FSovGameplayTags::Get().Status_Apply_Chill;
	if (Fixture.Status->ApplyStatusByTag(Tag, Fixture.Actor, 1.f, 20.f) != ESovStatusApplicationResult::Applied)
	{
		AddError(TEXT("Live status setup failed"));
		return false;
	}
	FNarrativeSaveComponent Record;
	if (!USovEncounterSnapshotLibrary::CaptureComponent(Fixture.Status, Record)) { AddError(TEXT("Capture failed")); return false; }
	const FDelegateHandle Removed = Fixture.ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[&](const FActiveGameplayEffect&)
		{
			Fixture.ASC->InitAbilityActorInfo(Fixture.Actor, Replacement);
		});
	TestFalse(TEXT("Component helper propagates failed native staging"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Status, Record));
	TestFalse(TEXT("Native staging reports ownership loss during old-effect cleanup"), Fixture.Status->WasSaveRecordLoadAccepted());
	Fixture.ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Removed);
	TestTrue(TEXT("Removal callback genuinely changed actor info"), Fixture.ASC->GetAvatarActor() == Replacement);
	Fixture.AdvanceTimers();
	TestFalse(TEXT("Failed staging cannot write old constraints into the replacement"), Fixture.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_Chilled));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointRetiredResourceBoundaryTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.RetiredResourceBoundaryCannotInstall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointRetiredResourceBoundaryTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FNarrativeActorFixture Fixture;
	if (!Fixture.Actor || !Fixture.ASC || !Fixture.Status) { AddError(TEXT("Narrative fixture creation failed")); return false; }
	auto* Replacement = Fixture.World->SpawnActor<ASovAxiomRuntimeTestCharacter>();
	if (!Replacement) { AddError(TEXT("Replacement creation failed")); return false; }
	Replacement->InitializeTestCombat(1);
	FNarrativeSaveComponent Record;
	if (!SeedPersistentRecord(Fixture.Actor, Fixture.Status, Record)) { AddError(TEXT("Persistent record capture failed")); return false; }
	TestTrue(TEXT("Original actor queues the native record"), USovEncounterSnapshotLibrary::RestoreComponent(Fixture.Status, Record));
	FSovCombatResourceSnapshot Resources;
	if (!USovEncounterSnapshotLibrary::CaptureResources(Fixture.ASC, Resources)) { AddError(TEXT("Resource capture failed")); return false; }
	Resources.Health = 71.f;
	TestTrue(TEXT("Authored currents update explicit snapshot bases"), USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(Fixture.ASC, Resources));
	const FDelegateHandle Changed = Fixture.ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData&)
		{
			Fixture.ASC->InitAbilityActorInfo(Fixture.Actor, Replacement);
		});
	TestFalse(TEXT("Resource callback ownership loss aborts completion"), USovEncounterSnapshotLibrary::RestoreResources(Fixture.ASC, Resources));
	Fixture.ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(Changed);
	Fixture.AdvanceTimers();
	TestFalse(TEXT("Retired component does not install queued state through the rebound ASC"), Fixture.ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Status_Exposed));
	TestEqual(TEXT("Retained semantic data is still available for recovery"), Fixture.Status->CaptureCheckpointState().Statuses.Num(), 1);
	return true;
}
#endif
