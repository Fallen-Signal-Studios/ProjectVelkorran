// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovAurelionPrioritySupport.h"
#include "Campaign/SovAurelionCheckpoint.h"
#include "Campaign/SovAurelionMedicalCache.h"
#include "Campaign/SovAurelionPriorityTerminal.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "Misc/AutomationTest.h"
#include "TimerManager.h"
#include "UObject/Script.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_AUTOMATION_TESTS
namespace SovAurelionPriorityTests
{
    struct FWorld
    {
        FEditorScriptExecutionGuard ScriptGuard;
        UWorld* World = nullptr;
        ASovHandoffRuntimeTestController* PC = nullptr;
        ASovHandoffRuntimeTestPawn* Player = nullptr;
        USovCampaignStateComponent* State = nullptr;
        ASovAurelionPrioritySupport* Support = nullptr;
        uint64 Frame = GFrameCounter;
        FWorld()
        {
            const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeActorsForPlay(FURL()); World->GetTimerManager().Tick(0.f);
            PC = World->SpawnActor<ASovHandoffRuntimeTestController>(); Player = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
            auto* PS = World->SpawnActor<ASovPlayerState>();
            if (!PC || !Player || !PS) { return; }
            World->AddController(PC); auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
            Player->PrepareCampaignInitialization(Definition); PC->SetTestPlayerState(PS); PC->Possess(Player);
            if (!Player->StageTestReadiness(PS, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
            auto* Mission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(Mission);
            Mission->MissionId = TEXT("M12_FireAndFrost"); Mission->Protagonist = Player->GetProtagonistIdentityTag();
            Mission->PawnClass = ASovHandoffRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition = Definition;
            auto& Arrival = Mission->Beats.AddDefaulted_GetRef(); Arrival.BeatId = TEXT("TarrikArrival");
            Arrival.ObjectiveText = FText::FromString(TEXT("Arrival"));
            FSovCampaignChoiceGroup Group; Group.GroupId = TEXT("ImmediateProtection"); Group.ReconciliationBeatId = TEXT("LocalPriorityCommitted");
            Group.ReconciliationNote = FText::FromString(TEXT("Both groups reach safety")); Mission->ChoiceGroups.Add(Group);
            for (auto Priority : { ESovAurelionRescuePriority::WestStretchers, ESovAurelionRescuePriority::EastWalkers })
            {
                auto& Beat = Mission->Beats.AddDefaulted_GetRef(); Beat.BeatId = ASovAurelionPrioritySupport::OutcomeBeat(Priority);
                Beat.ObjectiveText = FText::FromString(TEXT("Select priority")); Beat.RequiredProtagonist = Mission->Protagonist;
                Beat.bOptional = true; Beat.bInteractiveChoice = true; Beat.ChoiceGroupId = Group.GroupId;
            }
            auto& Rejoin = Mission->Beats.AddDefaulted_GetRef(); Rejoin.BeatId = Group.ReconciliationBeatId;
            Rejoin.ObjectiveText = FText::FromString(TEXT("Priority acknowledged")); Rejoin.RequiredProtagonist = Mission->Protagonist;
            Rejoin.RequiredChoiceGroups.Add(Group.GroupId);
            // This fixture supplies a progression boundary only. Production handoff proof is tested separately.
            auto& PhaseA = Mission->Beats.AddDefaulted_GetRef(); PhaseA.BeatId = TEXT("SeverCrucibleLinks");
            PhaseA.PrerequisiteBeats.Add(TEXT("LocalPriorityCommitted")); PhaseA.ObjectiveText = FText::FromString(TEXT("Phase A"));
            auto& PhaseB = Mission->Beats.AddDefaulted_GetRef(); PhaseB.BeatId = TEXT("HandoffToTarrikCrucible");
            PhaseB.PrerequisiteBeats.Add(TEXT("SeverCrucibleLinks")); PhaseB.ObjectiveText = FText::FromString(TEXT("Phase B"));
            State = PC->GetCampaignState();
            if (State->BeginMission(Mission) != ESovCampaignResult::Applied) { State = nullptr; return; }
            Support = World->SpawnActor<ASovAurelionPrioritySupport>(); Support->SetActorLocation(FVector(3000, 0, 0));
        }
        ~FWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        ASovAurelionPriorityTerminal* Terminal(ESovAurelionRescuePriority Priority, FVector Offset)
        {
            auto* Result = World->SpawnActor<ASovAurelionPriorityTerminal>();
            Result->Priority = Priority; Result->SetActorLocation(Player->GetActorLocation() + Offset); return Result;
        }
        void NextFrame() { TGuardValue<uint64> Scoped(GFrameCounter, ++Frame); World->GetTimerManager().Tick(.016f); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionPriorityProjectionTest,
    "ProjectVelkorran.Campaign.AurelionPriority.ExclusiveSupportAndRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionPriorityProjectionTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionPriorityTests;
    for (auto Priority : { ESovAurelionRescuePriority::WestStretchers, ESovAurelionRescuePriority::EastWalkers })
    {
        FWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.State)) { return false; }
        FNarrativeSaveComponent Before, Selected;
        TestTrue(TEXT("Capture unset priority"), USovEncounterSnapshotLibrary::CaptureComponent(F.State, Before));
        F.Support->RefreshFromCampaign();
        TestFalse(TEXT("No cache before decision"), F.Support->IsWestCacheAccessible());
        TestFalse(TEXT("No early flank before decision"), F.Support->IsEastFlankOpen());
        const FName Outcome = ASovAurelionPrioritySupport::OutcomeBeat(Priority);
        TestEqual(TEXT("Existing campaign choice owns the result"), F.State->ResolveChoice(TEXT("ImmediateProtection"), Outcome), ESovCampaignResult::Applied);
        F.Support->RefreshFromCampaign();
        const bool bWest = Priority == ESovAurelionRescuePriority::WestStretchers;
        TestEqual(TEXT("Only west selection enables cache access"), F.Support->IsWestCacheAccessible(), bWest);
        TestEqual(TEXT("Only east selection enables the early flank"), F.Support->IsEastFlankOpen(), !bWest);
        TestEqual(TEXT("West access has physical collision consequence"), F.Support->WestCacheBarrier->GetCollisionEnabled(),
            bWest ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
        TestEqual(TEXT("East access has physical collision consequence"), F.Support->EastFlankBarrier->GetCollisionEnabled(),
            bWest ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        TestEqual(TEXT("Aftermath selects the same outcome"), F.Support->GetAftermathConsequenceId(),
            FName(bWest ? TEXT("M12_WestStretchersPrioritized") : TEXT("M12_EastWalkersPrioritized")));
        const int32 Count = F.State->GetJournal().Num(); F.Support->RefreshFromCampaign(); F.Support->RefreshFromCampaign();
        TestEqual(TEXT("Projection creates no reward or journal event"), F.State->GetJournal().Num(), Count);
        TestTrue(TEXT("Capture selected priority"), USovEncounterSnapshotLibrary::CaptureComponent(F.State, Selected));
        TestTrue(TEXT("Load prechoice state"), USovEncounterSnapshotLibrary::RestoreComponent(F.State, Before));
        F.Support->RefreshFromCampaign();
        TestEqual(TEXT("Reload does not retain stale choice"), F.Support->GetPriority(), ESovAurelionRescuePriority::Unset);
        TestFalse(TEXT("Reload closes stale access"), F.Support->IsWestCacheAccessible() || F.Support->IsEastFlankOpen());
        TestTrue(TEXT("Reload selected state"), USovEncounterSnapshotLibrary::RestoreComponent(F.State, Selected));
        F.Support->RefreshFromCampaign(); TestEqual(TEXT("Same choice survives reload"), F.Support->GetPriority(), Priority);
        const FName Other = ASovAurelionPrioritySupport::OutcomeBeat(bWest ? ESovAurelionRescuePriority::EastWalkers : ESovAurelionRescuePriority::WestStretchers);
        TestTrue(TEXT("Opposite benefit cannot be selected afterward"), F.State->ResolveChoice(TEXT("ImmediateProtection"), Other) != ESovCampaignResult::Applied);
        TestEqual(TEXT("Acknowledge selected order"), F.State->CompleteBeat(TEXT("LocalPriorityCommitted")), ESovCampaignResult::Applied);
        TestEqual(TEXT("Complete fixture phase A"), F.State->CompleteBeat(TEXT("SeverCrucibleLinks")), ESovCampaignResult::Applied);
        TestEqual(TEXT("Enter fixture phase B"), F.State->CompleteBeat(TEXT("HandoffToTarrikCrucible")), ESovCampaignResult::Applied);
        F.Support->RefreshFromCampaign();
        TestTrue(TEXT("Normal east route opens at phase B for either choice"), F.Support->IsEastFlankOpen());
        TestEqual(TEXT("Phase transition never adds the unselected cache"), F.Support->IsWestCacheAccessible(), bWest);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionPriorityAdmissionTest,
    "ProjectVelkorran.Campaign.AurelionPriority.NativePanelNeedsBothOptionsAndCheckpoint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionPriorityAdmissionTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionPriorityTests;
    FWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.State)) { return false; }
    auto* West = F.Terminal(ESovAurelionRescuePriority::WestStretchers, FVector(180, -80, 0)); FText Error;
    TestFalse(TEXT("A single forced option is rejected"), West->CanUse(F.Player, Error));
    auto* East = F.Terminal(ESovAurelionRescuePriority::EastWalkers, FVector(180, 80, 0));
    East->Interactable->Deactivate();
    TestFalse(TEXT("A disabled alternate option cannot count as a choice"), West->CanUse(F.Player, Error));
    East->Interactable->Activate();
    East->Interactable->InteractionDistance = 0.f;
    TestFalse(TEXT("An unusable alternate range cannot force the other option"), West->CanUse(F.Player, Error));
    East->Interactable->InteractionDistance = 250.f;
    auto* Duplicate = F.World->SpawnActor<ASovAurelionPrioritySupport>();
    TestFalse(TEXT("Multiple support payoffs cannot be awarded"), West->CanUse(F.Player, Error));
    Duplicate->Destroy();
    if (!TestTrue(TEXT("Two real close options admit selection"), West->CanUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
    TestTrue(TEXT("Native interaction queues the request"), West->RequestUse(F.Player, Error));
    TestFalse(TEXT("Other option cannot race a pending choice"), East->RequestUse(F.Player, Error));
    F.NextFrame();
    TestTrue(TEXT("No GameInstance/storage means no choice publication"), F.State->GetSelectedChoice(TEXT("M12_FireAndFrost"), TEXT("ImmediateProtection")).IsNone());
    TestFalse(TEXT("Failed save clears request reservation"), West->IsRequestPending());
    TestEqual(TEXT("Select through native campaign owner"), F.State->ResolveChoice(TEXT("ImmediateProtection"), TEXT("PriorityWestStretchers")), ESovCampaignResult::Applied);
    TestEqual(TEXT("Commit fixture priority"), F.State->CompleteBeat(TEXT("LocalPriorityCommitted")), ESovCampaignResult::Applied);
    TestTrue(TEXT("Same selection can retry CP5 before E4"), West->CanUse(F.Player, Error));
    TestEqual(TEXT("Complete fixture link phase"), F.State->CompleteBeat(TEXT("SeverCrucibleLinks")), ESovCampaignResult::Applied);
    TestFalse(TEXT("Completed E4 phase cannot overwrite CP5 with later progress"), West->CanUse(F.Player, Error));
    F.Support->Destroy();
    TestFalse(TEXT("Missing support cannot accept a cosmetic choice"), West->CanUse(F.Player, Error));
    West->SetActorLocation(F.Player->GetActorLocation() + FVector(1000, 0, 0));
    TestFalse(TEXT("Remote choice requests rejected"), West->RequestUse(F.Player, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionPriorityLoadRetirementTest,
    "ProjectVelkorran.Campaign.AurelionPriority.LoadRetiresQueuedSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionPriorityLoadRetirementTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionPriorityTests;
    FWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.State)) { return false; }
    auto* West = F.Terminal(ESovAurelionRescuePriority::WestStretchers, FVector(180, -80, 0));
    F.Terminal(ESovAurelionRescuePriority::EastWalkers, FVector(180, 80, 0));
    FNarrativeSaveComponent Before; TestTrue(TEXT("Capture original state"), USovEncounterSnapshotLibrary::CaptureComponent(F.State, Before));
    FText Error; if (!TestTrue(TEXT("Queue selection"), West->RequestUse(F.Player, Error))) { AddError(Error.ToString()); return false; }
    TestTrue(TEXT("Restore same journal through production restore"), USovEncounterSnapshotLibrary::RestoreComponent(F.State, Before));
    TestFalse(TEXT("Load retires the queued request even with equal journal size"), West->IsRequestPending()); F.NextFrame();
    TestEqual(TEXT("Retired request did not write a fact"), F.State->GetJournal().Num(), 0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionCheckpointBoundaryTest,
    "ProjectVelkorran.Campaign.AurelionCheckpoint.ExactProgressAndNoInventedReceipt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionCheckpointBoundaryTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionPriorityTests;
    FWorld F; if (!TestNotNull(TEXT("Ready campaign"), F.State)) { return false; }
    TestTrue(TEXT("Context boundary precedes arrival"), ASovAurelionCheckpoint::MatchesProgress(ESovAurelionCheckpoint::ContextCP0, F.State));
    TestFalse(TEXT("Missing downstream beat cannot qualify checkpoint"), ASovAurelionCheckpoint::MatchesProgress(ESovAurelionCheckpoint::MeetingCP3, F.State));
    TestFalse(TEXT("Another mission's checkpoint cannot match"), ASovAurelionCheckpoint::MatchesProgress(ESovAurelionCheckpoint::CoreCP7, F.State));
    TestFalse(TEXT("Unknown boundary cannot match"), ASovAurelionCheckpoint::MatchesProgress(static_cast<ESovAurelionCheckpoint>(255), F.State));
    auto* Marker = F.World->SpawnActor<ASovAurelionCheckpoint>(); Marker->SetActorLocation(F.Player->GetActorLocation());
    FString Error;
    TestFalse(TEXT("No storage owner never reports checkpoint success"), Marker->RequestCheckpoint(F.PC, Error));
    TestEqual(TEXT("Checkpoint capture never completes a beat"), F.State->GetJournal().Num(), 0);
    TestEqual(TEXT("Advance through real state boundary"), F.State->CompleteBeat(TEXT("TarrikArrival")), ESovCampaignResult::Applied);
    TestFalse(TEXT("Old checkpoint cannot label later progress as prearrival"), ASovAurelionCheckpoint::MatchesProgress(ESovAurelionCheckpoint::ContextCP0, F.State));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionMedicalCacheOnceTest,
    "ProjectVelkorran.Campaign.AurelionPriority.MedicalCacheRealHealAndSavedConsumption", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionMedicalCacheOnceTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionPriorityTests;
    FWorld F; if (!TestNotNull(TEXT("Ready campaign"),F.State)) { return false; }
    auto* Cache=F.World->SpawnActor<ASovAurelionMedicalCache>(); Cache->CacheId=TEXT("WestCache"); Cache->Support=F.Support;
    Cache->SetActorLocation(F.Player->GetActorLocation()+FVector(170,0,0)); Cache->Interactable->Activate();
    auto* ASC=F.Player->GetNarrativeAbilitySystemComponent();
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(),100);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),20);
    FText Error;
    TestFalse(TEXT("No medical grant before the real choice"),Cache->TryUse(F.Player,Error));
    TestEqual(TEXT("Native choice owns west access"),F.State->ResolveChoice(TEXT("ImmediateProtection"),TEXT("PriorityWestStretchers")),ESovCampaignResult::Applied);
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),100);
    TestFalse(TEXT("Full health preserves the single use"),Cache->TryUse(F.Player,Error));
    TestFalse(TEXT("Full-health rejection does not consume"),Cache->IsConsumed());
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),20);
    bool bReentrantRejected=false, bSaveRejected=false;
    const FDelegateHandle Watch=ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).AddLambda(
        [&](const FOnAttributeChangeData& Change)
        {
            FText RetryError; bReentrantRejected=!Cache->TryUse(F.Player,RetryError);
            TArray<uint8> MidBytes; FMemoryWriter Writer(MidBytes); FObjectAndNameAsStringProxyArchive Ar(Writer,false); Ar.ArIsSaveGame=true;
            Cache->Serialize(Ar); bSaveRejected=Ar.IsError();
        });
    const bool bGranted=Cache->TryUse(F.Player,Error);
    ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(Watch);
    if (!TestTrue(TEXT("Actual instant GAS medical effect grants"),bGranted)) { AddError(Error.ToString()); return false; }
    TestEqual(TEXT("One aid adds 35 percent of maximum health"),ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),55.f);
    TestTrue(TEXT("Synchronous healing cannot reenter the cache"),bReentrantRejected);
    TestTrue(TEXT("A half-committed medical record cannot be saved"),bSaveRejected);
    TestTrue(TEXT("Consumption remains spent"),Cache->IsConsumed());
    TArray<uint8> Bytes; { FMemoryWriter Writer(Bytes); FObjectAndNameAsStringProxyArchive Ar(Writer,false); Ar.ArIsSaveGame=true; Cache->Serialize(Ar); TestFalse(TEXT("Settled cache record saves"),Ar.IsError()); }
    auto* Reloaded=F.World->SpawnActor<ASovAurelionMedicalCache>(); Reloaded->CacheId=Cache->CacheId; Reloaded->Support=F.Support;
    Reloaded->SetActorLocation(Cache->GetActorLocation()); Cache->Destroy(); Reloaded->Interactable->Activate();
    { FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Ar(Reader,true); Ar.ArIsSaveGame=true; Reloaded->Serialize(Ar); TestFalse(TEXT("Consumption record deserializes"),Ar.IsError()); }
    Reloaded->Load_Implementation();
    TestTrue(TEXT("A fresh map instance restores consumption"),Reloaded->IsConsumed());
    ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),20);
    TestFalse(TEXT("Injury after reload does not refill an exhausted cache"),Reloaded->TryUse(F.Player,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAurelionMedicalCacheAccessTest,
    "ProjectVelkorran.Campaign.AurelionPriority.MedicalCacheRejectsWrongPriority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovAurelionMedicalCacheAccessTest::RunTest(const FString& Parameters)
{
    using namespace SovAurelionPriorityTests;
    FWorld F; if (!TestNotNull(TEXT("Ready campaign"),F.State)) { return false; }
    auto* Cache=F.World->SpawnActor<ASovAurelionMedicalCache>(); Cache->CacheId=TEXT("WestCache"); Cache->Support=F.Support;
    Cache->SetActorLocation(F.Player->GetActorLocation()+FVector(170,0,0)); Cache->Interactable->Activate();
    auto* ASC=F.Player->GetNarrativeAbilitySystemComponent(); ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(),20);
    FText Error;
    TestEqual(TEXT("Native east choice applies"),F.State->ResolveChoice(TEXT("ImmediateProtection"),TEXT("PriorityEastWalkers")),ESovCampaignResult::Applied);
    TestFalse(TEXT("Walking around a barrier cannot claim the unselected aid"),Cache->TryUse(F.Player,Error));
    TestFalse(TEXT("Wrong priority leaves the aid unspent"),Cache->IsConsumed());
    return true;
}
#endif
