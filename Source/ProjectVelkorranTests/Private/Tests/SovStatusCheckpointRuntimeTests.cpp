// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovStatusCheckpointTestFixtures.h"

#include "Effects/SovGameplayEffect_Status.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "NarrativeSave.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Sovereign/SovGameplayTags.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
namespace SovStatusCheckpointTests
{
	struct FWorld
	{
		UWorld* World = nullptr;
		uint64 FixtureFrame = GFrameCounter;
		FWorld()
		{
			// CreateWorld performs the single initialization, matching the UE 5.7 Mac fixtures.
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(false).CreateNavigation(false)
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
		ASovStatusCheckpointTestActor* Actor(const bool bInitialize = true)
		{
			auto* Result = World ? World->SpawnActor<ASovStatusCheckpointTestActor>() : nullptr;
			if (Result && bInitialize) { Result->InitializeCombat(); }
			return Result;
		}
		UNarrativeSaveSubsystem* Save() const
		{
			return World ? World->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
		}
		void AdvanceTimers(const float Seconds)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++FixtureFrame);
			World->GetTimerManager().Tick(Seconds);
		}
		void AdvanceWorldTime(const float Seconds)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++FixtureFrame);
			World->Tick(LEVELTICK_TimeOnly, Seconds);
		}
	};

	/** Author actual persistent definitions before initialization, including the not-yet-ready destination. */
	void ConfigurePersistentDefinitions(ASovStatusCheckpointTestActor& Actor, const bool bResisted = false)
	{
		const auto& Tags = FSovGameplayTags::Get();
		const auto Add = [&Actor](const FName Name, const FGameplayTag Request, const FGameplayTag State,
			const FGameplayTag Cleanse, const ESovStatusCheckpointBehavior Checkpoint, const bool bInfinite)
		{
			auto* Definition = NewObject<USovStatusDefinition>(Actor.Status, Name);
			Definition->RequestTag = Request;
			Definition->StateTag = State;
			Definition->DisplayName = FText::FromName(Name);
			Definition->PresentationTag = Request;
			Definition->AccessibilityPresentationTag = State;
			Definition->CleanseTags.AddTag(Cleanse);
			Definition->UIPriority = 25;
			Definition->DefaultDuration = 20.f;
			Definition->CheckpointBehavior = Checkpoint;
			Definition->DurationPolicy = bInfinite ? ESovStatusDurationPolicy::Infinite : ESovStatusDurationPolicy::Timed;
			Definition->EffectClass = bInfinite ? USovGameplayEffect_StatusInfinite::StaticClass() : USovGameplayEffect_Status::StaticClass();
			Actor.Status->AddDefinitionOverride(Definition);
			return Definition;
		};
		auto* Chill = Add(TEXT("CheckpointChill"), Tags.Status_Apply_Chill, Tags.State_Status_Chilled,
			Tags.Status_Cleanse_Chill, ESovStatusCheckpointBehavior::PersistRemainingDuration, false);
		Chill->MaximumStacks = 3;
		Chill->ReapplyPolicy = ESovStatusReapplyPolicy::AddStack;
		if (bResisted)
		{
			// A real authored matching tag, not a test-only bypass of resistance calculation.
			Chill->ResistanceTags.AddTag(Tags.State_Corruption_Trace);
			Chill->ResistantMagnitudeMultiplier = 0.5f;
			Chill->ResistantDurationMultiplier = 0.5f;
		}
		Add(TEXT("CheckpointExposed"), Tags.Status_Apply_Exposed, Tags.State_Status_Exposed,
			Tags.Status_Cleanse_Exposed, ESovStatusCheckpointBehavior::PersistFullDuration, false);
		Add(TEXT("CheckpointDevice"), Tags.Status_Apply_DeviceDisabled, Tags.State_Status_DeviceDisabled,
			Tags.Status_Cleanse_DeviceDisabled, ESovStatusCheckpointBehavior::PersistRemainingDuration, true);
		Actor.Status->SetDeviceStatusEligible(true);
	}

	bool Apply(ASovStatusCheckpointTestActor& Actor, const FGameplayTag Tag, const float Magnitude = 8.f,
		const float Duration = 20.f, const float EffectLevel = 3.f)
	{
		FSovStatusApplicationRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.StatusTag = Tag;
		Request.SourceActor = &Actor;
		Request.TargetActor = &Actor;
		Request.Magnitude = Magnitude;
		Request.Duration = Duration;
		Request.EffectLevel = EffectLevel;
		Request.SourceAbilityTags.AddTag(FSovGameplayTags::Get().Ability_Echo_Tarrik_CinderStickyGrenade);
		const auto Result = Actor.Status->ApplyStatus(Request);
		return Result == ESovStatusApplicationResult::Applied || Result == ESovStatusApplicationResult::Refreshed;
	}

	const FSovStatusCheckpointRecord* FindRecord(const FSovStatusCheckpointState& State, const FGameplayTag Tag)
	{
		return State.Statuses.FindByPredicate([Tag](const FSovStatusCheckpointRecord& Record) { return Record.RequestTag == Tag; });
	}

	TArray<FActiveGameplayEffectHandle> StatusEffects(const ASovStatusCheckpointTestActor& Actor, const FGameplayTag State)
	{
		TArray<FActiveGameplayEffectHandle> Result;
		for (const FActiveGameplayEffectHandle Handle : Actor.ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const auto* Effect = Actor.ASC->GetActiveGameplayEffect(Handle);
			FGameplayTagContainer Granted;
			if (Effect) { Effect->Spec.GetAllGrantedTags(Granted); }
			if (Granted.HasTagExact(State)) { Result.Add(Handle); }
		}
		return Result;
	}

	bool CaptureRoundTrip(FAutomationTestBase& Test, UNarrativeSaveSubsystem& Save,
		ASovStatusCheckpointTestActor& Actor, FNarrativeActorRecord& OutRecord)
	{
		FNarrativeActorRecord Captured;
		if (!Test.TestTrue(TEXT("Real Narrative actor capture succeeds"), Save.CreateActorRecord(&Actor, Captured))) { return false; }
		TStrongObjectPtr<UNarrativeSave> Snapshot(NewObject<UNarrativeSave>());
		Snapshot->RecordMap.Add(Captured.ActorGUID, Captured);
		TArray<uint8> Bytes;
		if (!Test.TestTrue(TEXT("Narrative save serializes to actual save-game bytes"), UGameplayStatics::SaveGameToMemory(Snapshot.Get(), Bytes))) { return false; }
		TStrongObjectPtr<UNarrativeSave> Decoded(Cast<UNarrativeSave>(UGameplayStatics::LoadGameFromMemory(Bytes)));
		if (!Test.TestNotNull(TEXT("Fresh save object deserializes"), Decoded.Get())) { return false; }
		const auto* Record = Decoded->RecordMap.Find(Captured.ActorGUID);
		if (!Test.TestNotNull(TEXT("Decoded record retains stable identity"), Record)) { return false; }
		OutRecord = *Record;
		const auto* Component = OutRecord.SavedComponents.FindByPredicate([&Actor](const FNarrativeSaveComponent& Saved)
			{ return Saved.ComponentName == Actor.Status->GetFName(); });
		return Test.TestTrue(TEXT("Status participates in the production component save pipeline"), Component && !Component->ByteData.IsEmpty());
	}

	/** Corrupt only the serialized semantic payload, without calling Restore or changing runtime effects. */
	bool ReplaceSerializedStatus(ASovStatusCheckpointTestActor& Actor, FNarrativeActorRecord& Record,
		const FSovStatusCheckpointState& State)
	{
		auto* Property = FindFProperty<FStructProperty>(USovStatusComponent::StaticClass(), TEXT("SavedCheckpointState"));
		auto* Saved = Record.SavedComponents.FindByPredicate([&Actor](const FNarrativeSaveComponent& Component)
			{ return Component.ComponentName == Actor.Status->GetFName(); });
		if (!Property || Property->Struct != FSovStatusCheckpointState::StaticStruct() || !Saved) { return false; }
		*Property->ContainerPtrToValuePtr<FSovStatusCheckpointState>(Actor.Status.Get()) = State;
		Saved->ByteData.Reset();
		FMemoryWriter Writer(Saved->ByteData);
		FObjectAndNameAsStringProxyArchive Archive(Writer, true);
		Archive.ArIsSaveGame = true;
		Archive.ArNoDelta = true;
		Actor.Status->Serialize(Archive);
		return !Archive.IsError();
	}
}

using namespace SovStatusCheckpointTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointDefaultClearTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.DefaultPoliciesRoundTripClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointDefaultClearTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor();
	if (!TestNotNull(TEXT("Actor"), Actor) || !TestNotNull(TEXT("Narrative save"), Fixture.Save())) { return false; }
	const auto& Tags = FSovGameplayTags::Get();
	Actor->Status->SetDeviceStatusEligible(true);
	const FGameplayTag Requests[] = {Tags.Status_Apply_Burn, Tags.Status_Apply_Chill, Tags.Status_Apply_Freeze,
		Tags.Status_Apply_DeviceDisabled, Tags.Status_Apply_Exposed};
	for (const FGameplayTag Tag : Requests) { TestTrue(TEXT("Built-in status applies"), Apply(*Actor, Tag)); }
	TestEqual(TEXT("All five built-in statuses are tracked"), Actor->Status->GetActiveStatusPresentation().Num(), 5);
	TestTrue(TEXT("Default checkpoint includes no persistent statuses"), Actor->Status->CaptureCheckpointState().Statuses.IsEmpty());
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	TestEqual(TEXT("Capture is non-mutating"), Actor->Status->GetActiveStatusPresentation().Num(), 5);
	Actor->SavedMarker = 99;
	TestTrue(TEXT("Real load succeeds"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestEqual(TEXT("Default-valued actor field is restored through the real archive"), Actor->SavedMarker, 17);
	TestTrue(TEXT("Default status snapshot clears the existing runtime set"), Actor->Status->GetActiveStatusPresentation().IsEmpty());
	TestEqual(TEXT("No owned status or recovery-immunity effects remain"), Actor->ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 0);
	TestFalse(TEXT("Checkpoint cleanup does not grant Freeze recovery immunity"), Actor->ASC->HasMatchingGameplayTag(Tags.Status_Immunity_Freeze));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointPersistentRoundTripTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.PersistentPoliciesStacksLevelAndProvenance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointPersistentRoundTripTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!TestNotNull(TEXT("Actor"), Actor) || !TestNotNull(TEXT("Narrative save"), Fixture.Save())) { return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	for (int32 Stack = 0; Stack < 3; ++Stack) { TestTrue(TEXT("Authored Chill stack applies"), Apply(*Actor, Tags.Status_Apply_Chill)); }
	TestTrue(TEXT("Full-duration status applies"), Apply(*Actor, Tags.Status_Apply_Exposed, 4.f, 30.f, 2.f));
	TestTrue(TEXT("Infinite status applies"), Apply(*Actor, Tags.Status_Apply_DeviceDisabled, 2.f, 0.f, 4.f));
	const FSovStatusCheckpointState Captured = Actor->Status->CaptureCheckpointState();
	TestEqual(TEXT("Exactly the opted-in families are captured"), Captured.Statuses.Num(), 3);
	const auto* Full = FindRecord(Captured, Tags.Status_Apply_Exposed);
	const auto* Infinite = FindRecord(Captured, Tags.Status_Apply_DeviceDisabled);
	TestTrue(TEXT("Full duration retains the applied override, not the definition default"), Full && FMath::IsNearlyEqual(Full->RemainingDuration, 30.f));
	TestTrue(TEXT("Infinite policy remains explicit"), Infinite && Infinite->bInfinite && Infinite->RemainingDuration == 0.f);
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	Actor->Status->CleanseStatuses(Tags.Status_Cleanse_All);
	TestTrue(TEXT("Real load restores all persistent policies"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestEqual(TEXT("All Chill stacks restored in one status record"), Actor->Status->GetStatusStackCount(Tags.Status_Apply_Chill), 3);
	TestTrue(TEXT("Full authored duration restored"), FMath::IsNearlyEqual(Actor->Status->GetStatusRemainingDuration(Tags.Status_Apply_Exposed), 30.f, 0.05f));
	TestEqual(TEXT("Infinite duration is not converted to a timed status"), Actor->Status->GetStatusRemainingDuration(Tags.Status_Apply_DeviceDisabled), -1.f);
	const auto Effects = StatusEffects(*Actor, Tags.State_Status_Chilled);
	if (TestEqual(TEXT("Stack restore applies exactly one owned GE"), Effects.Num(), 1))
	{
		const auto* Effect = Actor->ASC->GetActiveGameplayEffect(Effects[0]);
		TestEqual(TEXT("GE effect level survives serialization"), Effect->Spec.GetLevel(), 3.f);
		TestEqual(TEXT("GE aggregate magnitude preserves all stacks"), Effect->Spec.GetSetByCallerMagnitude(Tags.SetByCaller_Status_Magnitude, false), 24.f);
		FGameplayTagContainer AssetTags;
		Effect->Spec.GetAllAssetTags(AssetTags);
		TestTrue(TEXT("Stable ability provenance survives"), AssetTags.HasTagExact(Tags.Ability_Echo_Tarrik_CinderStickyGrenade));
	}
	TestFalse(TEXT("Runtime source actor attribution is not serialized"), Actor->Status->WasStatusAppliedBy(Tags.Status_Apply_Chill, Actor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointElapsedPolicyTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.ElapsedRemainingVersusFullDuration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointElapsedPolicyTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Remaining-duration status applies"), Apply(*Actor, Tags.Status_Apply_Chill, 2.f, 10.f));
	TestTrue(TEXT("Full-duration status applies"), Apply(*Actor, Tags.Status_Apply_Exposed, 2.f, 10.f));
	const float BeforeTime = Fixture.World->GetTimeSeconds();
	Fixture.AdvanceWorldTime(2.f);
	const float Elapsed = Fixture.World->GetTimeSeconds() - BeforeTime;
	if (!TestTrue(TEXT("World-time fixture advances the actual status clock"), Elapsed > 0.f && Elapsed < 10.f)) { return false; }
	const FSovStatusCheckpointState Captured = Actor->Status->CaptureCheckpointState();
	const auto* Remaining = FindRecord(Captured, Tags.Status_Apply_Chill);
	const auto* Full = FindRecord(Captured, Tags.Status_Apply_Exposed);
	TestTrue(TEXT("Remaining policy captures elapsed gameplay time"), Remaining && FMath::IsNearlyEqual(Remaining->RemainingDuration, 10.f - Elapsed, 0.05f));
	TestTrue(TEXT("Full policy does not lose its original duration"), Full && FMath::IsNearlyEqual(Full->RemainingDuration, 10.f, 0.05f));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	Fixture.AdvanceWorldTime(1.f);
	TestTrue(TEXT("Load accepts elapsed policy snapshot"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestTrue(TEXT("Remaining countdown restarts from captured time, not wall-clock save age"),
		FMath::IsNearlyEqual(Actor->Status->GetStatusRemainingDuration(Tags.Status_Apply_Chill), 10.f - Elapsed, 0.05f));
	TestTrue(TEXT("Full countdown restarts at applied duration"),
		FMath::IsNearlyEqual(Actor->Status->GetStatusRemainingDuration(Tags.Status_Apply_Exposed), 10.f, 0.05f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointResistanceTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.EffectiveValuesNotResistedTwice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointResistanceTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor, true);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	Actor->ASC->AddLooseGameplayTag(Tags.State_Corruption_Trace);
	TestTrue(TEXT("Resisted status applies"), Apply(*Actor, Tags.Status_Apply_Chill, 8.f, 20.f));
	const auto Before = Actor->Status->CaptureCheckpointState();
	const auto* BeforeChill = FindRecord(Before, Tags.Status_Apply_Chill);
	if (!TestNotNull(TEXT("Persistent Chill"), BeforeChill)) { return false; }
	TestEqual(TEXT("Fresh application scales magnitude once"), BeforeChill->Magnitude, 4.f);
	TestTrue(TEXT("Fresh application scales duration once"), FMath::IsNearlyEqual(BeforeChill->RemainingDuration, 10.f, 0.05f));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	for (int32 Load = 0; Load < 3; ++Load)
	{
		TestTrue(TEXT("Repeated checkpoint load succeeds"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
		const auto After = Actor->Status->CaptureCheckpointState();
		const auto* AfterChill = FindRecord(After, Tags.Status_Apply_Chill);
		TestTrue(TEXT("Effective magnitude does not halve again"), AfterChill && AfterChill->Magnitude == 4.f);
		TestTrue(TEXT("Effective duration does not halve again"), AfterChill && FMath::IsNearlyEqual(AfterChill->RemainingDuration, 10.f, 0.05f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointRepeatedLoadTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.RepeatedLoadOneEffectOneExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointRepeatedLoadTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	for (int32 Stack = 0; Stack < 3; ++Stack)
	{
		TestTrue(TEXT("Timed status stack applies"), Apply(*Actor, Tags.Status_Apply_Chill, 2.f, 2.f));
	}
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	TStrongObjectPtr<USovStatusCheckpointObserver> Observer(NewObject<USovStatusCheckpointObserver>());
	Actor->Status->OnStatusChanged.AddDynamic(Observer.Get(), &USovStatusCheckpointObserver::OnStatusChanged);
	int32 EffectsApplied = 0;
	const FDelegateHandle Applied = Actor->ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddLambda(
		[&EffectsApplied](UAbilitySystemComponent*, const FGameplayEffectSpec&, FActiveGameplayEffectHandle) { ++EffectsApplied; });
	for (int32 Load = 0; Load < 5; ++Load)
	{
		TestTrue(TEXT("Repeated load accepted"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
		TestEqual(TEXT("One GE remains per status"), StatusEffects(*Actor, Tags.State_Status_Chilled).Num(), 1);
		TestEqual(TEXT("One semantic record remains"), Actor->Status->GetActiveStatusPresentation().Num(), 1);
		TestEqual(TEXT("One GE represents all restored stacks"), Actor->Status->GetStatusStackCount(Tags.Status_Apply_Chill), 3);
	}
	TestEqual(TEXT("One GE application per restore, without a temporary one-stack GE"), EffectsApplied, 5);
	TestEqual(TEXT("One restored notification per load"), Observer->RestoredCount, 5);
	Fixture.AdvanceTimers(3.f);
	Fixture.AdvanceTimers(3.f);
	TestEqual(TEXT("Only current expiry timer can publish expiration"), Observer->ExpiredCount, 1);
	TestFalse(TEXT("Expired status is removed"), Actor->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	TestEqual(TEXT("Expired owned GE removed"), StatusEffects(*Actor, Tags.State_Status_Chilled).Num(), 0);
	Actor->ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(Applied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointQueuedCaptureTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.QueuedBeforeASCReadinessSurvivesRecapture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointQueuedCaptureTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Source = Fixture.Actor(false);
	auto* Destination = Fixture.Actor(false);
	if (!Source || !Destination || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Source);
	ConfigurePersistentDefinitions(*Destination);
	Source->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Persistent source status applies"), Apply(*Source, Tags.Status_Apply_Chill, 6.f, 12.f, 4.f));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Source, Record)) { return false; }
	TestFalse(TEXT("Destination status is not initialized"), Destination->Status->IsInitialized());
	TestTrue(TEXT("Production load accepts a valid deferred snapshot"), Fixture.Save()->LoadActorFromRecord(Destination, Record));
	TestFalse(TEXT("Load does not fabricate ASC readiness"), Destination->Status->IsInitialized());
	TestEqual(TEXT("No effect is applied before actor info exists"), Destination->ASC->GetActiveEffects(FGameplayEffectQuery()).Num(), 0);
	const auto Pending = Destination->Status->CaptureCheckpointState();
	const auto* Chill = FindRecord(Pending, Tags.Status_Apply_Chill);
	TestTrue(TEXT("Capture retains accepted pending status magnitude/level"), Chill && Chill->Magnitude == 6.f && Chill->EffectLevel == 4.f);
	FNarrativeActorRecord Recaptured;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Destination, Recaptured)) { return false; }
	TestTrue(TEXT("A second pre-readiness load retains the queued state"), Fixture.Save()->LoadActorFromRecord(Destination, Recaptured));
	auto* QueuedDefinition = Destination->Status->GetStatusDefinition(Tags.Status_Apply_Chill);
	if (!TestNotNull(TEXT("Queued restore resolves the authored destination definition"), QueuedDefinition)) { return false; }
	++QueuedDefinition->SchemaVersion;
	FNarrativeActorRecord LastKnownGood;
	LastKnownGood.ActorName = TEXT("LastKnownGoodRecord");
	TestFalse(TEXT("An incompatible queued snapshot cannot replace a good save record"), Fixture.Save()->CreateActorRecord(Destination, LastKnownGood));
	TestEqual(TEXT("Rejected queued capture preserves prior output"), LastKnownGood.ActorName, FName(TEXT("LastKnownGoodRecord")));
	TestEqual(TEXT("Rejected queued capture does not erase pending status"), Destination->Status->CaptureCheckpointState().Statuses.Num(), 1);
	--QueuedDefinition->SchemaVersion;
	Destination->InitializeCombat();
	TestTrue(TEXT("Correct owner readiness applies queued state"), Destination->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	TestEqual(TEXT("Queued restore produces one GE"), StatusEffects(*Destination, Tags.State_Status_Chilled).Num(), 1);
	TestTrue(TEXT("Repeated initialization is harmless"), Destination->Status->InitializeWithAbilitySystem(Destination->ASC));
	TestEqual(TEXT("Repeated readiness does not duplicate GE"), StatusEffects(*Destination, Tags.State_Status_Chilled).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointMalformedPreflightTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.MalformedPayloadRejectedBeforeMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointMalformedPreflightTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Live status applies"), Apply(*Actor, Tags.Status_Apply_Chill));
	FNarrativeActorRecord GoodRecord;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, GoodRecord)) { return false; }
	const FSovStatusCheckpointState GoodState = Actor->Status->CaptureCheckpointState();
	const auto OriginalEffects = StatusEffects(*Actor, Tags.State_Status_Chilled);
	if (!TestEqual(TEXT("Initial effect"), OriginalEffects.Num(), 1)) { return false; }
	struct FBadCase { const TCHAR* Name; TFunction<void(FSovStatusCheckpointState&)> Mutate; };
	const FBadCase Cases[] = {
		{TEXT("Unsupported schema"), [](auto& S) { ++S.SchemaVersion; }},
		{TEXT("Duplicate request family"), [](auto& S) { S.Statuses.Add(S.Statuses[0]); }},
		{TEXT("Oversized semantic array"), [](auto& S) { const auto Record = S.Statuses[0]; S.Statuses.Init(Record, 65); }},
		{TEXT("Nonfinite magnitude"), [](auto& S) { S.Statuses[0].Magnitude = std::numeric_limits<float>::infinity(); }},
		{TEXT("Finite magnitude overflows with stacks"), [](auto& S) { S.Statuses[0].Magnitude = std::numeric_limits<float>::max(); S.Statuses[0].StackCount = 3; }},
		{TEXT("Negative magnitude"), [](auto& S) { S.Statuses[0].Magnitude = -1.f; }},
		{TEXT("Nonfinite duration"), [](auto& S) { S.Statuses[0].RemainingDuration = std::numeric_limits<float>::quiet_NaN(); }},
		{TEXT("Negative duration"), [](auto& S) { S.Statuses[0].RemainingDuration = -1.f; }},
		{TEXT("Nonfinite effect level"), [](auto& S) { S.Statuses[0].EffectLevel = std::numeric_limits<float>::infinity(); }},
		{TEXT("Zero effect level"), [](auto& S) { S.Statuses[0].EffectLevel = 0.f; }},
		{TEXT("Zero stack count"), [](auto& S) { S.Statuses[0].StackCount = 0; }},
		{TEXT("Stack count above authored maximum"), [](auto& S) { S.Statuses[0].StackCount = 4; }},
		{TEXT("Definition identity mismatch"), [](auto& S) { S.Statuses[0].DefinitionId = FPrimaryAssetId(FPrimaryAssetType(FName(TEXT("SovStatusDefinition"))), FName(TEXT("OtherDefinition"))); }},
		{TEXT("Definition schema mismatch"), [](auto& S) { ++S.Statuses[0].DefinitionSchemaVersion; }},
		{TEXT("Unknown request family"), [&Tags](auto& S) { S.Statuses[0].RequestTag = Tags.Status_Apply_Corruption; }},
		{TEXT("State alias cannot masquerade as request family"), [&Tags](auto& S) { S.Statuses[0].RequestTag = Tags.State_Status_Chilled; }},
		{TEXT("Non-ability source provenance"), [&Tags](auto& S) { S.Statuses[0].SourceAbilityTags.AddTag(Tags.State_Corruption_Trace); }},
		{TEXT("Infinite policy mismatch"), [](auto& S) { S.Statuses[0].bInfinite = true; }}
	};
	for (const FBadCase& Case : Cases)
	{
		FSovStatusCheckpointState BadState = GoodState;
		Case.Mutate(BadState);
		FNarrativeActorRecord BadRecord = GoodRecord;
		if (!TestTrue(TEXT("Malformed state encoded through real component proxy archive"), ReplaceSerializedStatus(*Actor, BadRecord, BadState))) { return false; }
		Actor->SavedMarker = 99;
		const int32 BeforeLoads = Actor->ActorLoadCalls;
		TestFalse(Case.Name, Fixture.Save()->LoadActorFromRecord(Actor, BadRecord));
		TestEqual(TEXT("Preflight rejection preserves actor bytes"), Actor->SavedMarker, 99);
		TestEqual(TEXT("Preflight rejection prevents actor Load callback"), Actor->ActorLoadCalls, BeforeLoads);
		const auto Remaining = StatusEffects(*Actor, Tags.State_Status_Chilled);
		TestTrue(TEXT("Malformed record cannot clear or replace the live GE"), Remaining.Num() == 1 && Remaining[0] == OriginalEffects[0]);
		TestTrue(TEXT("Malformed record leaves status tracked"), Actor->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	}
	for (int32 Corruption = 0; Corruption < 4; ++Corruption)
	{
		FNarrativeActorRecord BadRecord = GoodRecord;
		auto* Saved = BadRecord.SavedComponents.FindByPredicate([Actor](const FNarrativeSaveComponent& C)
			{ return C.ComponentName == Actor->Status->GetFName(); });
		if (!TestNotNull(TEXT("Status byte payload exists"), Saved)) { return false; }
		if (Corruption == 0) { Saved->ByteData.Reset(); }
		else if (Corruption == 1) { Saved->ByteData.Add(0x7f); }
		else if (Corruption == 2) { Saved->ByteData.SetNumZeroed(256 * 1024 + 1); }
		else
		{
			// Present component bytes without a semantic state field are malformed,
			// unlike an entirely absent legacy component record.
			TStrongObjectPtr<UActorComponent> WrongPayload(NewObject<UActorComponent>());
			Saved->ByteData.Reset();
			FMemoryWriter Writer(Saved->ByteData);
			FObjectAndNameAsStringProxyArchive Archive(Writer, true);
			Archive.ArIsSaveGame = true;
			Archive.ArNoDelta = true;
			WrongPayload->Serialize(Archive);
			if (!TestFalse(TEXT("Fixture archive serializes without error"), Archive.IsError())) { return false; }
		}
		Actor->SavedMarker = 99;
		TestFalse(TEXT("Empty, trailing-byte, oversized and missing-semantic-field payloads fail native preflight"), Fixture.Save()->LoadActorFromRecord(Actor, BadRecord));
		TestEqual(TEXT("Invalid archive cannot change actor bytes"), Actor->SavedMarker, 99);
		const auto Remaining = StatusEffects(*Actor, Tags.State_Status_Chilled);
		TestTrue(TEXT("Invalid archive cannot clear the live effect"), Remaining.Num() == 1 && Remaining[0] == OriginalEffects[0]);
	}
	TestTrue(TEXT("Last known good record remains loadable after rejected attempts"), Fixture.Save()->LoadActorFromRecord(Actor, GoodRecord));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointLegacyMissingRecordTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.LegacyMissingComponentClearsRuntimeAndPending",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointLegacyMissingRecordTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	auto* Destination = Fixture.Actor(false);
	if (!Actor || !Destination || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	ConfigurePersistentDefinitions(*Destination);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Persistent status applies"), Apply(*Actor, Tags.Status_Apply_Chill));
	FNarrativeActorRecord CurrentRecord;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, CurrentRecord)) { return false; }
	FNarrativeActorRecord LegacyRecord = CurrentRecord;
	LegacyRecord.SavedComponents.RemoveAll([Actor](const FNarrativeSaveComponent& C) { return C.ComponentName == Actor->Status->GetFName(); });
	TestTrue(TEXT("Legacy record remains loadable"), Fixture.Save()->LoadActorFromRecord(Actor, LegacyRecord));
	TestFalse(TEXT("Absence means clear, not stale runtime status reuse"), Actor->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	TestTrue(TEXT("Current snapshot queues on unready destination"), Fixture.Save()->LoadActorFromRecord(Destination, CurrentRecord));
	TestEqual(TEXT("A status is pending"), Destination->Status->CaptureCheckpointState().Statuses.Num(), 1);
	TestTrue(TEXT("Legacy snapshot can supersede a pending current snapshot"), Fixture.Save()->LoadActorFromRecord(Destination, LegacyRecord));
	TestTrue(TEXT("Legacy missing component clears pending state"), Destination->Status->CaptureCheckpointState().Statuses.IsEmpty());
	Destination->InitializeCombat();
	TestFalse(TEXT("Old queued effect cannot appear after readiness"), Destination->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointUnsafeEffectAuthoringTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.UnsafePersistentEffectAuthoringFailsBeforeClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointUnsafeEffectAuthoringTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Status applies before capture"), Apply(*Actor, Tags.Status_Apply_Chill));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	const auto BeforeEffects = StatusEffects(*Actor, Tags.State_Status_Chilled);
	if (!TestEqual(TEXT("One live effect"), BeforeEffects.Num(), 1)) { return false; }
	auto* Definition = Actor->Status->GetStatusDefinition(Tags.Status_Apply_Chill);
	if (!TestNotNull(TEXT("Authored definition"), Definition)) { return false; }
	const TSubclassOf<UGameplayEffect> OriginalClass = Definition->EffectClass;
	const TSubclassOf<UGameplayEffect> UnsafeClasses[] = {
		USovStatusCheckpointUnsafePeriodicEffect::StaticClass(),
		USovStatusCheckpointAggregateEffect::StaticClass(),
		USovStatusCheckpointContinuousResourceEffect::StaticClass(),
		USovGameplayEffect_StatusInfinite::StaticClass()
	};
	for (const auto UnsafeClass : UnsafeClasses)
	{
		Definition->EffectClass = UnsafeClass;
		Actor->SavedMarker = 99;
		TestFalse(TEXT("Periodic execute-on-load, aggregating, continuous resource or mismatched-duration GE rejects before commit"),
			Fixture.Save()->LoadActorFromRecord(Actor, Record));
		FNarrativeActorRecord PreviousRecord;
		PreviousRecord.ActorName = TEXT("LastKnownGoodRecord");
		TestFalse(TEXT("Unsafe authoring cannot create a new invalid save"), Fixture.Save()->CreateActorRecord(Actor, PreviousRecord));
		TestEqual(TEXT("Rejected unsafe capture preserves caller's prior record"), PreviousRecord.ActorName, FName(TEXT("LastKnownGoodRecord")));
		TestEqual(TEXT("Unsafe definition cannot modify actor record"), Actor->SavedMarker, 99);
		const auto AfterEffects = StatusEffects(*Actor, Tags.State_Status_Chilled);
		TestTrue(TEXT("Unsafe definition cannot clear existing owned effect"), AfterEffects.Num() == 1 && AfterEffects[0] == BeforeEffects[0]);
	}
	Definition->EffectClass = OriginalClass;
	TestTrue(TEXT("Corrected safe definition can load original record"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointSafePeriodicTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.SafePeriodicEffectDoesNotExecuteOnRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointSafePeriodicTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	auto* Definition = Actor->Status->GetStatusDefinition(Tags.Status_Apply_Chill);
	if (!TestNotNull(TEXT("Authored definition"), Definition)) { return false; }
	Definition->EffectClass = USovStatusCheckpointSafePeriodicEffect::StaticClass();
	Definition->Period = 1.f;
	TestTrue(TEXT("Safe periodic status applies"), Apply(*Actor, Tags.Status_Apply_Chill, 1.f, 20.f));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	const float HealthBefore = Actor->ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	for (int32 Load = 0; Load < 5; ++Load)
	{
		TestTrue(TEXT("Safe periodic payload restores"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
		TestEqual(TEXT("Loading cannot execute an extra periodic tick"),
			Actor->ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), HealthBefore);
		TestEqual(TEXT("Only one periodic GE survives repeated load"), StatusEffects(*Actor, Tags.State_Status_Chilled).Num(), 1);
	}
	Fixture.AdvanceTimers(1.1f);
	TestTrue(TEXT("Periodic fixture really executes once its normal timer is due"),
		Actor->ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) < HealthBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointAvatarReplacementTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.AvatarReplacementDuringClearFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointAvatarReplacementTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	auto* Replacement = Fixture.Actor();
	if (!Actor || !Replacement || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Status applies before capture"), Apply(*Actor, Tags.Status_Apply_Chill));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	bool bReplaced = false;
	const FDelegateHandle Removal = Actor->ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[Actor, Replacement, &bReplaced](const FActiveGameplayEffect&)
		{
			if (!bReplaced)
			{
				bReplaced = true;
				Actor->ASC->ClearActorInfo();
				Actor->ASC->InitAbilityActorInfo(Actor, Replacement);
			}
		});
	TestFalse(TEXT("Load reports loss of the owning avatar"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestTrue(TEXT("Actual GE removal callback replaced avatar"), bReplaced);
	TestTrue(TEXT("ASC now points to the replacement"), Actor->ASC->GetAvatarActor() == Replacement);
	TestEqual(TEXT("Retired status owner cannot apply into reassigned ASC"), StatusEffects(*Actor, Tags.State_Status_Chilled).Num(), 0);
	TestFalse(TEXT("Replacement actor does not inherit the retired snapshot"), Replacement->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	Actor->ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Removal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointRestoredCallbackOwnershipTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.RestoredPublicationStopsOnAvatarReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointRestoredCallbackOwnershipTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	auto* Replacement = Fixture.Actor();
	if (!Actor || !Replacement || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("First status applies"), Apply(*Actor, Tags.Status_Apply_Chill));
	TestTrue(TEXT("Second status applies"), Apply(*Actor, Tags.Status_Apply_Exposed));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	TStrongObjectPtr<USovStatusCheckpointObserver> Observer(NewObject<USovStatusCheckpointObserver>());
	Observer->ASC = Actor->ASC;
	Observer->OwnerActor = Actor;
	Observer->ReplacementAvatar = Replacement;
	Observer->bReplaceOnFirstRestore = true;
	Actor->Status->OnStatusChanged.AddDynamic(Observer.Get(), &USovStatusCheckpointObserver::OnStatusChanged);
	TestFalse(TEXT("Post-commit publication ownership loss is reported"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestEqual(TEXT("No further restored callback reaches the replacement avatar"), Observer->RestoredCount, 1);
	TestTrue(TEXT("Restored callback genuinely rebound the ASC"), Actor->ASC->GetAvatarActor() == Replacement);
	TestEqual(TEXT("Rollback removes Chill from the reassigned original ASC"), StatusEffects(*Actor, Tags.State_Status_Chilled).Num(), 0);
	TestEqual(TEXT("Rollback removes Exposed from the reassigned original ASC"), StatusEffects(*Actor, Tags.State_Status_Exposed).Num(), 0);
	TestTrue(TEXT("Retired component has no stale restored presentation"), Actor->Status->GetActiveStatusPresentation().IsEmpty());
	TestFalse(TEXT("Replacement component retains its own status state"), Replacement->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointNestedMutationTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.NestedRestoreAndStatusMutationRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointNestedMutationTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor(false);
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	ConfigurePersistentDefinitions(*Actor);
	Actor->InitializeCombat();
	const auto& Tags = FSovGameplayTags::Get();
	TestTrue(TEXT("Status applies before capture"), Apply(*Actor, Tags.Status_Apply_Chill));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	bool bCallbackRan = false, bNestedRestore = true, bNestedApply = true, bNestedRemove = true, bNestedCapture = true;
	int32 NestedCleanse = -1;
	FSovStatusCheckpointState CapturedDuringClear;
	FNarrativeActorRecord PreservedRecord;
	PreservedRecord.ActorName = TEXT("LastKnownGoodRecord");
	const FDelegateHandle Removal = Actor->ASC->OnAnyGameplayEffectRemovedDelegate().AddLambda(
		[Actor, &Fixture, &Tags, &bCallbackRan, &bNestedRestore, &bNestedApply, &bNestedRemove,
			&bNestedCapture, &NestedCleanse, &CapturedDuringClear, &PreservedRecord](const FActiveGameplayEffect&)
		{
			if (bCallbackRan) { return; }
			bCallbackRan = true;
			CapturedDuringClear = Actor->Status->CaptureCheckpointState();
			bNestedCapture = Fixture.Save()->CreateActorRecord(Actor, PreservedRecord);
			bNestedRestore = Actor->Status->RestoreCheckpointState(FSovStatusCheckpointState());
			bNestedApply = Apply(*Actor, Tags.Status_Apply_Exposed);
			bNestedRemove = Actor->Status->RemoveStatus(Tags.Status_Apply_Chill, false);
			NestedCleanse = Actor->Status->CleanseStatuses(Tags.Status_Cleanse_All);
		});
	TestTrue(TEXT("Outer restore owns the transaction"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestTrue(TEXT("Real effect callback executed"), bCallbackRan);
	TestFalse(TEXT("Nested restore cannot supersede the active transaction"), bNestedRestore);
	TestFalse(TEXT("Nested apply cannot add unrelated status during restore"), bNestedApply);
	TestFalse(TEXT("Nested removal cannot invalidate the active restore"), bNestedRemove);
	TestEqual(TEXT("Nested cleanse is rejected"), NestedCleanse, 0);
	const auto* CapturedChill = FindRecord(CapturedDuringClear, Tags.Status_Apply_Chill);
	TestTrue(TEXT("Semantic capture sees the full accepted snapshot, not the half-cleared map"),
		CapturedDuringClear.Statuses.Num() == 1 && CapturedChill && CapturedChill->Magnitude == 8.f);
	TestFalse(TEXT("Native save capture refuses to commit during restore mutation"), bNestedCapture);
	TestEqual(TEXT("Failed nested capture preserves caller's last good record"), PreservedRecord.ActorName, FName(TEXT("LastKnownGoodRecord")));
	TestTrue(TEXT("Outer snapshot remains authoritative"), Actor->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	TestFalse(TEXT("Rejected nested status did not leak"), Actor->Status->HasActiveStatus(Tags.Status_Apply_Exposed));
	Actor->ASC->OnAnyGameplayEffectRemovedDelegate().Remove(Removal);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStatusCheckpointUnrelatedEffectsTest,
	"ProjectVelkorran.Campaign.Status.Checkpoint.UnrelatedCorruptionOwnerEffectPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStatusCheckpointUnrelatedEffectsTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard ScriptGuard;
	FWorld Fixture;
	auto* Actor = Fixture.Actor();
	if (!Actor || !Fixture.Save()) { AddError(TEXT("Fixture creation failed")); return false; }
	const auto& Tags = FSovGameplayTags::Get();
	// Stand in for another owner's exact handle, granting the canonical corruption band tag.
	// This intentionally does not assert campaign corruption persistence, which has its own suite.
	FGameplayEffectSpec OtherOwner(GetDefault<USovGameplayEffect_StatusInfinite>(), Actor->ASC->MakeEffectContext(), 1.f);
	OtherOwner.DynamicGrantedTags.AddTag(Tags.State_Corruption_Trace);
	const auto OtherHandle = Actor->ASC->ApplyGameplayEffectSpecToSelf(OtherOwner);
	if (!TestTrue(TEXT("Unrelated owner GE is live"), OtherHandle.IsValid())) { return false; }
	TestTrue(TEXT("Clear-on-checkpoint status applies"), Apply(*Actor, Tags.Status_Apply_Chill));
	FNarrativeActorRecord Record;
	if (!CaptureRoundTrip(*this, *Fixture.Save(), *Actor, Record)) { return false; }
	TestTrue(TEXT("Status load succeeds"), Fixture.Save()->LoadActorFromRecord(Actor, Record));
	TestFalse(TEXT("Status component clears only its own status"), Actor->Status->HasActiveStatus(Tags.Status_Apply_Chill));
	TestNotNull(TEXT("Exact unrelated corruption GE survives"), Actor->ASC->GetActiveGameplayEffect(OtherHandle));
	TestTrue(TEXT("Canonical corruption band tag remains"), Actor->ASC->HasMatchingGameplayTag(Tags.State_Corruption_Trace));
	TestTrue(TEXT("Generic status snapshot never serializes canonical corruption"), Actor->Status->CaptureCheckpointState().Statuses.IsEmpty());
	return true;
}
#endif
