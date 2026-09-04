// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
struct FSovCampaignStateTestAccess
{
	static void SetFirstSequence(USovCampaignStateComponent& State, int32 Sequence) { State.Journal[0].Sequence = Sequence; }
	static void SetEvidenceGuid(USovCampaignStateComponent& State, FGuid Guid) { State.Evidence[0].SourceId = Guid; }
	static void SetFact(USovCampaignStateComponent& State, FGameplayTag Key, FGameplayTag Value) { State.StateValues.Add(Key, Value); }
};
namespace
{
	struct FCampaignWorld
	{
		UWorld* World = nullptr;
		ASovCampaignRuntimeTestController* PC = nullptr;
		ASovCampaignRuntimeTestPawn* Pawn = nullptr;
		FCampaignWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			PC = World->SpawnActor<ASovCampaignRuntimeTestController>();
			Pawn = World->SpawnActor<ASovCampaignRuntimeTestPawn>();
			if (PC && Pawn)
			{
				PC->Possess(Pawn);
				PC->State->OnBeatCommitted.AddUniqueDynamic(PC, &ASovCampaignRuntimeTestController::ObserveBeat);
				PC->State->OnEvidenceRecorded.AddUniqueDynamic(PC, &ASovCampaignRuntimeTestController::ObserveEvidence);
			}
		}
		~FCampaignWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		USovCampaignDefinition* Mission(FName Id, FGameplayTag Hero, bool TwoBeats = true)
		{
			auto* Definition = NewObject<USovCampaignDefinition>(PC);
			auto* Player = NewObject<UPlayerDefinition>(PC);
			PC->KeepAlive.Add(Definition); PC->KeepAlive.Add(Player);
			Definition->MissionId = Id;
			Definition->Protagonist = Hero;
			Definition->PawnClass = ASovCampaignRuntimeTestPawn::StaticClass();
			Definition->PlayerDefinition = Player;
			FSovCampaignBeatDefinition Start; Start.BeatId = TEXT("Start");
			Start.GrantedKnowledge.AddTag(FSovGameplayTags::Get().Echo_Source_WeakPointBreak);
			FSovCampaignStateWrite Fact; Fact.Key = FSovGameplayTags::Get().State_CommandLink_Active;
			Fact.Value = FSovGameplayTags::Get().Character_Player_Tarrik; Fact.bCanonProtected = true;
			Start.StateWrites.Add(Fact);
			Definition->Beats.Add(Start);
			if (TwoBeats)
			{
				FSovCampaignBeatDefinition End; End.BeatId = TEXT("End"); End.PrerequisiteBeats.Add(TEXT("Start"));
				End.RequiredKnowledge = Start.GrantedKnowledge; End.CinematicId = FName(*(Id.ToString() + TEXT("_Ending")));
				Definition->Beats.Add(End);
			}
			return Definition;
		}
		USovEvidenceSourceComponent* Evidence(FName Id, FName MissionId, FVector Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10.f)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldDynamic); Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Location);
			auto* Source = NewObject<USovEvidenceSourceComponent>(Actor); Actor->AddInstanceComponent(Source); Source->RegisterComponent();
			Source->EvidenceId = Id; Source->SourceId = FGuid::NewGuid(); Source->AcquisitionMission = MissionId;
			Source->AllowedProtagonists.AddTag(Pawn->TestHero); Source->GrantedKnowledge.AddTag(FSovGameplayTags::Get().Echo_Source_WeakPointBreak);
			return Source;
		}
		AActor* Wall(FVector Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(10.f, 80.f, 100.f)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent(); Actor->SetActorLocation(Location); return Actor;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignDefinitionTest, "ProjectVelkorran.Campaign.Story.DefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignDefinitionTest::RunTest(const FString& Parameters)
{
	FCampaignWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* Definition = F.Mission(TEXT("MissionA"), F.Pawn->TestHero); FString Error;
	TestTrue(TEXT("Authored DAG validates"), Definition->ValidateDefinition(Error));
	auto* Mantle = NewObject<USovMantleMissionDefinition>(F.PC); F.PC->KeepAlive.Add(Mantle);
	Mantle->PlayerDefinition = Definition->PlayerDefinition;
	TestTrue(TEXT("Native Mantle schema validates with authored player definition"), Mantle->ValidateDefinition(Error));
	const auto* Heir = Mantle->FindBeat(TEXT("HeirNamed"));
	TestTrue(TEXT("Native heir fact is immutable Tarrik canon"), Heir && Heir->StateWrites.Num() == 1
		&& Heir->StateWrites[0].bCanonProtected && Heir->StateWrites[0].Value == FSovGameplayTags::Get().Campaign_Value_Tarrik);
	auto* OneDegree = NewObject<USovOneDegreeMissionDefinition>(F.PC); F.PC->KeepAlive.Add(OneDegree);
	OneDegree->PlayerDefinition = Definition->PlayerDefinition;
	TestTrue(TEXT("Native One Degree schema validates"), OneDegree->ValidateDefinition(Error));
	const auto* Shot = OneDegree->FindBeat(TEXT("PrecisionShot"));
	TestTrue(TEXT("Both perspectives preserve wounded-alive Caelus"), Shot && Shot->StateWrites.Num() == 1
		&& Shot->StateWrites[0].bCanonProtected && Shot->StateWrites[0].Value == FSovGameplayTags::Get().Campaign_Value_WoundedAlive);
	Definition->Beats[0].PrerequisiteBeats.Add(TEXT("End"));
	TestFalse(TEXT("Cycle rejected"), Definition->ValidateDefinition(Error)); Definition->Beats[0].PrerequisiteBeats.Reset();
	Definition->Beats[0].bOptional = true;
	TestFalse(TEXT("Mandatory beat cannot depend on optional progress"), Definition->ValidateDefinition(Error)); Definition->Beats[0].bOptional = false;
	auto BadFact = Definition->Beats[0].StateWrites[0]; BadFact.Value = FSovGameplayTags::Get().Character_Player_Selene;
	Definition->Beats[1].StateWrites.Add(BadFact);
	TestFalse(TEXT("Protected ancestor contradiction rejected before play"), Definition->ValidateDefinition(Error));
	Definition->Beats[1].StateWrites.Reset();
	Definition->Beats[1].PrerequisiteBeats.Add(TEXT("Start"));
	TestFalse(TEXT("Duplicate prerequisite rejected"), Definition->ValidateDefinition(Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignCommitTest, "ProjectVelkorran.Campaign.Story.CommitAndHeroKnowledge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignCommitTest::RunTest(const FString& Parameters)
{
	FCampaignWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* State = F.PC->State.Get(); const auto& Tags = FSovGameplayTags::Get();
	auto* First = F.Mission(TEXT("MissionA"), Tags.Character_Player_Tarrik);
	auto* Second = F.Mission(TEXT("MissionB"), Tags.Character_Player_Selene, false);
	Second->Beats[0].RequiredKnowledge.AddTag(Tags.Echo_Source_WeakPointBreak);
	First->AllowedSuccessorMissions.Add(Second->MissionId);
	TestEqual(TEXT("Mission starts with owned matching pawn"), State->BeginMission(First), ESovCampaignResult::Applied);
	TestEqual(TEXT("Prerequisites enforced"), State->CompleteBeat(TEXT("End")), ESovCampaignResult::PrerequisiteMissing);
	F.PC->ReentrantBeat = TEXT("End");
	TestEqual(TEXT("First beat commits"), State->CompleteBeat(TEXT("Start")), ESovCampaignResult::Applied);
	TestEqual(TEXT("Callbacks cannot interleave another commit"), F.PC->NestedResult, ESovCampaignResult::Busy);
	TestEqual(TEXT("One journal event"), State->GetJournal().Num(), 1);
	F.PC->ReentrantBeat = NAME_None;
	TestEqual(TEXT("Duplicate is idempotent"), State->CompleteBeat(TEXT("Start")), ESovCampaignResult::AlreadyApplied);
	TestEqual(TEXT("Unviewed cinematic cannot skip"), State->CompleteBeat(TEXT("End"), true), ESovCampaignResult::SkipUnavailable);
	TestTrue(TEXT("Full playback can mark eligible cinematic viewed"), State->RecordCinematicViewed(TEXT("End")));
	First->Beats[1].bInteractiveChoice = true;
	TestEqual(TEXT("Viewed interactive choice cannot skip"), State->CompleteBeat(TEXT("End"), true), ESovCampaignResult::SkipUnavailable);
	First->Beats[1].bInteractiveChoice = false;
	TestEqual(TEXT("Viewed noninteractive cinematic can skip"), State->CompleteBeat(TEXT("End"), true), ESovCampaignResult::Applied);
	TestTrue(TEXT("All mandatory beats complete mission"), State->IsMissionComplete(First->MissionId));
	TestEqual(TEXT("Mismatched pawn cannot start successor"), State->BeginMission(Second), ESovCampaignResult::Invalid);
	F.Pawn->TestHero = Tags.Character_Player_Selene;
	TestEqual(TEXT("Matching successor can begin"), State->BeginMission(Second), ESovCampaignResult::Applied);
	TestEqual(TEXT("Tarrik knowledge is not silently shared"), State->CompleteBeat(TEXT("Start")), ESovCampaignResult::KnowledgeMissing);
	auto* Evidence = F.Evidence(TEXT("Briefing"), Second->MissionId, FVector(120.f,0.f,0.f));
	TestEqual(TEXT("Native source acquisition grants Selene knowledge"), State->AcquireEvidence(Evidence), ESovCampaignResult::Applied);
	TestEqual(TEXT("Evidence unlocks successor objective"), State->CompleteBeat(TEXT("Start")), ESovCampaignResult::Applied);
	const int32 Events = F.PC->BeatEvents;
	State->PrepareForSave_Implementation(); State->Load_Implementation();
	TestTrue(TEXT("Ordered two-hero journal/evidence restore validates"), State->IsStateValid());
	TestEqual(TEXT("Restore replays no commit event"), F.PC->BeatEvents, Events);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignEvidenceTest, "ProjectVelkorran.Campaign.Story.EvidenceRangeAndVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignEvidenceTest::RunTest(const FString& Parameters)
{
	FCampaignWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* State = F.PC->State.Get(); auto* Mission = F.Mission(TEXT("MissionA"), F.Pawn->TestHero);
	State->BeginMission(Mission);
	auto* First = F.Evidence(TEXT("First"), Mission->MissionId, FVector(200.f,0.f,0.f));
	First->RequiredCompletedBeat = TEXT("Start");
	TestEqual(TEXT("Evidence prerequisite enforced"), State->AcquireEvidence(First), ESovCampaignResult::PrerequisiteMissing);
	State->CompleteBeat(TEXT("Start"));
	AActor* OwnVisual = F.Wall(FVector(60.f,0.f,0.f)); OwnVisual->AttachToActor(F.Pawn, FAttachmentTransformRules::KeepWorldTransform);
	AActor* Wall = F.Wall(FVector(100.f,0.f,0.f));
	TestEqual(TEXT("World geometry blocks evidence"), State->AcquireEvidence(First), ESovCampaignResult::Invalid);
	Wall->Destroy();
	TestEqual(TEXT("Owned attached presentation does not block evidence"), State->AcquireEvidence(First), ESovCampaignResult::Applied);
	TestEqual(TEXT("Duplicate source does not duplicate event"), State->AcquireEvidence(First), ESovCampaignResult::AlreadyApplied);
	TestEqual(TEXT("One acquisition event"), F.PC->EvidenceEvents, 1);
	auto* Other = F.Evidence(TEXT("Other"), Mission->MissionId, FVector(500.f,0.f,0.f));
	TestEqual(TEXT("Out-of-range source rejected"), State->AcquireEvidence(Other), ESovCampaignResult::Invalid);
	Other->GetOwner()->SetActorLocation(FVector(100.f,50.f,0.f)); Other->SourceId = First->SourceId;
	TestEqual(TEXT("Reused source GUID cannot claim a different artifact"), State->AcquireEvidence(Other), ESovCampaignResult::Invalid);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCampaignSaveValidationTest, "ProjectVelkorran.Campaign.Story.SavedStateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCampaignSaveValidationTest::RunTest(const FString& Parameters)
{
	FCampaignWorld F; if (!F.PC || !F.Pawn) { AddError(TEXT("Fixture failed")); return false; }
	auto* State = F.PC->State.Get(); const auto& Tags = FSovGameplayTags::Get();
	auto* Mission = F.Mission(TEXT("MissionA"), F.Pawn->TestHero); State->BeginMission(Mission); State->CompleteBeat(TEXT("Start"));
	auto* Source = F.Evidence(TEXT("Record"), Mission->MissionId, FVector(100.f,0.f,0.f));
	State->AcquireEvidence(Source); State->Load_Implementation();
	TestTrue(TEXT("Original snapshot validates"), State->IsStateValid());
	FSovCampaignStateTestAccess::SetFirstSequence(*State, 99); State->Load_Implementation();
	TestFalse(TEXT("Out-of-order journal fails closed"), State->IsStateValid());
	TestFalse(TEXT("Invalid state denies even empty knowledge query"), State->HasKnowledge(Tags.Character_Player_Tarrik, FGameplayTagContainer()));
	FSovCampaignStateTestAccess::SetFirstSequence(*State, 1); State->Load_Implementation();
	TestTrue(TEXT("Valid journal can restore again"), State->IsStateValid());
	FSovCampaignStateTestAccess::SetEvidenceGuid(*State, FGuid()); State->Load_Implementation();
	TestFalse(TEXT("Missing provenance fails restore"), State->IsStateValid());
	FSovCampaignStateTestAccess::SetEvidenceGuid(*State, Source->SourceId); State->Load_Implementation();
	TestTrue(TEXT("Provenance repairs from saved record"), State->IsStateValid());
	FSovCampaignStateTestAccess::SetFact(*State, Tags.State_CommandLink_Active, Tags.Character_Player_Selene); State->Load_Implementation();
	TestFalse(TEXT("Canon fact contradicting committed journal fails restore"), State->IsStateValid());
	return true;
}
#endif
