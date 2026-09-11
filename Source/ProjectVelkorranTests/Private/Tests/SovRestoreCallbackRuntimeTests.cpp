// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Campaign/SovEncounterDirector.h"
#include "Character/PlayerDefinition.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Weapons/WeaponVisual.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#if WITH_AUTOMATION_TESTS
struct FSovEncounterCallbackTestAccess
{
    static void Suspend(ASovEncounterDirector* Director, AActor* Actor) { Director->SuspendActor(Actor); }
    static uint64 Generation(const ASovEncounterDirector* Director) { return Director->RestoreGeneration; }
    static void Invalidate(ASovEncounterDirector* Director) { ++Director->RestoreGeneration; }
    static bool Release(ASovEncounterDirector* Director)
    { const uint64 Generation = Director->RestoreGeneration; return Director->ReleaseSuspensions([Director, Generation]() { return Director->RestoreGeneration == Generation; }); }
    static void SeedFinish(ASovEncounterDirector* Director, ASovPlayerCharacterBase* Player)
    {
        Director->State = ESovEncounterState::Restoring; Director->AttemptId = FGuid::NewGuid(); Director->EncounterPlayer = Player; Director->bPlayerAndControllerRestored = true;
        Director->RestoreController = Player->GetController(); Director->RestorePlayerState = Player->GetPlayerState<ASovPlayerState>(); Director->RestorePlayerASC = Player->GetNarrativeAbilitySystemComponent();
    }
    static void Finish(ASovEncounterDirector* Director) { Director->FinishRestore(); }
    static void SeedAttempt(ASovEncounterDirector* Director, ASovPlayerCharacterBase* Player)
    { Director->State = ESovEncounterState::Failed; Director->EncounterPlayer = Player; }
    static void SeedLegacyActor(ASovEncounterDirector* Director, AActor* Actor) { Director->AttemptActors.AddUnique(Actor); }
    static void Cleanup(ASovEncounterDirector* Director) { Director->CleanupAttemptActors(); }
};
struct FSovTransitionCallbackTestAccess
{
    static void Seed(ASovPlayerController* PC, ASovPlayerCharacterBase* Pawn)
    { PC->PendingPawn = Pawn; PC->TransitionState = ESovCampaignTransitionState::Initializing; ++PC->TransitionEpoch; }
    static void Fail(ASovPlayerController* PC) { PC->FailCampaignInitialization(TEXT("Callback ownership regression")); }
    static void ReplaceOperation(ASovPlayerController* PC, ASovPlayerCharacterBase* Pawn)
    { ++PC->TransitionEpoch; PC->PendingPawn = Pawn; PC->TransitionState = ESovCampaignTransitionState::Initializing; }
    static void PollOldEpoch(ASovPlayerController* PC, uint64 Epoch) { PC->PollCampaignInitialization(Epoch); }
    static uint64 Epoch(const ASovPlayerController* PC) { return PC->TransitionEpoch; }
};
namespace
{
struct FRestoreWorld
{
    UWorld* World = nullptr;
    FRestoreWorld()
    {
        const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
            ERHIFeatureLevel::Num, &WorldInitialization);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
    }
    ~FRestoreWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovRetryPreservesPresentationTest, "ProjectVelkorran.Campaign.Encounter.RetryPreservesCharacterAndWeaponVisuals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovRetryPreservesPresentationTest::RunTest(const FString& Parameters)
{
    FRestoreWorld F;
    auto* Director = F.World->SpawnActor<ASovEncounterDirector>();
    auto* Pawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
    auto* Visual = F.World->SpawnActor<ANarrativeCharacterVisual>();
    auto* WeaponVisual = F.World->SpawnActor<AWeaponVisual>();
    auto* Projectile = F.World->SpawnActor<AActor>();
    if (!Director || !Pawn || !Visual || !WeaponVisual || !Projectile) { return false; }
    FSovEncounterCallbackTestAccess::SeedAttempt(Director, Pawn);
    Visual->SetOwner(Pawn); WeaponVisual->SetOwner(Pawn); Projectile->SetOwner(WeaponVisual);
    TestFalse(TEXT("Player appearance is not disposable encounter content"), Director->RegisterAttemptActor(Visual));
    TestFalse(TEXT("Equipped weapon presentation is not disposable encounter content"), Director->RegisterAttemptActor(WeaponVisual));
    TestTrue(TEXT("A projectile owned through the weapon remains attempt content"), Director->RegisterAttemptActor(Projectile));
    // Old checkpoints may already contain the formerly overbroad classification.
    FSovEncounterCallbackTestAccess::SeedLegacyActor(Director, Visual);
    FSovEncounterCallbackTestAccess::SeedLegacyActor(Director, WeaponVisual);
    FSovEncounterCallbackTestAccess::Cleanup(Director);
    TestTrue(TEXT("Retry preserves the existing character visual"), IsValid(Visual) && !Visual->IsActorBeingDestroyed());
    TestTrue(TEXT("Retry preserves the existing weapon visual"), IsValid(WeaponVisual) && !WeaponVisual->IsActorBeingDestroyed());
    TestTrue(TEXT("Retry still retires attributed projectiles"), !IsValid(Projectile) || Projectile->IsActorBeingDestroyed());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSuspensionCallbackRuntimeTest, "ProjectVelkorran.Campaign.Encounter.SuspensionCallbackOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSuspensionCallbackRuntimeTest::RunTest(const FString& Parameters)
{
    FRestoreWorld F; auto* Director = F.World->SpawnActor<ASovEncounterDirector>();
    auto* Actor = F.World->SpawnActor<ASovAxiomRuntimeTestCharacter>();
    auto* ForeignActor = F.World->SpawnActor<ASovAxiomRuntimeTestCharacter>();
    if (!Director || !Actor || !ForeignActor) { return false; }
    Actor->InitializeTestCombat(0); ForeignActor->InitializeTestCombat(0);
    auto* ASC = Actor->GetNarrativeAbilitySystemComponent();
    auto* ForeignASC = ForeignActor->GetNarrativeAbilitySystemComponent();
    const auto& N = FNarrativeGameplayTags::Get();
    // UE 5.7 defers removal delegates but only dispatches them at a zero boundary.
    // Exercise the real callback on this actor and foreign Busy ownership on the
    // next actor, which an interrupted release must not reach.
    ASC->AddLooseGameplayTag(N.State_Invulnerable);
    ForeignASC->AddLooseGameplayTag(N.State_Busy); ForeignASC->AddLooseGameplayTag(N.State_Invulnerable);
    FSovEncounterCallbackTestAccess::Suspend(Director, Actor);
    FSovEncounterCallbackTestAccess::Suspend(Director, ForeignActor);
    bool bCallbackRan = false;
    const auto Callback = ASC->RegisterGameplayTagEvent(N.State_Busy, EGameplayTagEventType::AnyCountChange).AddLambda(
        [Director, &bCallbackRan](FGameplayTag Tag, int32 Count)
        { if (Count == 0) { bCallbackRan = true; FSovEncounterCallbackTestAccess::Invalidate(Director); } });
    TestFalse(TEXT("A synchronous GAS callback stops the old release"), FSovEncounterCallbackTestAccess::Release(Director));
    TestTrue(TEXT("The actual GAS removal callback retired the operation"), bCallbackRan);
    TestEqual(TEXT("Only its Busy count was removed before interruption"), ASC->GetTagCount(N.State_Busy), 0);
    TestEqual(TEXT("Protection is retained until the rightful operation resumes"), ASC->GetTagCount(N.State_Invulnerable), 2);
    TestEqual(TEXT("Interruption cannot touch the next actor's Busy owners"), ForeignASC->GetTagCount(N.State_Busy), 2);
    ASC->RegisterGameplayTagEvent(N.State_Busy, EGameplayTagEventType::AnyCountChange).Remove(Callback);
    FSovEncounterCallbackTestAccess::Suspend(Director, Actor);
    TestEqual(TEXT("Retry reacquires exactly one missing Busy count"), ASC->GetTagCount(N.State_Busy), 1);
    TestEqual(TEXT("Retry cannot stack another protection owner"), ASC->GetTagCount(N.State_Invulnerable), 2);
    TestTrue(TEXT("Current restore completes its owned cleanup"), FSovEncounterCallbackTestAccess::Release(Director));
    TestEqual(TEXT("Retried actor releases its last owned Busy"), ASC->GetTagCount(N.State_Busy), 0);
    TestEqual(TEXT("Foreign Busy survives interrupted release and retry"), ForeignASC->GetTagCount(N.State_Busy), 1);
    TestEqual(TEXT("Foreign protection survives interrupted release and retry"), ASC->GetTagCount(N.State_Invulnerable), 1);
    TestEqual(TEXT("The second actor also retains foreign protection"), ForeignASC->GetTagCount(N.State_Invulnerable), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFinishRestoreCallbackRuntimeTest, "ProjectVelkorran.Campaign.Encounter.FinishRejectsChangedPlayerOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovFinishRestoreCallbackRuntimeTest::RunTest(const FString& Parameters)
{
    FRestoreWorld F; auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
    auto* Pawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>(); auto* PS = F.World->SpawnActor<ASovPlayerState>();
    auto* Director = F.World->SpawnActor<ASovEncounterDirector>(); if (!PC || !Pawn || !PS || !Director) { return false; }
    auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
    Pawn->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Pawn);
    if (!Pawn->StageTestReadiness(PS, true) || !Pawn->CompleteCampaignDataInitialization(false)) { return false; }
    FSovEncounterCallbackTestAccess::SeedFinish(Director, Pawn); FSovEncounterCallbackTestAccess::Suspend(Director, Pawn);
    auto* ASC = Pawn->GetNarrativeAbilitySystemComponent(); const auto OriginalAttempt = Director->GetAttemptId();
    const auto Protection = FSovGameplayTags::Get().State_Invulnerable_Respawn;
    const auto Callback = ASC->RegisterGameplayTagEvent(Protection, EGameplayTagEventType::NewOrRemoved).AddLambda(
        [PC](FGameplayTag Tag, int32 Count) { if (Count > 0) { PC->UnPossess(); } });
    FSovEncounterCallbackTestAccess::Finish(Director);
    TestEqual(TEXT("Old restore cannot publish an active attempt after possession changed"), Director->GetEncounterState(), ESovEncounterState::Failed);
    TestEqual(TEXT("No replacement attempt ID is minted by the old callback"), Director->GetAttemptId(), OriginalAttempt);
    TestNull(TEXT("Old restore does not repossess the player"), PC->GetPawn());
    ASC->RegisterGameplayTagEvent(Protection, EGameplayTagEventType::NewOrRemoved).Remove(Callback);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovStaleTransitionCallbackRuntimeTest, "ProjectVelkorran.Campaign.Handoff.StalePollPreservesReplacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovStaleTransitionCallbackRuntimeTest::RunTest(const FString& Parameters)
{
    FRestoreWorld F; auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
    auto* OldPawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>(); auto* Replacement = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
    if (!PC || !OldPawn || !Replacement) { return false; }
    FSovTransitionCallbackTestAccess::Seed(PC, OldPawn); const auto OldEpoch = FSovTransitionCallbackTestAccess::Epoch(PC);
    FSovTransitionCallbackTestAccess::ReplaceOperation(PC, Replacement);
    FSovTransitionCallbackTestAccess::PollOldEpoch(PC, OldEpoch);
    TestEqual(TEXT("Expired readiness callback cannot fail the replacement operation"), PC->GetCampaignTransitionState(), ESovCampaignTransitionState::Initializing);
    TestEqual(TEXT("Expired readiness callback preserves replacement epoch"), FSovTransitionCallbackTestAccess::Epoch(PC), OldEpoch + 1);
    return true;
}
#endif
