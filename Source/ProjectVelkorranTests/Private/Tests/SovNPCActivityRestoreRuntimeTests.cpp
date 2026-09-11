// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNPCActivityRestoreTestFixtures.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/Script.h"

float USovNPCActivityRestoreTestActivity::ScoreActivity_Implementation(const FNPCGoalContainer& Container,
	UNPCGoalItem*& BestGoal, TArray<UNPCGoalItem*>& InvalidGoals)
{
	if (auto* Component = Cast<USovNPCActivityRestoreTestComponent>(OwnerActivityComponent))
	{
		const TFunction<void()> Callback = Component->DuringActivityScore;
		if (Callback) { Callback(); }
	}
	return -1.f;
}

void USovNPCActivityRestoreTestGenerator::InitializeGoalGenerator_Implementation()
{
	if (auto* Component = Cast<USovNPCActivityRestoreTestComponent>(OwnerActivityComponent))
	{ Component->RecordInitialization(true, SavedValue, OwnerController); }
}

void USovNPCActivityRestoreTestGoal::Initialize_Implementation()
{
	if (auto* Component = Cast<USovNPCActivityRestoreTestComponent>(GetOuter()))
	{ Component->RecordInitialization(false, SavedValue, OwnerController); }
}

void USovNPCActivityRestoreTestComponent::RecordInitialization(bool bGenerator, int32 Value, ANarrativeNPCController* Controller)
{
	bAllCallbacksHadRealInitializedOwner &= IsValid(Controller) && Controller == GetOwner()
		&& Controller->HasActorBegunPlay() && HasBegunPlay() && IsRegistered()
		&& Controller->GetActivityComponent() == this && Controller->GetControlledNPC() != nullptr;
	(bGenerator ? GeneratorInitializations : GoalInitializations).Add(Value);
	// Copy the callable: a reentrant load or retirement may replace its owner.
	const TFunction<void()> Callback = bGenerator ? DuringGeneratorInitialization : DuringGoalInitialization;
	if (Callback) { Callback(); }
}

void USovNPCActivityRestoreTestComponent::SeedSavedValues(int32 Value, bool bIncludeGenerator)
{
	const auto Serialize = [](UObject* Object, TArray<uint8>& Bytes)
	{
		FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Ar(Writer, true);
		Ar.ArIsSaveGame = true; Object->Serialize(Ar);
	};
	SavedActivities.Reset(); SavedGoalGenerators.Reset(); SavedGoals.Reset();
	auto* Activity = NewObject<USovNPCActivityRestoreTestActivity>(this); Activity->SavedValue = Value;
	auto& ActivityRecord = SavedActivities.AddDefaulted_GetRef(); ActivityRecord.Class = Activity->GetClass();
	Serialize(Activity, ActivityRecord.Data);
	if (bIncludeGenerator)
	{
		auto* Generator = NewObject<USovNPCActivityRestoreTestGenerator>(this); Generator->SavedValue = Value;
		auto& GeneratorRecord = SavedGoalGenerators.AddDefaulted_GetRef(); GeneratorRecord.Class = Generator->GetClass();
		Serialize(Generator, GeneratorRecord.Data);
	}
	auto* Goal = NewObject<USovNPCActivityRestoreTestGoal>(this); Goal->SavedValue = Value;
	Goal->bSaveGoal = true; Goal->CreationTime = 17.f; Goal->TODCreationTime = 800.f;
	Goal->IntendedTODStartTime = 730.f; Goal->GoalLifetime = 120.f;
	auto& GoalRecord = SavedGoals.AddDefaulted_GetRef(); GoalRecord.Class = Goal->GetClass();
	Serialize(Goal, GoalRecord.Data);
}

#if WITH_AUTOMATION_TESTS
namespace SovNPCActivityRestoreTests
{
	struct FWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		uint64 TimerFrame = GFrameCounter;
		FWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World) { World->InitializeActorsForPlay(FURL()); }
		}
		~FWorld() { if (World) { World->DestroyWorld(false); } }
		ASovNPCActivityRestoreTestController* SpawnController()
		{
			FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Pawn = World->SpawnActor<ASovNPCActivityRestoreTestPawn>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			auto* Controller = World->SpawnActor<ASovNPCActivityRestoreTestController>();
			if (Controller && Pawn) { Controller->Possess(Pawn); }
			return Controller;
		}
		void Begin(ASovNPCActivityRestoreTestController* Controller)
		{
			if (Controller->GetPawn() && !Controller->GetPawn()->HasActorBegunPlay()) { Controller->GetPawn()->DispatchBeginPlay(); }
			Controller->DispatchBeginPlay();
		}
		void Tick()
		{
			// Native TimerManager executes the deferred next-tick restore. No direct
			// restoration helper or owner-cache fixture call is used.
			TGuardValue<uint64> Frame(GFrameCounter, ++TimerFrame);
			World->GetTimerManager().Tick(.05f);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCActivityPreBeginPlayRestoreTest,
	"ProjectVelkorran.Campaign.NPCActivityRestore.PreBeginPlayActorRecordPreservesState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCActivityPreBeginPlayRestoreTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCActivityRestoreTests;
	FWorld Scope; if (!TestNotNull(TEXT("Native world"), Scope.World)) { return false; }
	auto* Source = Scope.SpawnController(); auto* Destination = Scope.SpawnController();
	if (!TestNotNull(TEXT("Source controller"), Source) || !TestNotNull(TEXT("Loaded controller"), Destination)) { return false; }
	const FGuid SourceGuid = INarrativeStableActor::Execute_GetActorGUID(Source);
	const FGuid DestinationGuid = INarrativeStableActor::Execute_GetActorGUID(Destination);
	const FGuid SourcePawnGuid = INarrativeStableActor::Execute_GetActorGUID(Source->GetPawn());
	const FGuid DestinationPawnGuid = INarrativeStableActor::Execute_GetActorGUID(Destination->GetPawn());
	TestTrue(TEXT("Actual savable fixture actors have valid distinct construction identities"),
		SourceGuid.IsValid() && DestinationGuid.IsValid() && SourcePawnGuid.IsValid() && DestinationPawnGuid.IsValid()
		&& TSet<FGuid>{SourceGuid, DestinationGuid, SourcePawnGuid, DestinationPawnGuid}.Num() == 4);
	auto* SourceComponent = Source->TestActivities(); auto* Component = Destination->TestActivities();
	SourceComponent->SeedSavedValues(41);
	INarrativeSavableComponent::Execute_Load(SourceComponent);
	Scope.Begin(Source); Scope.Tick();
	if (!TestFalse(TEXT("Source snapshot materialized through real BeginPlay"), SourceComponent->IsRestorePending())) { return false; }
	auto* Save = Scope.World->GetSubsystem<UNarrativeSaveSubsystem>();
	FNarrativeActorRecord Record;
	if (!TestNotNull(TEXT("Actual Narrative world save subsystem"), Save)
		|| !TestTrue(TEXT("Create real controller record including activity component"), Save->CreateActorRecord(Source, Record))) { return false; }
	TestFalse(TEXT("Destination controller has not begun play"), Destination->HasActorBegunPlay());
	TestFalse(TEXT("Destination component has no invented owner cache"), Component->HasCachedOwner());
	TestEqual(TEXT("Actual controller record retains its stable identity"), Record.ActorGUID, SourceGuid);
	TestTrue(TEXT("Actual pre-BeginPlay LoadActorFromRecord accepts deferred initialization"), Save->LoadActorFromRecord(Destination, Record));
	TestEqual(TEXT("Actual save loader restores the destination controller's saved identity"),
		INarrativeStableActor::Execute_GetActorGUID(Destination), SourceGuid);
	TestEqual(TEXT("Controller identity restore does not replace its possessed pawn's identity"),
		INarrativeStableActor::Execute_GetActorGUID(Destination->GetPawn()), DestinationPawnGuid);
	TestTrue(TEXT("Saved record accepted while genuinely waiting for startup"), Component->WasSaveRecordLoadAccepted() && Component->IsRestorePending());
	TestEqual(TEXT("No early generator callback"), Component->GeneratorInitializations.Num(), 0);
	TestNull(TEXT("No ownerless activity created"), Component->GetActivity(USovNPCActivityRestoreTestActivity::StaticClass()));
	const auto PendingGoalBytes = Component->FirstSavedGoalBytes();
	INarrativeSavableComponent::Execute_PrepareForSave(Component);
	TestTrue(TEXT("Preparing during deferred load preserves exact saved goal bytes"), PendingGoalBytes == Component->FirstSavedGoalBytes());
	TestEqual(TEXT("Pending activity row retained"), Component->NumSavedActivities(), 1);
	TestEqual(TEXT("Pending generator row retained"), Component->NumSavedGenerators(), 1);
	Scope.Begin(Destination);
	TestEqual(TEXT("Component BeginPlay does not initialize before controller BeginPlay completes"), Component->GeneratorInitializations.Num(), 0);
	Scope.Tick();
	TestFalse(TEXT("Deferred restore completed"), Component->IsRestorePending());
	TestTrue(TEXT("Every callback observed actual initialized controller and possessed NPC"), Component->bAllCallbacksHadRealInitializedOwner);
	auto* Activity = Cast<USovNPCActivityRestoreTestActivity>(Component->GetActivity(USovNPCActivityRestoreTestActivity::StaticClass()));
	if (!TestNotNull(TEXT("Restored activity"), Activity)) { return false; }
	TestTrue(TEXT("Real native activity owns the correct controller and component"), Activity->HasRealOwner(Destination));
	TestEqual(TEXT("Activity saved value restored"), Activity->SavedValue, 41);
	TestTrue(TEXT("Generator initialized once with saved value, not class defaults"), Component->GeneratorInitializations == TArray<int32>{41});
	const auto Goals = Component->GetGoals(USovNPCActivityRestoreTestGoal::StaticClass());
	if (!TestEqual(TEXT("One actual restored goal"), Goals.Goals.Num(), 1)) { return false; }
	TestEqual(TEXT("Saved goal creation time is retained"), Goals.Goals[0]->CreationTime, 17.f);
	TestEqual(TEXT("Saved goal TOD is retained"), Goals.Goals[0]->TODCreationTime, 800.f);
	TestEqual(TEXT("Saved goal intended start is retained"), Goals.Goals[0]->IntendedTODStartTime, 730.f);
	TestEqual(TEXT("Saved goal lifetime is retained"), Goals.Goals[0]->GoalLifetime, 120.f);
	INarrativeSavableComponent::Execute_PrepareForSave(Component);
	INarrativeSavableComponent::Execute_PrepareForSave(Component);
	TestEqual(TEXT("Repeated saves do not append duplicate generator rows"), Component->NumSavedGenerators(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCActivityRestoreExistingAndReentryTest,
	"ProjectVelkorran.Campaign.NPCActivityRestore.ExistingDefaultsAndNewerLoadOwnPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCActivityRestoreExistingAndReentryTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCActivityRestoreTests;
	FWorld Scope; auto* Controller = Scope.SpawnController();
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	Scope.Begin(Controller); auto* Component = Controller->TestActivities();
	auto* Existing = Cast<USovNPCActivityRestoreTestActivity>(Component->AddActivity(USovNPCActivityRestoreTestActivity::StaticClass(), true));
	if (!TestNotNull(TEXT("Default activity already initialized normally"), Existing)) { return false; }
	Existing->SavedValue = 2;
	Component->SeedSavedValues(51);
	Component->DuringGeneratorInitialization = [Component]()
	{
		Component->DuringGeneratorInitialization = nullptr;
		Component->SeedSavedValues(73);
		INarrativeSavableComponent::Execute_Load(Component);
	};
	INarrativeSavableComponent::Execute_Load(Component);
	TestTrue(TEXT("Existing activity receives first serialized value"), Existing->SavedValue == 51);
	TestEqual(TEXT("Retired restore cannot initialize or publish its saved goal"), Component->GoalInitializations.Num(), 0);
	TestTrue(TEXT("Newer load is deferred until old callback stack retires"), Component->IsRestorePending());
	Scope.Tick();
	TestFalse(TEXT("Latest restore completes"), Component->IsRestorePending());
	TestTrue(TEXT("Saved data applied to existing activity instead of silently skipped"), Existing->SavedValue == 73
		&& Existing == Component->GetActivity(USovNPCActivityRestoreTestActivity::StaticClass()));
	auto* Generator = Cast<USovNPCActivityRestoreTestGenerator>(Component->GetGoalGenerator(USovNPCActivityRestoreTestGenerator::StaticClass()));
	TestTrue(TEXT("Single initialized generator receives latest saved settings"), Generator && Generator->SavedValue == 73
		&& Component->GeneratorInitializations == TArray<int32>{51});
	TestTrue(TEXT("Only newest saved goal initializes"), Component->GoalInitializations == TArray<int32>{73});
	const auto Goals = Component->GetGoals(USovNPCActivityRestoreTestGoal::StaticClass());
	TestEqual(TEXT("Only newest saved goal published"), Goals.Goals.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCActivityRestorePossessionRetirementTest,
	"ProjectVelkorran.Campaign.NPCActivityRestore.PossessionEpochAndEndPlayRetireCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCActivityRestorePossessionRetirementTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCActivityRestoreTests;
	FWorld Scope; auto* Controller = Scope.SpawnController();
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	auto* Component = Controller->TestActivities(); Component->SeedSavedValues(91);
	INarrativeSavableComponent::Execute_Load(Component);
	APawn* Pawn = Controller->GetPawn(); const uint64 Assignment = Controller->GetPawnAssignmentGeneration();
	Controller->UnPossess(); Controller->Possess(Pawn);
	TestTrue(TEXT("Same-pawn reacquisition advances real assignment epoch"), Controller->GetPawn() == Pawn
		&& Controller->GetPawnAssignmentGeneration() > Assignment);
	Scope.Begin(Controller); Scope.Tick();
	TestEqual(TEXT("Queued older possession gets no generator callback"), Component->GeneratorInitializations.Num(), 0);
	TestFalse(TEXT("Retired pending restore is rejected"), Component->WasSaveRecordLoadAccepted());
	TestNull(TEXT("Retired pending restore creates no activity"), Component->GetActivity(USovNPCActivityRestoreTestActivity::StaticClass()));

	Component->SeedSavedValues(92);
	Component->DuringGeneratorInitialization = [Controller]() { Controller->Destroy(); };
	INarrativeSavableComponent::Execute_Load(Component);
	TestEqual(TEXT("Current real owner may initialize generator before its callback retires it"), Component->GeneratorInitializations.Num(), 1);
	TestEqual(TEXT("EndPlay during generator callback prevents all later goal callbacks"), Component->GoalInitializations.Num(), 0);
	TestEqual(TEXT("No goal published for ended controller"), Component->GetGoals(USovNPCActivityRestoreTestGoal::StaticClass()).Goals.Num(), 0);
	Scope.Tick();
	TestEqual(TEXT("No deferred work revives ended controller"), Component->GoalInitializations.Num(), 0);

	auto* ScoringController = Scope.SpawnController();
	if (!TestNotNull(TEXT("Second real controller for scoring reentry"), ScoringController)) { return false; }
	Scope.Begin(ScoringController);
	auto* ScoringComponent = ScoringController->TestActivities();
	ScoringComponent->SeedSavedValues(93);
	const uint64 ScoringAssignment = ScoringController->GetPawnAssignmentGeneration();
	ScoringComponent->DuringActivityScore = [ScoringController, ScoringComponent]()
	{
		ScoringComponent->DuringActivityScore = nullptr;
		APawn* ScoringPawn = ScoringController->GetPawn();
		ScoringController->UnPossess(); ScoringController->Possess(ScoringPawn);
	};
	INarrativeSavableComponent::Execute_Load(ScoringComponent);
	TestTrue(TEXT("Final real scoring callback changed the possession epoch"),
		ScoringController->GetPawnAssignmentGeneration() > ScoringAssignment);
	TestFalse(TEXT("Final scoring retirement rejects load completion"), ScoringComponent->WasSaveRecordLoadAccepted());
	TestFalse(TEXT("Retired scoring continuation cancels its rescore timer"),
		Scope.World->GetTimerManager().IsTimerActive(ScoringComponent->TimerHandle_RescoreGoals));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovNPCActivityRestorePreflightTest,
	"ProjectVelkorran.Campaign.NPCActivityRestore.DetachedPreflightRejectsMissingSavedClass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovNPCActivityRestorePreflightTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCActivityRestoreTests;
	FWorld Scope; auto* Controller = Scope.SpawnController();
	if (!TestNotNull(TEXT("Controller"), Controller)) { return false; }
	auto* Component = Controller->TestActivities(); Component->SeedSavedValues(18);
	const auto Bytes = [Component]()
	{
		TArray<uint8> Data; FMemoryWriter Writer(Data);
		FObjectAndNameAsStringProxyArchive Ar(Writer, true); Ar.ArIsSaveGame = true;
		Component->Serialize(Ar); return Data;
	};
	TestTrue(TEXT("Valid serialized rows pass detached preflight before startup"), Component->ValidateSaveRecord(Bytes()));
	Component->InvalidateSavedActivityClass();
	TestFalse(TEXT("Missing saved class is rejected synchronously, before deferred actor mutation"), Component->ValidateSaveRecord(Bytes()));
	TestEqual(TEXT("Preflight never initializes a generator"), Component->GeneratorInitializations.Num(), 0);
	TestEqual(TEXT("Preflight never initializes a goal"), Component->GoalInitializations.Num(), 0);
	TestFalse(TEXT("Preflight does not synthesize the owner cache"), Component->HasCachedOwner());
	TestNull(TEXT("Preflight does not publish a live activity"), Component->GetActivity(USovNPCActivityRestoreTestActivity::StaticClass()));
	return true;
}
#endif
