// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAurelionScannerTestFixtures.h"
#include "Tests/SovAurelionThermalTestFixtures.h"
#include "AI/SovAurelionSweepScanner.h"
#include "AI/NarrativeNPCController.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/PhysicsVolume.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

FGameplayTag ASovAurelionScannerTestPlayer::GetProtagonistIdentityTag() const
{ return FSovGameplayTags::Get().Character_Player_Selene; }
void ASovAurelionScannerTestDrone::InitializeTestCombat()
{
    auto* ASC = GetNarrativeAbilitySystemComponent(); ASC->AddAttributeSetSubobject(GetAttributeSetBase());
    ASC->InitAbilityActorInfo(this, this);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    bEncounterSnapshotReady = true;
}

#if WITH_AUTOMATION_TESTS
namespace
{
struct FScannerWorld
{
    FEditorScriptExecutionGuard ScriptGuard;
    UWorld* World = nullptr;
    ASovHandoffRuntimeTestController* PC = nullptr;
    ASovAurelionScannerTestPlayer* Player = nullptr;
    ASovAurelionThermalTestDirector* Director = nullptr;
    ASovAurelionSweepScanner* Scanner = nullptr;
    TArray<ASovAurelionScannerTestDrone*> Drones;
    TArray<ANarrativeNPCController*> Controllers;
    bool bReady = false;
    uint64 Frame = GFrameCounter;
    FScannerWorld()
    {
        const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
        if (!World) { return; }
        if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
        World->InitializeActorsForPlay(FURL());
        PC = World->SpawnActor<ASovHandoffRuntimeTestController>(); Player = World->SpawnActor<ASovAurelionScannerTestPlayer>();
        auto* PS = World->SpawnActor<ASovPlayerState>();
        if (!PC || !Player || !PS) { return; }
        World->AddController(PC); PC->SetTestPlayerState(PS);
        auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
        Player->PrepareCampaignInitialization(Definition); PC->Possess(Player);
        if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
        Player->SetActorLocation(FVector(700.,0.,200.)); Player->SetActorEnableCollision(true);
        auto* Capsule = Player->GetCapsuleComponent(); Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
        Mission->MissionId = TEXT("M12_ScannerFixture"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
        Mission->PawnClass = ASovAurelionScannerTestPlayer::StaticClass(); Mission->PlayerDefinition = Definition;
        FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Relay"); Beat.ObjectiveText = FText::FromString(TEXT("Reach relay")); Mission->Beats.Add(Beat);
        if (PC->GetCampaignState()->BeginMission(Mission) != ESovCampaignResult::Applied) { return; }
        Director = World->SpawnActor<ASovAurelionThermalTestDirector>(); Director->EncounterId = TEXT("M12.Scanner.E2");
        Scanner = World->SpawnActor<ASovAurelionSweepScanner>(); Scanner->SetActorLocation(FVector(0.,0.,200.));
        Scanner->ScannerId = TEXT("M12.Scanner.A"); Scanner->MissionId = Mission->MissionId; Scanner->RelayDirector = Director;
        Scanner->Pitch = 0.f; Scanner->SweepHalfArc = 0.f; Scanner->RelayDroneIds = { TEXT("Drone1"), TEXT("Drone2") };
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto* Drone = World->SpawnActor<ASovAurelionScannerTestDrone>(ASovAurelionScannerTestDrone::StaticClass(),
                FVector(1500.,800. + Index * 300.,200.), FRotator::ZeroRotator, Spawn);
            auto* AI = World->SpawnActor<ANarrativeNPCController>();
            if (!Drone || !AI) { return; }
            Drone->InitializeTestCombat(); AI->Possess(Drone); AI->bAcceptNetworkThreats = true; AI->bShareThreatsWithFaction = false;
            if (!Director->RegisterParticipant(Scanner->RelayDroneIds[Index], Drone, false)) { return; }
            Drones.Add(Drone); Controllers.Add(AI);
        }
        FString Error; bReady = Scanner->ValidateConfiguration(Error) && Player->IsCharacterReady();
    }
    ~FScannerWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
    void Advance(const float Seconds)
    {
        const double Until = World->GetTimeSeconds() + Seconds;
        while (World->GetTimeSeconds() + UE_DOUBLE_SMALL_NUMBER < Until)
        { TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame); World->Tick(LEVELTICK_TimeOnly, FMath::Min(.05, Until - World->GetTimeSeconds())); }
    }
    UBoxComponent* Blocker()
    {
        auto* Actor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box); Box->SetBoxExtent(FVector(25.,150.,150.));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent(); Actor->SetActorLocation(FVector(350.,0.,200.)); return Box;
    }
    void Hold(const int32 Index, const bool bHold)
    {
        Controllers[Index]->SetThreatMemorySuspended(Director, bHold);
        auto* ASC = Drones[Index]->GetNarrativeAbilitySystemComponent();
        if (bHold) { ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy); }
        else { ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy); }
    }
    int32 ActorCount() const { int32 Count = 0; for (TActorIterator<AActor> It(World); It; ++It) { ++Count; } return Count; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionScannerPhysicalTest, "ProjectVelkorran.Campaign.Aurelion.Scanner.PhysicalConeOcclusionAndReadiness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionScannerPhysicalTest::RunTest(const FString& Parameters)
{
    FScannerWorld F; if (!TestTrue(TEXT("Actual ready Selene and exactly two registered drones"), F.bReady)) { return false; }
    auto* Block = F.Blocker(); F.Scanner->Tick(.05f);
    TestFalse(TEXT("Opaque corridor cover blocks the actual visibility trace"), F.Scanner->HasPendingObservation());
    Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    F.Player->SetActorLocation(FVector(-700.,0.,200.)); F.Scanner->Tick(.05f);
    TestFalse(TEXT("Behind the physical cone cannot trigger detection"), F.Scanner->HasPendingObservation());
    F.Player->SetActorLocation(FVector(3100.,0.,200.)); F.Scanner->Tick(.05f);
    TestFalse(TEXT("Off-route remote player is outside finite sensor range"), F.Scanner->HasPendingObservation());
    F.Player->SetActorLocation(FVector(700.,0.,200.)); F.Player->SetTestVisualReady(false); F.Scanner->Tick(.05f);
    TestFalse(TEXT("An unready pawn cannot authorize a sensor alarm"), F.Scanner->HasPendingObservation());
    F.Player->SetTestVisualReady(true);
    auto* ASC = F.Player->GetNarrativeAbilitySystemComponent(); ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
    F.Scanner->Tick(.05f); TestFalse(TEXT("Optical scanner respects actual cloak"), F.Scanner->HasPendingObservation());
    ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
    F.Controllers[0]->bAcceptNetworkThreats = false; F.Scanner->Tick(.05f);
    TestFalse(TEXT("Unselected stock controller cannot receive a sensor alarm"), F.Scanner->HasPendingObservation());
    F.Controllers[0]->bAcceptNetworkThreats = true;
    F.Scanner->Tick(.05f); TestTrue(TEXT("Clear cone and actual LOS detect the current ready player"), F.Scanner->HasPendingObservation());
    TestEqual(TEXT("Detection alone does not release the inactive encounter"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
    TestEqual(TEXT("No early alert reaches inactive relay recipients"), F.Scanner->GetAlertedRecipientCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionScannerDeferredTest, "ProjectVelkorran.Campaign.Aurelion.Scanner.HeldRelayReceivesOnlyFiniteCapturedNetworkObservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionScannerDeferredTest::RunTest(const FString& Parameters)
{
    FScannerWorld F; if (!TestTrue(TEXT("Ready physical scanner fixture"), F.bReady)) { return false; }
    // UE creates this world service on demand (World.cpp InternalGetDefaultPhysicsVolume).
    // Complete external fixture initialization before measuring sensor side effects.
    const int32 BeforePhysicsVolume = F.ActorCount();
    const auto* PhysicsVolume = F.World->GetDefaultPhysicsVolume();
    if (!TestNotNull(TEXT("Actual world physics volume initialized before sensor measurement"), PhysicsVolume)) { return false; }
    AddInfo(FString::Printf(TEXT("Fixture physics volume %s (%s); actor count %d -> %d before observation baseline"),
        *PhysicsVolume->GetPathName(), *PhysicsVolume->GetClass()->GetPathName(), BeforePhysicsVolume, F.ActorCount()));
    // NetworkPredictionWorldManager::OnWorldPreTick creates its replicated manager
    // even for LEVELTICK_TimeOnly. Observe that first engine tick separately from
    // the scanner transaction, without ever invoking the scanner's actor tick.
    TArray<FString> WarmupClasses;
    {
        const FDelegateHandle WarmupObserver = F.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
            [this, &WarmupClasses](AActor* Actor)
            {
                WarmupClasses.Add(GetPathNameSafe(Actor->GetClass()));
                AddInfo(FString::Printf(TEXT("First engine tick created %s (%s) before scanner baseline"),
                    *GetPathNameSafe(Actor), *GetPathNameSafe(Actor->GetClass())));
            }));
        ON_SCOPE_EXIT { F.World->RemoveOnActorSpawnedHandler(WarmupObserver); };
        F.Advance(.05f);
    }
    if (!TestEqual(TEXT("First time-only engine tick creates exactly its network manager"), WarmupClasses.Num(), 1)) { return false; }
    if (!TestEqual(TEXT("The only warm-up actor is the actual engine NetworkPrediction manager"),
        WarmupClasses[0], FString(TEXT("/Script/NetworkPrediction.NetworkPredictionReplicatedManager")))) { return false; }
    TestFalse(TEXT("Engine warm-up did not invoke sensor detection"), F.Scanner->HasPendingObservation());
    TestEqual(TEXT("Engine warm-up did not alert any recipient"), F.Scanner->GetAlertedRecipientCount(), 0);
    TestEqual(TEXT("Engine warm-up left the encounter inactive"), F.Director->GetEncounterState(), ESovEncounterState::Inactive);
    TestEqual(TEXT("Engine warm-up wrote no campaign journal"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    for (auto* Controller : F.Controllers)
    {
        FNarrativeThreatMemory WarmupMemory;
        TestFalse(TEXT("Engine warm-up created no receiver threat memory"), Controller->GetBestThreatMemory(F.Player, WarmupMemory));
    }
    const int32 ActorsBefore = F.ActorCount(); const FVector Seen = F.Player->GetActorLocation();
    TArray<FString> SpawnedActors;
    const FDelegateHandle SpawnObserver = F.World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda(
        [&SpawnedActors](AActor* Actor) { SpawnedActors.Add(FString::Printf(TEXT("%s (%s)"), *GetPathNameSafe(Actor), *GetPathNameSafe(Actor->GetClass()))); }));
    ON_SCOPE_EXIT { F.World->RemoveOnActorSpawnedHandler(SpawnObserver); };
    const double InitialTime = F.World->GetTimeSeconds();
    F.Hold(0, true); F.Hold(1, true); F.Scanner->Tick(.05f);
    if (!TestTrue(TEXT("Real detection may wait behind existing native suspension owners"), F.Scanner->HasPendingObservation())) { return false; }
    F.Advance(5.f); F.Player->SetActorLocation(FVector(1100.,500.,200.)); // Outside the cone; no new optical observation.
    F.Director->StartTestAttempt(F.Player); F.Scanner->Tick(.05f);
    TestEqual(TEXT("Active encounter alone cannot bypass either native controller hold"), F.Scanner->GetAlertedRecipientCount(), 0);
    F.Hold(0, false); F.Scanner->Tick(.05f);
    TestEqual(TEXT("Only the actually released recipient is admitted"), F.Scanner->GetAlertedRecipientCount(), 1);
    FNarrativeThreatMemory Memory;
    if (!TestTrue(TEXT("Released drone owns a real Narrative memory"), F.Controllers[0]->GetBestThreatMemory(F.Player, Memory))) { return false; }
    TestEqual(TEXT("No forged Sight observation was produced"), Memory.Source, ENarrativeThreatSource::NetworkSensor);
    TestTrue(TEXT("Memory preserves the actually seen position, not unseen player motion"), Memory.LastKnownPosition.Equals(Seen));
    TestTrue(TEXT("Deferred alert cannot extend the original expiry"), Memory.ExpiresAt <= InitialTime + 30.01);
    TestTrue(TEXT("Waiting ages confidence instead of rejuvenating the observation"), Memory.Confidence < .8f);
    TestFalse(TEXT("Suspended second drone still has no memory"), F.Controllers[1]->GetBestThreatMemory(F.Player, Memory));
    F.Hold(1, false); F.Scanner->Tick(.05f);
    TestEqual(TEXT("Exactly two existing drones received the real alarm"), F.Scanner->GetAlertedRecipientCount(), 2);
    F.Controllers[0]->GetBestThreatMemory(F.Player, Memory); const double DeliveredAt = Memory.ObservedAt;
    F.Advance(1.f); F.Scanner->Tick(.05f); F.Controllers[0]->GetBestThreatMemory(F.Player, Memory);
    TestEqual(TEXT("Already delivered observation is not refreshed every sensor tick"), Memory.ObservedAt, DeliveredAt);
    TestEqual(TEXT("Sensor never creates additional enemies or actors"), F.ActorCount(), ActorsBefore);
    TestEqual(TEXT("No actor was created even transiently during the sensor observation"), SpawnedActors.Num(), 0);
    for (const FString& Spawned : SpawnedActors) { AddError(TEXT("Unexpected actor creation during sensor test: ") + Spawned); }
    TestEqual(TEXT("Sensor never writes campaign completion"), F.PC->GetCampaignState()->GetJournal().Num(), 0);
    TestEqual(TEXT("Sensor never damages the player"), F.Player->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionScannerRetirementTest, "ProjectVelkorran.Campaign.Aurelion.Scanner.ExpiryAndReplacementRetirePendingAlarm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionScannerRetirementTest::RunTest(const FString& Parameters)
{
    for (int32 Case = 0; Case < 4; ++Case)
    {
        FScannerWorld F; if (!TestTrue(TEXT("Ready bounded retirement fixture"), F.bReady)) { return false; }
        F.Scanner->Tick(.05f); if (!TestTrue(TEXT("Physical detection captured before mutation"), F.Scanner->HasPendingObservation())) { return false; }
        if (Case == 0) { F.Player->SetActorLocation(FVector(-700.,0.,200.)); F.Advance(30.1f); }
        if (Case == 1) { auto* ASC = F.Player->GetNarrativeAbilitySystemComponent(); ASC->SetCharacterReadyEpoch(ASC->GetCharacterReadyEpoch() + 1); }
        if (Case == 2) { F.Controllers[0]->UnPossess(); auto* Replacement = F.World->SpawnActor<ANarrativeNPCController>(); Replacement->bAcceptNetworkThreats = true; Replacement->Possess(F.Drones[0]); }
        if (Case == 3) { F.Scanner->RelayDroneIds[0] = TEXT("ForeignDrone"); }
        F.Director->StartTestAttempt(F.Player); F.Scanner->Tick(.05f);
        TestFalse(TEXT("Expired or replaced context cannot deliver the cached alarm"), F.Scanner->HasPendingObservation());
        FNarrativeThreatMemory Memory;
        TestFalse(TEXT("Unchanged peer receives no partial stale delivery"), F.Controllers[1]->GetBestThreatMemory(F.Player, Memory));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionScannerSweepTest, "ProjectVelkorran.Campaign.Aurelion.Scanner.AuthoredSweepMovesActualLightCone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovAurelionScannerSweepTest::RunTest(const FString& Parameters)
{
    FScannerWorld F; if (!TestTrue(TEXT("Ready sweep fixture"), F.bReady)) { return false; }
    F.Player->SetActorLocation(FVector(-700.,0.,200.)); F.Scanner->SweepHalfArc = 55.f; F.Scanner->SweepPeriod = 6.f;
    F.Scanner->Tick(.05f); const FVector Initial = F.Scanner->Cone->GetForwardVector();
    F.Advance(1.5f); F.Scanner->Tick(.05f);
    TestFalse(TEXT("The actual native light/sensor axis sweeps across the corridor"), F.Scanner->Cone->GetForwardVector().Equals(Initial, .01));
    TestTrue(TEXT("Quarter-period reaches authored sweep limit"), FMath::IsNearlyEqual(F.Scanner->Cone->GetRelativeRotation().Yaw, 55., .1));
    TestFalse(TEXT("A sweep never observes a player behind the sensor"), F.Scanner->HasPendingObservation());
    return true;
}
#endif
