// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNarrativeRuntimeTestFixtures.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Narrative/SovCampaignNarrativeAdapters.h"
#include "Narrative/SovNarrativeValidationLibrary.h"
#include "Narrative/SovViewmakerLibrary.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "Tales/Dialogue.h"
#include "Tales/DialogueSM.h"

#if WITH_AUTOMATION_TESTS
struct FSovNarrativeStateTestAccess
{
    static void AlterWitness(USovCampaignStateComponent& State) { State.Journal[0].Consequences[0].Definition.WitnessIds.Add(TEXT("Forged")); }
    static void ChangeEvidenceStage(USovCampaignStateComponent& State, ESovEvidenceStage Stage) { State.Evidence[0].Stage = Stage; }
};
namespace
{
    struct FNarrativeWorld
    {
        UWorld* World = nullptr;
        ASovCampaignRuntimeTestController* PC = nullptr;
        ASovNarrativeRuntimeTestPawn* Pawn = nullptr;
        FNarrativeWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            if (!World) { return; }
            if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
            World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
                .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
            PC = World->SpawnActor<ASovNarrativeRuntimeTestController>();
            Pawn = World->SpawnActor<ASovNarrativeRuntimeTestPawn>();
            if (PC && Pawn) { PC->Possess(Pawn); PC->SetViewTarget(Pawn); PC->SetControlRotation(FRotator::ZeroRotator); }
        }
        ~FNarrativeWorld()
        { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
        USovCampaignDefinition* Mission(FName Id)
        {
            auto* Result = NewObject<USovCampaignDefinition>(PC);
            auto* Definition = NewObject<UPlayerDefinition>(PC);
            PC->KeepAlive.Add(Result); PC->KeepAlive.Add(Definition);
            Result->MissionId = Id; Result->Protagonist = Pawn->TestHero;
            Result->PawnClass = ASovNarrativeRuntimeTestPawn::StaticClass(); Result->PlayerDefinition = Definition;
            FSovCampaignBeatDefinition Beat; Beat.BeatId = TEXT("Observe"); Result->Beats.Add(Beat);
            return Result;
        }
        USovEvidenceDefinition* Evidence(FName Id, FName MissionId)
        {
            auto* Result = NewObject<USovEvidenceDefinition>(PC); PC->KeepAlive.Add(Result);
            Result->EvidenceId = Id; Result->CanonicalContentId = Id; Result->Summary = FText::FromString(TEXT("A bounded record."));
            Result->OriginalCustodian = TEXT("Archive");
            Result->SourceCustodians = {TEXT("Archive"), TEXT("Independent"), TEXT("Authority"), TEXT("Selene")};
            Result->AuthenticationAuthorities = {TEXT("Authority")}; Result->RelevantMissions = {MissionId};
            Result->CustodianInstitutions.Add(TEXT("Archive"), TEXT("Dominion"));
            Result->CustodianInstitutions.Add(TEXT("Selene"), TEXT("Reformation"));
            return Result;
        }
        USovEvidenceSourceComponent* Source(USovEvidenceDefinition* Definition, FName Custodian = TEXT("Archive"))
        {
            auto* Actor = World->SpawnActor<AActor>();
            auto* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(5)); Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); Box->RegisterComponent();
            Actor->SetActorLocation(FVector(100, 0, 0));
            auto* Result = NewObject<USovEvidenceSourceComponent>(Actor); Actor->AddInstanceComponent(Result); Result->RegisterComponent();
            Result->Definition = Definition; Result->EvidenceId = Definition->EvidenceId; Result->SourceId = FGuid::NewGuid();
            Result->AcquisitionMission = PC->State->GetActiveMission()->MissionId; Result->SourceLocationId = TEXT("ArchiveRoom");
            Result->CustodianId = Custodian; Result->AllowedProtagonists.AddTag(Pawn->TestHero);
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEvidenceProgressionRuntimeTest, "ProjectVelkorran.Campaign.Narrative.EvidenceProvenanceAndDistribution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEvidenceProgressionRuntimeTest::RunTest(const FString& Parameters)
{
    FNarrativeWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture unavailable")); return false; }
    auto* State = F.PC->State.Get(); auto* Mission = F.Mission(TEXT("EvidenceMission"));
    TestEqual(TEXT("Begin"), State->BeginMission(Mission), ESovCampaignResult::Applied);
    auto* Definition = F.Evidence(TEXT("Wafer"), Mission->MissionId);
    Definition->SupportingEvidenceIds = {TEXT("IndependentLog")}; Definition->CopyDestinations = {TEXT("Selene")};
    auto* Observed = F.Source(Definition);
    TestTrue(TEXT("Physical observation accepted"), Observed->TryAcquire(F.PC));
    auto* Authenticated = F.Source(Definition, TEXT("Authority")); Authenticated->RequestedStage = ESovEvidenceStage::Authenticated;
    TestFalse(TEXT("Authentication cannot skip questioning and corroboration"), Authenticated->TryAcquire(F.PC));
    Observed->RequestedStage = ESovEvidenceStage::Questioned;
    TestTrue(TEXT("Recognized inconsistency advances one step"), Observed->TryAcquire(F.PC));
    auto* Corroborated = F.Source(Definition, TEXT("Independent"));
    Corroborated->RequestedStage = ESovEvidenceStage::Corroborated; Corroborated->SupportingEvidenceId = TEXT("IndependentLog");
    TestFalse(TEXT("Unseen support cannot prove corroboration"), Corroborated->TryAcquire(F.PC));
    auto* Support = F.Source(F.Evidence(TEXT("IndependentLog"), Mission->MissionId));
    TestTrue(TEXT("Independent supporting record acquired"), Support->TryAcquire(F.PC));
    TestTrue(TEXT("Distinct source and custodian corroborate"), Corroborated->TryAcquire(F.PC));
    TestTrue(TEXT("Allowed authority authenticates"), Authenticated->TryAcquire(F.PC));
    auto* Copy = F.Source(Definition, TEXT("Selene")); Copy->RequestedStage = ESovEvidenceStage::Distributed; Copy->CopyDestination = TEXT("Selene");
    TestTrue(TEXT("Independent recipient records distributed copy"), Copy->TryAcquire(F.PC));
    const int32 Acquisitions = State->GetEvidence().Num();
    TestTrue(TEXT("Repeat copy is idempotent"), Copy->TryAcquire(F.PC));
    TestEqual(TEXT("No duplicate copy acquisition"), State->GetEvidence().Num(), Acquisitions);
    TestTrue(TEXT("Copy recipient now knows the evidence"), State->ObserverKnowsEvidence(TEXT("Wafer"), TEXT("Selene")));
    TestFalse(TEXT("Uninformed observer does not know private record"), State->ObserverKnowsEvidence(TEXT("Wafer"), TEXT("Lyessa")));
    TestEqual(TEXT("Current reader sees final stage"), State->GetEvidenceStage(TEXT("Wafer"), F.Pawn->TestHero), ESovEvidenceStage::Distributed);
    State->Load_Implementation(); TestTrue(TEXT("All five provenance stages replay"), State->IsStateValid());
    FString Error; Definition->CustodianInstitutions[TEXT("Selene")] = TEXT("Dominion");
    TestFalse(TEXT("Copy within the original institution is not distribution"), Definition->ValidateDefinition(Error));
    Definition->CustodianInstitutions[TEXT("Selene")] = TEXT("Reformation");
    FSovNarrativeStateTestAccess::ChangeEvidenceStage(*State, ESovEvidenceStage::Authenticated);
    State->Load_Implementation(); TestFalse(TEXT("Tampered stage order rejects the snapshot"), State->IsStateValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovConsequenceMemoryRuntimeTest, "ProjectVelkorran.Campaign.Narrative.ConsequencesMemoriesAndCriticalEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovConsequenceMemoryRuntimeTest::RunTest(const FString& Parameters)
{
    FNarrativeWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture unavailable")); return false; }
    const auto& Tags = FSovGameplayTags::Get(); auto* State = F.PC->State.Get(); auto* Mission = F.Mission(TEXT("WitnessMission"));
    FSovConsequenceDefinition Fact; Fact.ConsequenceId = TEXT("Consequence.Witness.Protected");
    Fact.SubjectIds = {TEXT("Lyessa")}; Fact.WitnessIds = {TEXT("Lyessa")}; Fact.ChoiceTag = Tags.Event_Guard_Blocked;
    Fact.OutcomeTag = Tags.Echo_Source_PerfectGuard; Fact.bArchivalOnly = true;
    Mission->Beats[0].Consequences.Add(Fact);
    FSovRelationshipMemoryDefinition Memory; Memory.MemoryId = TEXT("Memory.Lyessa.Protected"); Memory.HolderId = TEXT("Lyessa");
    Memory.SubjectId = TEXT("Tarrik"); Memory.ConsequenceId = Fact.ConsequenceId; Memory.Type = ESovRelationshipMemoryType::Protection;
    Mission->Beats[0].RelationshipMemories.Add(Memory);
    Memory.MemoryId = TEXT("Memory.Tarrik.Trusted"); Memory.HolderId = TEXT("Tarrik"); Memory.SubjectId = TEXT("Lyessa"); Memory.Type = ESovRelationshipMemoryType::TrustGiven;
    Mission->Beats[0].RelationshipMemories.Add(Memory);
    auto* Critical = F.Evidence(TEXT("CriticalOrders"), Mission->MissionId); Critical->bCriticalPath = true;
    Mission->Beats[0].CriticalEvidence.Add(Critical);
    TestEqual(TEXT("Begin authored mission"), State->BeginMission(Mission), ESovCampaignResult::Applied);
    auto* OptionalWorldSource = F.Source(Critical);
    TestFalse(TEXT("Optional search cannot be the required evidence acquisition"), OptionalWorldSource->TryAcquire(F.PC));
    TestEqual(TEXT("Mandatory beat commits evidence and memories atomically"), State->CompleteBeat(TEXT("Observe")), ESovCampaignResult::Applied);
    TestTrue(TEXT("Critical path guarantees the record"), State->KnowsEvidence(Critical->EvidenceId, F.Pawn->TestHero));
    FSovConsequenceRecord Record;
    TestTrue(TEXT("Actual witness knows consequence"), State->FindConsequence(Fact.ConsequenceId, TEXT("Lyessa"), Record));
    TestFalse(TEXT("Absent observer does not know private consequence"), State->FindConsequence(Fact.ConsequenceId, TEXT("Selene"), Record));
    TestTrue(TEXT("Failure clears previously returned record"), Record.Definition.ConsequenceId.IsNone());
    TestTrue(TEXT("Lyessa remembers protection"), State->HasRelationshipMemory(TEXT("Lyessa"), TEXT("Tarrik"), ESovRelationshipMemoryType::Protection));
    TestTrue(TEXT("Tarrik remembers his different interpretation"), State->HasRelationshipMemory(TEXT("Tarrik"), TEXT("Lyessa"), ESovRelationshipMemoryType::TrustGiven));
    TestFalse(TEXT("Memories are not automatically symmetrical"), State->HasRelationshipMemory(TEXT("Tarrik"), TEXT("Lyessa"), ESovRelationshipMemoryType::Protection));
    State->Load_Implementation(); TestTrue(TEXT("Authored consequence/memory/evidence replay validates"), State->IsStateValid());
    FSovNarrativeStateTestAccess::AlterWitness(*State); State->Load_Implementation();
    TestFalse(TEXT("Forged witness list fails restore"), State->IsStateValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInheritedNarrativeRuntimeTest, "ProjectVelkorran.Campaign.Narrative.InheritedMemoriesAndWitnessKnowledge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInheritedNarrativeRuntimeTest::RunTest(const FString& Parameters)
{
    FNarrativeWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture unavailable")); return false; }
    auto* State = F.PC->State.Get(); const auto& Tags = FSovGameplayTags::Get();
    auto* First = F.Mission(TEXT("FirstWitness")); auto* Second = F.Mission(TEXT("LaterWitness"));
    Second->Protagonist = Tags.Character_Player_Selene; First->AllowedSuccessorMissions.Add(Second->MissionId);
    FSovConsequenceDefinition Fact; Fact.ConsequenceId = TEXT("Consequence.First.Shared"); Fact.SubjectIds = {TEXT("Selene")};
    Fact.ChoiceTag = Tags.Event_Guard_Blocked; Fact.OutcomeTag = Tags.Echo_Source_PerfectGuard; Fact.bArchivalOnly = true;
    First->Beats[0].Consequences.Add(Fact); Second->RequiredPriorConsequenceIds.Add(Fact.ConsequenceId);
    FSovRelationshipMemoryDefinition Memory; Memory.MemoryId = TEXT("Memory.Later.Learned"); Memory.HolderId = TEXT("Selene");
    Memory.SubjectId = TEXT("Tarrik"); Memory.ConsequenceId = Fact.ConsequenceId; Memory.LearnedThrough = ESovKnowledgeMethod::Told;
    Second->Beats[0].RelationshipMemories.Add(Memory);
    auto* Critical = F.Evidence(TEXT("SharedOrders"), First->MissionId); Critical->RelevantMissions.Add(Second->MissionId); Critical->bCriticalPath = true;
    First->Beats[0].CriticalEvidence.Add(Critical); Second->Beats[0].CriticalEvidence.Add(Critical);
    TestFalse(TEXT("Cannot enter a mission before its inherited fact exists"), State->CanEnterMission(Second));
    State->BeginMission(First); State->CompleteBeat(TEXT("Observe"));
    auto* Discussion = F.Source(Critical); Discussion->RequestedStage = ESovEvidenceStage::Questioned;
    Discussion->WitnessIds.Add(TEXT("Selene"));
    TestTrue(TEXT("Physical discussion tells the named witness"), Discussion->TryAcquire(F.PC));
    TestTrue(TEXT("Witness knows the required record before taking control"), State->KnowsEvidence(Critical->EvidenceId, Tags.Character_Player_Selene));
    F.Pawn->TestHero = Tags.Character_Player_Selene;
    TestEqual(TEXT("Later protagonist enters with guaranteed inherited fact"), State->BeginMission(Second), ESovCampaignResult::Applied);
    const int32 Acquisitions = State->GetEvidence().Num();
    TestEqual(TEXT("Later memory commits against earlier mission journal"), State->CompleteBeat(TEXT("Observe")), ESovCampaignResult::Applied);
    TestEqual(TEXT("Already-witnessed critical record is not duplicated"), State->GetEvidence().Num(), Acquisitions);
    State->Load_Implementation();
    TestTrue(TEXT("Cross-mission memories and witnessed critical evidence survive replay"), State->IsStateValid());
    FSovConsequenceRecord Record;
    TestTrue(TEXT("Told memory grants consequence knowledge without manufacturing a witness"), State->FindConsequence(Fact.ConsequenceId, TEXT("Selene"), Record));
    TestFalse(TEXT("Original witness list remains immutable"), Record.Definition.WitnessIds.Contains(TEXT("Selene")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovViewmakerRuntimeTest, "ProjectVelkorran.Campaign.Narrative.ViewmakerPhysicalBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovViewmakerRuntimeTest::RunTest(const FString& Parameters)
{
    FNarrativeWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture unavailable")); return false; }
    F.Pawn->TestHero = FSovGameplayTags::Get().Character_Player_Selene;
    auto* Mission = F.Mission(TEXT("ScanMission")); F.PC->State->BeginMission(Mission);
    auto* Source = F.Source(F.Evidence(TEXT("Trace"), Mission->MissionId)); Source->bRequiresViewmaker = true;
    Source->AuthoredTraceIds.Add(TEXT("RecentDeviceUse"));
    FSovViewmakerScanResult Result;
    TestFalse(TEXT("Ordinary interaction cannot bypass the viewmaker"), Source->TryAcquire(F.PC));
    TestFalse(TEXT("Unready avatar cannot scan"), USovViewmakerLibrary::ScanTarget(F.PC, Source->GetOwner(), Result));
    F.Pawn->SetTestReady(true);
    TestTrue(TEXT("Ready Selene can inspect visible physical trace"), USovViewmakerLibrary::ScanTarget(F.PC, Source->GetOwner(), Result));
    TestEqual(TEXT("Authored trace returned"), Result.AuthoredTraceIds.Num(), 1);
    Source->GetOwner()->SetActorLocation(FVector(-100, 0, 0));
    TestFalse(TEXT("Behind-camera record rejected"), USovViewmakerLibrary::ScanTarget(F.PC, Source->GetOwner(), Result));
    TestNull(TEXT("Failed scan clears stale target"), Result.Target.Get());
    Source->GetOwner()->SetActorLocation(FVector(100, 0, 0));
    auto* Wall = F.World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(5, 100, 200));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block); Box->RegisterComponent();
    Wall->SetActorLocation(FVector(50, 0, 0));
    TestFalse(TEXT("Opaque wall blocks the reconstruction layer"), USovViewmakerLibrary::ScanTarget(F.PC, Source->GetOwner(), Result));
    Wall->Destroy(); F.Pawn->bTestAlive = false;
    TestFalse(TEXT("Fatal avatar cannot scan"), USovViewmakerLibrary::ScanTarget(F.PC, Source->GetOwner(), Result));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDialogueValidationRuntimeTest, "ProjectVelkorran.Campaign.Narrative.ExistingGraphValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDialogueValidationRuntimeTest::RunTest(const FString& Parameters)
{
    auto* Dialogue = NewObject<UDialogue>(); auto* Root = NewObject<UDialogueNode_NPC>(Dialogue); auto* Exit = NewObject<UDialogueNode_Player>(Dialogue);
    Dialogue->RootDialogue = Root; Dialogue->NPCReplies.Add(Root); Dialogue->PlayerReplies.Add(Exit);
    Root->SetID(TEXT("Root")); Exit->SetID(TEXT("Exit")); Root->PlayerReplies.Add(Exit);
    TArray<FString> Errors, Warnings;
    TestTrue(TEXT("Existing Narrative graph with legal exit validates"), USovNarrativeValidationLibrary::ValidateDialogue(Dialogue, nullptr, false, Errors, Warnings));
    Exit->NPCReplies.Add(Root);
    TestFalse(TEXT("Cycle without an exit is rejected"), USovNarrativeValidationLibrary::ValidateDialogue(Dialogue, nullptr, false, Errors, Warnings));
    Exit->NPCReplies.Reset();
    auto* Condition = NewObject<USovCampaignNarrativeCondition>(Exit); Condition->Query = ESovCampaignQuery::EvidenceKnownBy; Condition->RecordId = TEXT("UnknownRecord");
    Exit->Conditions.Add(Condition);
    TestFalse(TEXT("All-conditional choices require a guaranteed fallback"), USovNarrativeValidationLibrary::ValidateDialogue(Dialogue, nullptr, false, Errors, Warnings));
    Exit->Conditions.Reset(); Exit->Line.Duration = ELineDuration::LD_Never; Exit->bIsSkippable = false;
    TestFalse(TEXT("Infinite unskippable line is rejected"), USovNarrativeValidationLibrary::ValidateDialogue(Dialogue, nullptr, false, Errors, Warnings));
    return true;
}
#endif
