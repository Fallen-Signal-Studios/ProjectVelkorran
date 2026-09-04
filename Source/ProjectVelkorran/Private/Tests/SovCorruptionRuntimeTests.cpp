// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCorruptionRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SovCorruptionComponent.h"
#include "Corruption/SovCorruptionSourceVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
struct FSovCorruptionTestAccess
{
	static void Refresh(ASovCorruptionSourceVolume& Source) { Source.ExposureSphere->UpdateOverlaps(); Source.RefreshContacts(); }
	static float SavedExposure(const USovCorruptionComponent& Component)
	{
		float Total = 0.0f; for (const auto& Record : Component.SavedRecords) { Total += Record.Exposure; } return Total;
	}
	static void CorruptSave(USovCorruptionComponent& Component) { Component.SavedSchemaVersion = -1; }
	static void CopySave(const USovCorruptionComponent& Source, USovCorruptionComponent& Destination)
	{
		Destination.SavedSchemaVersion = Source.SavedSchemaVersion;
		Destination.SavedRecords = Source.SavedRecords;
		Destination.SavedBand = Source.SavedBand;
	}
};
namespace
{
	struct FCorruptionWorld
	{
		UWorld* World = nullptr;
		ASovCampaignRuntimeTestController* PC = nullptr;
		ASovCorruptionRuntimeTestPawn* Pawn = nullptr;
		FCorruptionWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
			PC = World->SpawnActor<ASovCampaignRuntimeTestController>();
			Pawn = World->SpawnActor<ASovCorruptionRuntimeTestPawn>();
			if (PC && Pawn) { PC->Possess(Pawn); Pawn->InitializeCombat(); }
		}
		~FCorruptionWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		USovCampaignDefinition* Mission()
		{
			auto* Mission = NewObject<USovCampaignDefinition>(PC);
			auto* Player = NewObject<UPlayerDefinition>(PC);
			PC->KeepAlive.Add(Mission); PC->KeepAlive.Add(Player);
			Mission->MissionId = TEXT("CorruptionTestMission"); Mission->Protagonist = Pawn->TestHero;
			Mission->PawnClass = ASovCorruptionRuntimeTestPawn::StaticClass(); Mission->PlayerDefinition = Player;
			FSovCampaignBeatDefinition Consequence; Consequence.BeatId = TEXT("Exposed"); Consequence.bOptional = true;
			FSovCampaignBeatDefinition Finish; Finish.BeatId = TEXT("Finish");
			Mission->Beats.Add(Consequence); Mission->Beats.Add(Finish);
			return Mission;
		}
		USovCorruptionProfile* Profile(FName Id, float Contact = 30.0f)
		{
			auto* Profile = NewObject<USovCorruptionProfile>(PC); PC->KeepAlive.Add(Profile);
			Profile->SourceId = Id; Profile->ContactExposure = Contact; Profile->ExposurePerSecond = 5.0f;
			Profile->bLinearFalloff = false; Profile->AllowedProtagonists.AddTag(Pawn->TestHero);
			Profile->MaximumBand = ESovCorruptionBand::OverwriteRisk;
			FSovCorruptionMissionPermission Permission; Permission.MissionId = TEXT("CorruptionTestMission");
			Permission.MaximumBand = ESovCorruptionBand::OverwriteRisk; Profile->MissionPermissions.Add(Permission);
			Profile->RemedyText = FText::FromString(TEXT("Leave this field."));
			Profile->InformationText = FText::FromString(TEXT("An Eclipse field is raising exposure."));
			Profile->ReducedEffectsSubstitute = FText::FromString(TEXT("Exposure rising; leave this field."));
			Profile->PresentationProfileId = Id;
			return Profile;
		}
		ASovCorruptionSourceVolume* Source(USovCorruptionProfile* Profile, FVector Location = FVector::ZeroVector)
		{
			auto* Source = World->SpawnActor<ASovCorruptionSourceVolume>();
			Source->SetActorLocation(Location); Source->SetCorruptionProfile(Profile);
			FSovCorruptionTestAccess::Refresh(*Source); return Source;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionPermissionTest, "ProjectVelkorran.Campaign.Corruption.PermissionAndContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionPermissionTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	auto* Component = F.Pawn->GetCorruptionComponent();
	auto* Profile = F.Profile(TEXT("Field"));
	auto* Source = F.Source(Profile);
	TestFalse(TEXT("No active mission means no exposure source"), Component->HasExposureSource());
	TestEqual(TEXT("No active mission stays clear"), Component->GetCorruptionState().Exposure, 0.0f);
	TestEqual(TEXT("Actual campaign mission starts"), F.PC->State->BeginMission(F.Mission()), ESovCampaignResult::Applied);
	FSovCorruptionTestAccess::Refresh(*Source);
	TestTrue(TEXT("Authority overlap admits source"), Component->HasExposureSource());
	TestEqual(TEXT("Contact amount applied once"), Component->GetCorruptionState().Exposure, 30.0f);
	FSovCorruptionTestAccess::Refresh(*Source);
	TestEqual(TEXT("Repeated overlap does not duplicate contact amount"), Component->GetCorruptionState().Exposure, 30.0f);
	F.Pawn->SetActorLocation(FVector(2000.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Forged out-of-volume acquire fails"), Component->AcquireSource(Source).IsValid());
	Component->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Leaving contact removes handle"), Component->HasExposureSource());
	TestTrue(TEXT("Authored escape dissipates exposure"), Component->GetCorruptionState().Exposure < 30.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionSnapshotTest, "ProjectVelkorran.Campaign.Corruption.SnapshotAndAccessibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionSnapshotTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); F.PC->State->BeginMission(Mission);
	auto* Component = F.Pawn->GetCorruptionComponent();
	Component->OnBandChanged.AddDynamic(F.Pawn, &ASovCorruptionRuntimeTestPawn::SaveOnBandChanged);
	auto* Profile = F.Profile(TEXT("PersistentField"), 60.0f); Profile->bPersistExposureAtCheckpoint = true;
	auto* Source = F.Source(Profile);
	TestTrue(TEXT("Band callback requested a snapshot"), F.Pawn->SaveCallbacks > 0);
	TestEqual(TEXT("Reentrant checkpoint sees committed exposure"), FSovCorruptionTestAccess::SavedExposure(*Component), 60.0f);
	const auto Normal = Component->GetPresentationRequest();
	Component->SetReducedEffects(true);
	const auto Reduced = Component->GetPresentationRequest();
	TestTrue(TEXT("Reduced presentation preserves exposure band"), Normal.Band == Reduced.Band);
	TestEqual(TEXT("Reduced presentation preserves exposure"), Normal.Exposure, Reduced.Exposure);
	TestTrue(TEXT("Reduced presentation preserves remedy"), Normal.RemedyTexts.Num() == 1 && Reduced.RemedyTexts.Num() == 1
		&& Normal.RemedyTexts[0].EqualTo(Reduced.RemedyTexts[0]));
	TestEqual(TEXT("Reduced presentation suppresses distortion intensity"), Reduced.SuggestedIntensity, 0.0f);
	Component->Load_Implementation();
	TestFalse(TEXT("Load does not trust serialized source handles"), Component->HasExposureSource());
	FString RestoreError;
	TestTrue(TEXT("Destination barrier restores saved mechanics before readiness"), Component->FinishCampaignRestore(Mission, RestoreError));
	FSovCorruptionTestAccess::Refresh(*Source);
	TestEqual(TEXT("Restored contact cannot duplicate instant exposure"), Component->GetCorruptionState().Exposure, 60.0f);
	TestTrue(TEXT("Cleanse succeeds"), Component->CleanseExposure(100.0f));
	TestEqual(TEXT("Cleanse callback checkpoint is already clear"), FSovCorruptionTestAccess::SavedExposure(*Component), 0.0f);
	TestFalse(TEXT("Cleanse removes own band effect"), F.Pawn->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Corruption_Contest));
	FSovCorruptionTestAccess::CorruptSave(*Component);
	Component->Load_Implementation();
	TestEqual(TEXT("Invalid schema fails clear"), Component->GetCorruptionState().Exposure, 0.0f);
	TestFalse(TEXT("Invalid schema rejects the destination readiness barrier"), Component->FinishCampaignRestore(Mission, RestoreError));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionConsequenceTest, "ProjectVelkorran.Campaign.Corruption.ConsequenceMissionCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionConsequenceTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); F.PC->State->BeginMission(Mission);
	auto* Component = F.Pawn->GetCorruptionComponent();
	auto* Limited = F.Profile(TEXT("LimitedCanonicalSource"), 1.0f);
	Limited->bCanonPersistent = true; Limited->ConsequenceBeatId = TEXT("Exposed");
	Limited->ConsequenceBand = ESovCorruptionBand::Contest; Limited->MissionPermissions[0].MaximumBand = ESovCorruptionBand::Trace;
	auto* SourceA = F.Source(Limited);
	auto* Other = F.Profile(TEXT("OtherSource"), 60.0f);
	auto* SourceB = F.Source(Other);
	TestTrue(TEXT("Aggregate exposure reaches Contest"), Component->GetCorruptionState().Band == ESovCorruptionBand::Contest);
	TestFalse(TEXT("Trace-permitted profile cannot borrow another source's Contest consequence"), F.PC->State->IsBeatComplete(Mission->MissionId, TEXT("Exposed")));
	SourceB->SetCorruptionProfile(nullptr);
	Component->CleanseExposure(100.0f, Other->SourceId);
	Limited->MissionPermissions[0].MaximumBand = ESovCorruptionBand::Contest; Limited->ExposurePerSecond = 100.0f;
	FSovCorruptionTestAccess::Refresh(*SourceA);
	Component->TickComponent(0.6f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Own permitted source contribution commits authored beat"), F.PC->State->IsBeatComplete(Mission->MissionId, TEXT("Exposed")));
	const int32 JournalSize = F.PC->State->GetJournal().Num();
	Component->CleanseExposure(100.0f);
	TestEqual(TEXT("Gameplay cleanse preserves consequence journal"), F.PC->State->GetJournal().Num(), JournalSize);
	TestTrue(TEXT("Gameplay cleanse preserves authored story fact"), F.PC->State->IsBeatComplete(Mission->MissionId, TEXT("Exposed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionProfileValidationTest, "ProjectVelkorran.Campaign.Corruption.ProfileValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionProfileValidationTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	auto* Profile = F.Profile(TEXT("Profile")); FString Error;
	TestTrue(TEXT("Complete explicit source contract validates"), Profile->ValidateProfile(Error));
	Profile->ReducedEffectsSubstitute = FText::GetEmpty();
	TestFalse(TEXT("Missing accessible substitute is rejected"), Profile->ValidateProfile(Error));
	Profile->ReducedEffectsSubstitute = FText::FromString(TEXT("Leave the field."));
	Profile->MissionPermissions.Empty();
	TestFalse(TEXT("Unpermissioned profile is rejected"), Profile->ValidateProfile(Error));
	F.PC->State->BeginMission(F.Mission());
	auto* SourceA = F.Source(F.Profile(TEXT("DuplicateIdentity"), 0.0f));
	auto* SourceB = F.Source(F.Profile(TEXT("DuplicateIdentity"), 0.0f));
	TestTrue(TEXT("Rate-only source registers before its first exposure tick"), F.Pawn->GetCorruptionComponent()->AcquireSource(SourceA).IsValid());
	TestFalse(TEXT("Distinct assets cannot claim one source ID even before any exposure record exists"), F.Pawn->GetCorruptionComponent()->AcquireSource(SourceB).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionOcclusionTest, "ProjectVelkorran.Campaign.Corruption.OcclusionCapAndSourceLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionOcclusionTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission()); F.Pawn->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	auto* Wall = F.World->SpawnActor<AActor>();
	auto* Box = NewObject<UBoxComponent>(Wall); Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(20.0f, 200.0f, 200.0f)); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Wall->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
	auto* Profile = F.Profile(TEXT("CappedField"), 100.0f); Profile->MaximumBand = ESovCorruptionBand::Trace;
	auto* Source = F.Source(Profile); auto* Component = F.Pawn->GetCorruptionComponent();
	TestFalse(TEXT("Occluded real overlap cannot register exposure"), Component->HasExposureSource());
	TestFalse(TEXT("Direct native acquire also enforces LOS"), Component->AcquireSource(Source).IsValid());
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision); Wall->Destroy();
	FSovCorruptionTestAccess::Refresh(*Source);
	TestTrue(TEXT("Unoccluded contact registers"), Component->HasExposureSource());
	TestTrue(TEXT("Source cap stops below Intrusion despite 100 contact exposure"), Component->GetCorruptionState().Exposure < 25.0f
		&& Component->GetCorruptionState().Band == ESovCorruptionBand::Trace);
	Source->Destroy(); Component->TickComponent(3.0f, LEVELTICK_All, nullptr);
	TestFalse(TEXT("Source destruction removes its handle"), Component->HasExposureSource());
	TestEqual(TEXT("Escape recovery clears exposure after source loss"), Component->GetCorruptionState().Exposure, 0.0f);
	TestFalse(TEXT("Expired exposure removes owned native band grant"), F.Pawn->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Corruption_Trace));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionDeferredRestoreTest, "ProjectVelkorran.Campaign.Corruption.DestinationRestoreBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionDeferredRestoreTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld SourceWorld;
	if (!SourceWorld.Pawn || !SourceWorld.PC) { AddError(TEXT("Source fixture failed")); return false; }
	SourceWorld.PC->State->BeginMission(SourceWorld.Mission());
	auto* Profile = SourceWorld.Profile(TEXT("RestoredField"), 60.0f);
	Profile->bPersistExposureAtCheckpoint = true;
	Profile->bCanonPersistent = true; Profile->ConsequenceBeatId = TEXT("Exposed");
	Profile->ConsequenceBand = ESovCorruptionBand::Intrusion;
	SourceWorld.Source(Profile);
	auto* SourceComponent = SourceWorld.Pawn->GetCorruptionComponent();
	SourceComponent->CleanseExposure(12.0f);
	TestEqual(TEXT("Save retains hysteresis at 48 exposure"), SourceComponent->GetCorruptionState().Band, ESovCorruptionBand::Contest);

	FCorruptionWorld DestinationWorld;
	if (!DestinationWorld.Pawn || !DestinationWorld.PC) { AddError(TEXT("Destination fixture failed")); return false; }
	auto* Destination = DestinationWorld.Mission();
	auto* Component = DestinationWorld.Pawn->GetCorruptionComponent();
	FSovCorruptionTestAccess::CopySave(*SourceComponent, *Component);
	Component->Load_Implementation();
	Component->TickComponent(10.0f, LEVELTICK_All, nullptr);
	Component->PrepareForSave_Implementation();
	TestEqual(TEXT("Null mission during controller restore cannot erase saved exposure"), FSovCorruptionTestAccess::SavedExposure(*Component), 48.0f);
	TestEqual(TEXT("Exposure remains dormant before destination admission"), Component->GetCorruptionState().Exposure, 0.0f);
	FString Error;
	TestFalse(TEXT("Barrier rejects a mission that has not become authoritative"), Component->FinishCampaignRestore(Destination, Error));
	TestEqual(TEXT("Destination mission can begin"), DestinationWorld.PC->State->BeginMission(Destination), ESovCampaignResult::Applied);
	TestTrue(TEXT("Explicit barrier works before character readiness"), Component->FinishCampaignRestore(Destination, Error));
	TestEqual(TEXT("Exposure survives the destination restore"), Component->GetCorruptionState().Exposure, 48.0f);
	TestEqual(TEXT("Saved hysteresis band survives deferred restore"), Component->GetCorruptionState().Band, ESovCorruptionBand::Contest);
	TestTrue(TEXT("Restored native GAS band exists before input readiness"), DestinationWorld.Pawn->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Corruption_Contest));
	TestFalse(TEXT("Restore does not synthesize a source contact"), Component->HasExposureSource());
	TestFalse(TEXT("Restore cannot replay an exposure consequence"), DestinationWorld.PC->State->IsBeatComplete(Destination->MissionId, TEXT("Exposed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionMissionIsolationTest, "ProjectVelkorran.Campaign.Corruption.MissionIsolationAndReadyRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionMissionIsolationTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	auto* Mission = F.Mission(); Mission->AllowedSuccessorMissions.Add(TEXT("NextMission"));
	F.PC->State->BeginMission(Mission);
	auto* Component = F.Pawn->GetCorruptionComponent();
	FString Error;
	TestTrue(TEXT("A fresh dormant pawn passes the validated mission barrier"), Component->FinishCampaignRestore(Mission, Error));
	auto* Profile = F.Profile(TEXT("MissionField"), 60.0f); Profile->bPersistExposureAtCheckpoint = true;
	F.Source(Profile);
	F.Pawn->SetReadyForRestoreTest(true);
	Component->Load_Implementation();
	TestEqual(TEXT("Already-ready encounter load restores synchronously"), Component->GetCorruptionState().Exposure, 60.0f);
	F.Pawn->SetReadyForRestoreTest(false);
	Component->Load_Implementation();
	Component->TickComponent(10.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Temporary source mission does not decay a pending destination snapshot"), FSovCorruptionTestAccess::SavedExposure(*Component), 60.0f);
	F.PC->State->CompleteBeat(TEXT("Finish"));
	auto* Next = F.Mission(); Next->MissionId = TEXT("NextMission");
	TestEqual(TEXT("Authored successor is admitted"), F.PC->State->BeginMission(Next), ESovCampaignResult::Applied);
	TestTrue(TEXT("New mission completes its explicit restore barrier"), Component->FinishCampaignRestore(Next, Error));
	TestEqual(TEXT("Exposure from a previous mission never crosses into the next"), Component->GetCorruptionState().Exposure, 0.0f);
	TestEqual(TEXT("Checkpoint now contains no previous-mission exposure"), FSovCorruptionTestAccess::SavedExposure(*Component), 0.0f);
	return true;
}
#endif
