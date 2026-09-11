// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNPCGoalKeyLifetimeTestFixtures.h"
#include "Tests/SovNPCActivityRestoreTestFixtures.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "TimerManager.h"
#include "UObject/Script.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#if WITH_AUTOMATION_TESTS
namespace SovNPCGoalKeyLifetimeTests
{
    struct FWorld
    {
#if WITH_EDITOR
        FEditorScriptExecutionGuard ScriptGuard;
#endif
        UWorld* World = nullptr;
        ASovNPCActivityRestoreTestController* Controller = nullptr;
        FWorld()
        {
            const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
            if (!World) { return; }
            World->InitializeActorsForPlay(FURL());
            auto* Pawn = SpawnTarget();
            Controller = World->SpawnActor<ASovNPCActivityRestoreTestController>();
            if (Controller && Pawn)
            {
                Controller->Possess(Pawn);
                Pawn->DispatchBeginPlay();
                Controller->DispatchBeginPlay();
            }
        }
        ~FWorld()
        {
            if (IsValid(Controller) && Controller->GetActivityComponent())
            { Controller->GetActivityComponent()->RemoveAllGoals(); }
            if (World) { World->DestroyWorld(false); }
        }
        ASovNPCActivityRestoreTestPawn* SpawnTarget()
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            return World->SpawnActor<ASovNPCActivityRestoreTestPawn>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
        }
        UNPCActivityComponent* Activities() const { return Controller ? Controller->GetActivityComponent() : nullptr; }
        USovNPCGoalKeyLifetimeTestActivity* AddActivity(UClass* GoalClass)
        {
            auto* Activity = Cast<USovNPCGoalKeyLifetimeTestActivity>(Activities()->AddActivity(USovNPCGoalKeyLifetimeTestActivity::StaticClass(), false));
            if (Activity) { Activity->Support(GoalClass); }
            return Activity;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStockAttackGoalDestroyedTargetTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.StockAttackGoalRetiresDestroyedLivingTarget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovStockAttackGoalDestroyedTargetTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    FWorld Scope;
    if (!TestNotNull(TEXT("Real initialized controller"), Scope.Controller)) { return false; }
    auto* Component = Scope.Activities();
    UClass* GoalClass = LoadClass<UNPCGoalItem>(nullptr,
        TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
    if (!TestNotNull(TEXT("Actual shipped attack goal Blueprint"), GoalClass)) { return false; }
    auto* Target = Scope.SpawnTarget();
    auto* ReplacementTarget = Scope.SpawnTarget();
    auto* Activity = Scope.AddActivity(GoalClass);
    if (!TestNotNull(TEXT("Target actor"), Target) || !TestNotNull(TEXT("Replacement actor"), ReplacementTarget)
        || !TestNotNull(TEXT("Native scoring activity"), Activity)) { return false; }
    const FObjectPropertyBase* TargetProperty = FindFProperty<FObjectPropertyBase>(GoalClass, TEXT("TargetToAttack"));
    const FStructProperty* FastTimer = FindFProperty<FStructProperty>(GoalClass, TEXT("FastTickHandle"));
    const FStructProperty* SlowTimer = FindFProperty<FStructProperty>(GoalClass, TEXT("SlowTickHandle"));
    if (!TestNotNull(TEXT("Real stock target property"), TargetProperty) || !TestNotNull(TEXT("Fast timer"), FastTimer)
        || !TestNotNull(TEXT("Slow timer"), SlowTimer)) { return false; }
    TStrongObjectPtr<UNPCGoalItem> Goal(NewObject<UNPCGoalItem>(Component, GoalClass));
    TargetProperty->SetObjectPropertyValue_InContainer(Goal.Get(), Target);
    if (!TestEqual(TEXT("Real Initialize and registration accept stock goal"), Component->AddGoal(Goal.Get(), true), Goal.Get())) { return false; }
    TestEqual(TEXT("Blueprint key is its exact target"), Goal->GetGoalKey(), static_cast<UObject*>(Target));
    TestTrue(TEXT("Actual stock score is positive for a valid owner and target"), Activity->ScoreOne(Goal.Get()) > 0.f);
    TestEqual(TEXT("Ordinary selection activates the goal"), Component->GetCurrentActivityGoal(), Goal.Get());
    const FTimerHandle FastBefore = *FastTimer->ContainerPtrToValuePtr<FTimerHandle>(Goal.Get());
    const FTimerHandle SlowBefore = *SlowTimer->ContainerPtrToValuePtr<FTimerHandle>(Goal.Get());
    TestTrue(TEXT("Actual Blueprint Initialize starts both normal timers"),
        Scope.World->GetTimerManager().TimerExists(FastBefore) && Scope.World->GetTimerManager().TimerExists(SlowBefore));
    // Reproduce the uncovered ordering: the stock SlowTick directly calls its
    // Blueprint score; a faster target-validity timer need not get there first.
    Scope.World->GetTimerManager().PauseTimer(FastBefore);
    TestTrue(TEXT("Actual slow timer is active before target destruction"), Scope.World->GetTimerManager().IsTimerActive(SlowBefore));
    TestTrue(TEXT("Target has not entered the native ASC death state"), Target->IsAlive());
    // A handoff destroys a living actor without emitting an ASC death event.
    if (!TestTrue(TEXT("Actual actor destruction"), Target->Destroy())) { return false; }
    TestFalse(TEXT("Old target is natively invalid"), IsValid(Target));
    TestFalse(TEXT("Actor destruction retires its registered goal synchronously"), Component->GetGoals(GoalClass).Goals.Contains(Goal.Get()));
    TestFalse(TEXT("Destruction cleanup cancels even the paused fast timer"), Scope.World->GetTimerManager().TimerExists(FastBefore));
    TestFalse(TEXT("Destruction cleanup cancels the direct-score slow timer before its next callback"), Scope.World->GetTimerManager().TimerExists(SlowBefore));
    // Real timer dispatch now crosses the original slow interval without a
    // manufactured callback or an intervening replacement/selection request.
    Scope.World->GetTimerManager().Tick(0.3f);
    TStrongObjectPtr<UNPCGoalItem> Replacement(NewObject<UNPCGoalItem>(Component, GoalClass));
    TargetProperty->SetObjectPropertyValue_InContainer(Replacement.Get(), ReplacementTarget);
    TestEqual(TEXT("A real replacement goal triggers ordinary comparison and reselection"),
        Component->AddGoal(Replacement.Get(), true), Replacement.Get());
    const auto Remaining = Component->GetGoals(GoalClass);
    TestFalse(TEXT("Old goal removed by ordinary selection"), Remaining.Goals.Contains(Goal.Get()));
    TestTrue(TEXT("Replacement preserved"), Remaining.Goals.Contains(Replacement.Get()));
    TestEqual(TEXT("Replacement is selected"), Component->GetCurrentActivityGoal(), Replacement.Get());
    TestFalse(TEXT("Stock OnRemoved cleared original fast timer"), Scope.World->GetTimerManager().TimerExists(FastBefore));
    TestFalse(TEXT("Stock OnRemoved cleared original slow timer"), Scope.World->GetTimerManager().TimerExists(SlowBefore));
    TestFalse(TEXT("No expired registration retains the old goal"), Remaining.GoalUniqueObjectMap.FindKey(Goal.Get()) != nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGoalKeylessAndReentrantRemovalTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.KeylessLiveAndReentrantReplacementRemainValid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovGoalKeylessAndReentrantRemovalTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    FWorld Scope;
    if (!TestNotNull(TEXT("Controller"), Scope.Controller)) { return false; }
    auto* Component = Scope.Activities();
    auto* Activity = Scope.AddActivity(USovNPCGoalKeyLifetimeTestGoal::StaticClass());
    auto* Target = Scope.SpawnTarget();
    if (!TestNotNull(TEXT("Scoring activity"), Activity) || !TestNotNull(TEXT("Live key"), Target)) { return false; }
    auto* Keyless = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    auto* Keyed = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    Keyed->GoalKey = Target; Keyed->DefaultScore = 2.f;
    Component->AddGoal(Keyless, true); Component->AddGoal(Keyed, true);
    const int32 KeyReads = Keyed->KeyReads;
    TestFalse(TEXT("Keyless goals remain legal"), Component->HasStaleRegisteredGoalKey(Keyless));
    TestFalse(TEXT("Live registered object key remains legal"), Component->HasStaleRegisteredGoalKey(Keyed));
    TestEqual(TEXT("Admission never calls the Blueprint key getter"), Keyed->KeyReads, KeyReads);
    TestEqual(TEXT("Live score remains unchanged"), Activity->ScoreOne(Keyed), 2.f);
    auto* Replacement = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    Replacement->GoalKey = Target; Replacement->DefaultScore = 3.f;
    bool bReentered = false;
    Keyed->DuringRemoval = [&]() { bReentered = Component->AddGoal(Replacement, true) == Replacement; };
    Component->RemoveGoal(Keyed);
    TestTrue(TEXT("OnRemoved can register a new goal for the same live key"), bReentered);
    bool bFound = false;
    TestEqual(TEXT("Outer cleanup cannot erase callback's replacement"),
        Component->GetGoalByKey(Replacement->GetClass(), Target, bFound), static_cast<UNPCGoalItem*>(Replacement));
    TestTrue(TEXT("Replacement remains indexed"), bFound);
    TestEqual(TEXT("Retired goal getter was not called during cleanup"), Keyed->KeyReads, KeyReads);
    TestEqual(TEXT("Cleanup callback fired once"), Keyed->Removals, 1);
    Keyed->DuringRemoval = nullptr;
    TestTrue(TEXT("Unkeyed goal remains in the actual container"), Component->GetGoals(Keyless->GetClass()).Goals.Contains(Keyless));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRestoredGoalKeyLifetimeTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.ActualRecordRestoreRebuildsKeyLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovRestoredGoalKeyLifetimeTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    FWorld Scope;
    if (!TestNotNull(TEXT("Controller"), Scope.Controller)) { return false; }
    auto* Component = Scope.Activities();
    auto* Target = Scope.SpawnTarget();
    if (!TestNotNull(TEXT("Target"), Target)) { return false; }
    auto* Goal = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    Goal->GoalKey = Target; Goal->bSaveGoal = true;
    Component->AddGoal(Goal, false);
    auto* Save = Scope.World->GetSubsystem<UNarrativeSaveSubsystem>();
    FNarrativeActorRecord Record;
    if (!TestNotNull(TEXT("Actual world save subsystem"), Save)
        || !TestTrue(TEXT("Actual controller/component record"), Save->CreateActorRecord(Scope.Controller, Record))) { return false; }
    Component->RemoveAllGoals();
    if (!TestTrue(TEXT("Actual record load accepted"), Save->LoadActorFromRecord(Scope.Controller, Record))) { return false; }
    TestTrue(TEXT("Actual activity restore completed"), Component->WasSaveRecordLoadAccepted() && !Component->HasPendingSavedActivityRestore());
    bool bFound = false;
    auto* Restored = Component->GetGoalByKey(USovNPCGoalKeyLifetimeTestGoal::StaticClass(), Target, bFound);
    if (!TestTrue(TEXT("Saved exact object key reconstructed by native load"), bFound) || !TestNotNull(TEXT("Restored goal"), Restored)) { return false; }
    TestFalse(TEXT("Restored live key admitted"), Component->HasStaleRegisteredGoalKey(Restored));
    Target->Destroy();
    TestFalse(TEXT("Restored actor binding removes the goal synchronously without new saved fields"),
        Component->GetGoals(Restored->GetClass()).Goals.Contains(Restored));
    auto* Activity = Scope.AddActivity(Restored->GetClass());
    if (!TestNotNull(TEXT("Native scoring activity"), Activity)) { return false; }
    Component->PerformActivitySelection(true);
    TestFalse(TEXT("Native selection removes restored stale goal"), Component->GetGoals(Restored->GetClass()).Goals.Contains(Restored));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGoalActorDestructionRegistrationTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.ActorDestructionPreservesOtherKeysAndReentrantReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovGoalActorDestructionRegistrationTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    FWorld Scope;
    if (!TestNotNull(TEXT("Controller"), Scope.Controller)) { return false; }
    auto* Component = Scope.Activities();
    auto* Target = Scope.SpawnTarget();
    auto* OtherTarget = Scope.SpawnTarget();
    auto* ReplacementTarget = Scope.SpawnTarget();
    if (!TestNotNull(TEXT("Target"), Target) || !TestNotNull(TEXT("Other target"), OtherTarget)
        || !TestNotNull(TEXT("Replacement target"), ReplacementTarget)) { return false; }
    auto* Activity = Scope.AddActivity(USovNPCGoalKeyLifetimeTestGoal::StaticClass());
    if (!TestNotNull(TEXT("Native selection activity"), Activity)) { return false; }
    auto* Retiring = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    auto* Other = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    auto* Keyless = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    Retiring->GoalKey = Target; Retiring->DefaultScore = 3.f;
    Other->GoalKey = OtherTarget; Other->DefaultScore = 2.f;
    Component->AddGoal(Retiring, true);
    Component->AddGoal(Other, true);
    Component->AddGoal(Keyless, true);
    const FName CallbackName(TEXT("OnGoalKeyActorDestroyed"));
    TestTrue(TEXT("Exact actor owns the native destruction subscription"), Target->OnDestroyed.Contains(Component, CallbackName));
    auto* Replacement = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    auto* Refused = NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component);
    Replacement->GoalKey = ReplacementTarget; Replacement->DefaultScore = 4.f;
    Refused->GoalKey = Target;
    bool bReplaced = false, bRejectedDyingKey = false;
    Retiring->DuringRemoval = [&]()
    {
        bRejectedDyingKey = Component->AddGoal(Refused, true) == nullptr;
        bReplaced = Component->AddGoal(Replacement, true) == Replacement;
    };
    const int32 ScoresBeforeRetirement = Retiring->ScoreReads;
    Target->Destroy();
    TestEqual(TEXT("Reentrant replacement never scores the removed current goal"), Retiring->ScoreReads, ScoresBeforeRetirement);
    TestTrue(TEXT("Removed current goal is no longer admitted for scoring"), Component->HasStaleRegisteredGoalKey(Retiring));
    TestEqual(TEXT("Destroyed registration receives exactly one normal cleanup"), Retiring->Removals, 1);
    TestTrue(TEXT("Cleanup cannot register another goal for the actor already being destroyed"), bRejectedDyingKey);
    TestEqual(TEXT("Rejected initialized goal receives normal timer cleanup"), Refused->Removals, 1);
    TestTrue(TEXT("Cleanup may register a new living target normally"), bReplaced);
    const auto Remaining = Component->GetGoals(Retiring->GetClass());
    TestFalse(TEXT("Destroyed goal removed"), Remaining.Goals.Contains(Retiring));
    TestTrue(TEXT("Other actor's goal preserved"), Remaining.Goals.Contains(Other));
    TestTrue(TEXT("Keyless goal preserved"), Remaining.Goals.Contains(Keyless));
    TestTrue(TEXT("Callback replacement preserved"), Remaining.Goals.Contains(Replacement));
    TestEqual(TEXT("Ordinary selection accepts the callback replacement"), Component->GetCurrentActivityGoal(), static_cast<UNPCGoalItem*>(Replacement));
    TestTrue(TEXT("New actor receives its own subscription"), ReplacementTarget->OnDestroyed.Contains(Component, CallbackName));
    Retiring->DuringRemoval = nullptr;
    Component->RemoveGoal(Other);
    TestFalse(TEXT("Normal removal unbinds its last actor-key subscription"), OtherTarget->OnDestroyed.Contains(Component, CallbackName));
    OtherTarget->Destroy();
    TestEqual(TEXT("Later actor destruction cannot clean a removed registration twice"), Other->Removals, 1);
    Component->RemoveAllGoals();
    TestFalse(TEXT("RemoveAllGoals releases the remaining actor subscription"), ReplacementTarget->OnDestroyed.Contains(Component, CallbackName));
    ReplacementTarget->Destroy();
    TestEqual(TEXT("Whole-goal cleanup remains single-shot"), Replacement->Removals, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRejectedStockAttackGoalCleanupTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.RejectedStockDuplicateCancelsItsOwnTimers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovRejectedStockAttackGoalCleanupTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    FWorld Scope;
    if (!TestNotNull(TEXT("Real initialized controller"), Scope.Controller)) { return false; }
    auto* Component = Scope.Activities();
    UClass* GoalClass = LoadClass<UNPCGoalItem>(nullptr,
        TEXT("/NarrativePro/Pro/Core/AI/Activities/Attacks/Goals/Goal_Attack.Goal_Attack_C"));
    if (!TestNotNull(TEXT("Actual shipped attack goal Blueprint"), GoalClass)) { return false; }
    auto* Target = Scope.SpawnTarget();
    auto* ReplacementTarget = Scope.SpawnTarget();
    auto* Activity = Scope.AddActivity(GoalClass);
    if (!TestNotNull(TEXT("Live target"), Target) || !TestNotNull(TEXT("Replacement target"), ReplacementTarget)
        || !TestNotNull(TEXT("Ordinary scoring activity"), Activity)) { return false; }
    const FObjectPropertyBase* TargetProperty = FindFProperty<FObjectPropertyBase>(GoalClass, TEXT("TargetToAttack"));
    const FStructProperty* FastTimer = FindFProperty<FStructProperty>(GoalClass, TEXT("FastTickHandle"));
    const FStructProperty* SlowTimer = FindFProperty<FStructProperty>(GoalClass, TEXT("SlowTickHandle"));
    if (!TestNotNull(TEXT("Stock target property"), TargetProperty) || !TestNotNull(TEXT("Stock fast timer"), FastTimer)
        || !TestNotNull(TEXT("Stock slow timer"), SlowTimer)) { return false; }
    TStrongObjectPtr<UNPCGoalItem> Accepted(NewObject<UNPCGoalItem>(Component, GoalClass));
    TargetProperty->SetObjectPropertyValue_InContainer(Accepted.Get(), Target);
    if (!TestEqual(TEXT("First real stock goal is admitted"), Component->AddGoal(Accepted.Get(), true), Accepted.Get())) { return false; }
    const auto ReadFast = [&](UNPCGoalItem* Goal) { return *FastTimer->ContainerPtrToValuePtr<FTimerHandle>(Goal); };
    const auto ReadSlow = [&](UNPCGoalItem* Goal) { return *SlowTimer->ContainerPtrToValuePtr<FTimerHandle>(Goal); };
    const FTimerHandle AcceptedFast = ReadFast(Accepted.Get());
    const FTimerHandle AcceptedSlow = ReadSlow(Accepted.Get());
    TestTrue(TEXT("Real Initialize starts both accepted timers"), Scope.World->GetTimerManager().TimerExists(AcceptedFast)
        && Scope.World->GetTimerManager().TimerExists(AcceptedSlow));
    TestNull(TEXT("Adding the exact published instance again is refused"), Component->AddGoal(Accepted.Get(), true));
    TestTrue(TEXT("Refusing the same instance neither reinitializes nor clears its fast timer"),
        ReadFast(Accepted.Get()) == AcceptedFast && Scope.World->GetTimerManager().TimerExists(AcceptedFast));
    TestTrue(TEXT("Refusing the same instance preserves its slow timer"),
        ReadSlow(Accepted.Get()) == AcceptedSlow && Scope.World->GetTimerManager().TimerExists(AcceptedSlow));

    // A distinct candidate executes the actual stock Initialize before its key
    // collides. Its timers must be retired even though it was never registered.
    TStrongObjectPtr<UNPCGoalItem> Rejected(NewObject<UNPCGoalItem>(Component, GoalClass));
    TargetProperty->SetObjectPropertyValue_InContainer(Rejected.Get(), Target);
    TestNull(TEXT("Duplicate target candidate is refused"), Component->AddGoal(Rejected.Get(), true));
    TestEqual(TEXT("Only the original goal remains registered"), Component->GetGoals(GoalClass).Goals.Num(), 1);
    TestFalse(TEXT("Rejected candidate is absent from the real container"), Component->GetGoals(GoalClass).Goals.Contains(Rejected.Get()));
    TestFalse(TEXT("Actual OnRemoved invalidates rejected fast timer"), ReadFast(Rejected.Get()).IsValid());
    TestFalse(TEXT("Actual OnRemoved invalidates rejected direct-score slow timer"), ReadSlow(Rejected.Get()).IsValid());
    TestTrue(TEXT("Duplicate cleanup preserves the accepted timer leases"), Scope.World->GetTimerManager().TimerExists(AcceptedFast)
        && Scope.World->GetTimerManager().TimerExists(AcceptedSlow));
    TestEqual(TEXT("Original goal remains the selected activity goal"), Component->GetCurrentActivityGoal(), Accepted.Get());

    TestTrue(TEXT("Target remains alive until ordinary actor retirement"), Target->IsAlive());
    if (!TestTrue(TEXT("Real living-target destruction"), Target->Destroy())) { return false; }
    TestFalse(TEXT("Existing destruction subscription retires accepted fast timer"), Scope.World->GetTimerManager().TimerExists(AcceptedFast));
    TestFalse(TEXT("Existing destruction subscription retires accepted slow timer"), Scope.World->GetTimerManager().TimerExists(AcceptedSlow));
    // The rejected stock goal is strongly retained intentionally: GC cannot hide
    // a leaked timer. Actual timer dispatch must never score its retired target.
    Scope.World->GetTimerManager().Tick(0.3f);
    TStrongObjectPtr<UNPCGoalItem> Replacement(NewObject<UNPCGoalItem>(Component, GoalClass));
    TargetProperty->SetObjectPropertyValue_InContainer(Replacement.Get(), ReplacementTarget);
    TestEqual(TEXT("New living target still uses normal stock goal admission"), Component->AddGoal(Replacement.Get(), true), Replacement.Get());
    TestEqual(TEXT("Ordinary selection reaches the new target"), Component->GetCurrentActivityGoal(), Replacement.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRestoredDuplicateGoalCleanupTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.RestoredDuplicateRetiresOnlyRejectedCandidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovRestoredDuplicateGoalCleanupTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    FWorld Scope;
    if (!TestNotNull(TEXT("Real initialized controller"), Scope.Controller)) { return false; }
    auto* Component = Scope.Activities();
    auto* Target = Scope.SpawnTarget();
    if (!TestNotNull(TEXT("Live actor key"), Target)) { return false; }
    TStrongObjectPtr<USovNPCGoalKeyLifetimeTestGoal> Accepted(NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component));
    Accepted->GoalKey = Target;
    Accepted->bSaveGoal = true;
    if (!TestEqual(TEXT("Real registration"), Component->AddGoal(Accepted.Get(), false), static_cast<UNPCGoalItem*>(Accepted.Get()))) { return false; }
    auto* Save = Scope.World->GetSubsystem<UNarrativeSaveSubsystem>();
    FNarrativeActorRecord Record;
    if (!TestNotNull(TEXT("Actual save subsystem"), Save)
        || !TestTrue(TEXT("Capture real controller/component/goal bytes"), Save->CreateActorRecord(Scope.Controller, Record))) { return false; }
    // Unsaved live generator goals are intentionally retained by native Load.
    // The actual saved row now collides with that preserved registration.
    Accepted->bSaveGoal = false;
    if (!TestTrue(TEXT("Load the real saved record"), Save->LoadActorFromRecord(Scope.Controller, Record))) { return false; }
    TestTrue(TEXT("Restore completes without changing saved-state acceptance"), Component->WasSaveRecordLoadAccepted()
        && !Component->HasPendingSavedActivityRestore());
    TestEqual(TEXT("Exactly one live registration remains"), Component->GetGoals(Accepted->GetClass()).Goals.Num(), 1);
    TestTrue(TEXT("Preserved original is still registered"), Component->GetGoals(Accepted->GetClass()).Goals.Contains(Accepted.Get()));
    TestEqual(TEXT("Rejected saved candidate cannot clean the accepted goal"), Accepted->Removals, 0);
    TArray<TStrongObjectPtr<USovNPCGoalKeyLifetimeTestGoal>> Candidates;
    for (TObjectIterator<USovNPCGoalKeyLifetimeTestGoal> It; It; ++It)
    {
        if (It->GetOuter() == Component && *It != Accepted.Get() && It->GoalKey == Target)
        { Candidates.Emplace(*It); }
    }
    if (!TestEqual(TEXT("Actual load constructed exactly one duplicate"), Candidates.Num(), 1)) { return false; }
    TestEqual(TEXT("Unpublished deserialized goal receives its normal cleanup once"), Candidates[0]->Removals, 1);
    TestFalse(TEXT("Rejected saved candidate is never published"), Component->GetGoals(Accepted->GetClass()).Goals.Contains(Candidates[0].Get()));
    Target->Destroy();
    TestEqual(TEXT("Later target destruction cleans the accepted goal exactly once"), Accepted->Removals, 1);
    TestEqual(TEXT("Later target destruction does not clean the rejected goal twice"), Candidates[0]->Removals, 1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReentrantGoalPublicationCleanupTest,
    "ProjectVelkorran.Campaign.NPCGoalLifetime.ReentrantPublicationPreservesAcceptedCandidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovReentrantGoalPublicationCleanupTest::RunTest(const FString& Parameters)
{
    using namespace SovNPCGoalKeyLifetimeTests;
    for (const bool bFromKeyRead : {false, true})
    {
        FWorld Scope;
        if (!TestNotNull(TEXT("Real initialized controller"), Scope.Controller)) { return false; }
        auto* Component = Scope.Activities();
        auto* Target = Scope.SpawnTarget();
        auto* Activity = Scope.AddActivity(USovNPCGoalKeyLifetimeTestGoal::StaticClass());
        if (!TestNotNull(TEXT("Live target"), Target) || !TestNotNull(TEXT("Scoring activity"), Activity)) { return false; }
        TStrongObjectPtr<USovNPCGoalKeyLifetimeTestGoal> Candidate(NewObject<USovNPCGoalKeyLifetimeTestGoal>(Component));
        Candidate->GoalKey = Target;
        bool bNestedAccepted = false;
        const auto PublishFromCallback = [&]()
        {
            Candidate->DuringInitialize = nullptr;
            Candidate->DuringKeyRead = nullptr;
            bNestedAccepted = Component->AddGoal(Candidate.Get(), true) == Candidate.Get();
        };
        if (bFromKeyRead) { Candidate->DuringKeyRead = PublishFromCallback; }
        else { Candidate->DuringInitialize = PublishFromCallback; }
        TestNull(TEXT("Outer admission refuses its already-published candidate"), Component->AddGoal(Candidate.Get(), true));
        TestTrue(TEXT("Actual callback admitted the same candidate"), bNestedAccepted);
        TestEqual(TEXT("Outer refusal cannot clean callback-owned work"), Candidate->Removals, 0);
        TestEqual(TEXT("Only the accepted exact instance is registered"), Component->GetGoals(Candidate->GetClass()).Goals.Num(), 1);
        TestTrue(TEXT("Callback registration remains admitted"), !Component->HasStaleRegisteredGoalKey(Candidate.Get()));
        TestEqual(TEXT("Callback registration owns the current activity"), Component->GetCurrentActivityGoal(), static_cast<UNPCGoalItem*>(Candidate.Get()));
        Component->RemoveGoal(Candidate.Get());
        TestEqual(TEXT("Later normal removal cleans that registration exactly once"), Candidate->Removals, 1);
    }
    return true;
}

#endif
