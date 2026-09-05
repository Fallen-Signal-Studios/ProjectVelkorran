// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCorruptionRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SovCorruptionComponent.h"
#include "Corruption/SovCorruptionSourceVolume.h"
#include "Corruption/SovCorruptionSourceComponent.h"
#include "Corruption/SovCorruptionInteractableComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "NarrativeGameplayTags.h"
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
	static void Bind(USovCorruptionSourceComponent& Producer) { Producer.BindOwner(); }
	static void SetNode(ASovCorruptionSourceVolume& Source, AActor* Node) { Source.SourceNode = Node; }
	static void Interact(USovCorruptionInteractableComponent& Source, APawn* Player, UNarrativeInteractionComponent* Interaction)
	{
		Source.Interact(Player, Interaction);
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

namespace
{
	USovCorruptionSourceComponent* AttachProducer(ASovCorruptionRuntimeTestPawn* Source, USovCorruptionProfile* Profile)
	{
		auto* Producer = NewObject<USovCorruptionSourceComponent>(Source);
		Producer->Profile = Profile; Source->AddInstanceComponent(Producer); Producer->RegisterComponent();
		FSovCorruptionTestAccess::Bind(*Producer); return Producer;
	}
	void CorruptionHit(ASovCorruptionRuntimeTestPawn* Source, ASovCorruptionRuntimeTestPawn* Target, float Damage = 1.0f, bool bMixed = false)
	{
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		auto Context = ASC->MakeEffectContext(); Context.AddInstigator(Source, Source);
		FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.0f);
		Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
		Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Corruption);
		if (bMixed) { Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic); }
		ASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionAttackProducerTest, "ProjectVelkorran.Campaign.Corruption.NativeAttackProducer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionAttackProducerTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission());
	auto* Enemy = F.World->SpawnActor<ASovCorruptionRuntimeTestPawn>(); Enemy->InitializeCombat();
	auto* Profile = F.Profile(TEXT("EnemyHit"), 12.0f); Profile->SourceKind = ESovCorruptionSourceKind::EnemyAttack;
	AttachProducer(Enemy, Profile);
	auto* ASC = F.Pawn->GetNarrativeAbilitySystemComponent();
	ASC->OnDamageResolvedAsTarget.AddDynamic(F.Pawn, &ASovCorruptionRuntimeTestPawn::ObserveDamage);
	CorruptionHit(Enemy, F.Pawn);
	TestEqual(TEXT("Real accepted Corruption damage delivers profile exposure"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 12.0f);
	Enemy->GetNarrativeAbilitySystemComponent()->DamageResolvedAsSource(F.Pawn->LastDamage);
	TestEqual(TEXT("Replayed transaction cannot expose twice"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 12.0f);
	CorruptionHit(Enemy, F.Pawn);
	TestEqual(TEXT("A separate actual hit adds exposure"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 24.0f);
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().Damage_Immunity_Corruption);
	CorruptionHit(Enemy, F.Pawn, 1.0f, true);
	TestEqual(TEXT("Rejected Corruption channel does not borrow mixed Kinetic acceptance"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 24.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionLinkProducerTest, "ProjectVelkorran.Campaign.Corruption.NativeLinkAndRemedy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionLinkProducerTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission());
	auto* Enemy = F.World->SpawnActor<ASovCorruptionRuntimeTestPawn>(); Enemy->InitializeCombat();
	auto* Profile = F.Profile(TEXT("EnemyLink"), 30.0f); Profile->SourceKind = ESovCorruptionSourceKind::CommandLink;
	Profile->Escape = ESovCorruptionEscape::BreakLink;
	auto* Producer = AttachProducer(Enemy, Profile);
	auto* Link = NewObject<USovCorruptionRuntimeTestLink>(Enemy); Enemy->AddInstanceComponent(Link); Link->RegisterComponent();
	Link->RegisterLinkedActor(F.Pawn); TestTrue(TEXT("Native command link activates"), Link->ActivateCommandLink(Enemy));
	Producer->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Only actual membership exposes"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 30.0f);
	FSovCommandLinkSeverResult Sever;
	TestTrue(TEXT("Actual sever resolves the registered link"), Link->TrySeverCommandLink(F.Pawn, Sever) == ESovCommandLinkSeverResolution::NewlySevered);
	Producer->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Real sever clears that profile and disables its source"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionBandPressureTest, "ProjectVelkorran.Campaign.Corruption.NativeBandPressure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionBandPressureTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission());
	auto* ASC = F.Pawn->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaRegenRateAttribute(), 20.0f);
	auto* Profile = F.Profile(TEXT("Pressure"), 60.0f); F.Source(Profile);
	TestEqual(TEXT("Contest owned GAS effect adds documented vulnerability"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetDamageResistanceAttribute()), -5.0f);
	TestEqual(TEXT("Contest scales the existing Stamina recovery attribute"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaRegenRateAttribute()), 10.0f);
	TestEqual(TEXT("Owned Contest effect extends an accepted harmful status by 25 percent"), USovCorruptionComponent::ResolveIncomingStatusDuration(F.Pawn, 4.0f), 5.0f);
	F.Pawn->GetCorruptionComponent()->CleanseExposure(100.0f);
	TestEqual(TEXT("Cleanse removes only the owned vulnerability"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetDamageResistanceAttribute()), 0.0f);
	TestEqual(TEXT("Cleanse restores the original recovery rate"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaRegenRateAttribute()), 20.0f);
	TestEqual(TEXT("Cleanse removes status vulnerability"), USovCorruptionComponent::ResolveIncomingStatusDuration(F.Pawn, 4.0f), 4.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionMachineryTest, "ProjectVelkorran.Campaign.Corruption.NativeMachineryAndProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionMachineryTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission());
	auto* Machine = F.World->SpawnActor<AActor>();
	auto* Profile = F.Profile(TEXT("Machinery"), 30.0f); Profile->SourceKind = ESovCorruptionSourceKind::Machinery;
	auto* Interactable = NewObject<USovCorruptionInteractableComponent>(Machine);
	Interactable->Profile = Profile; Machine->AddInstanceComponent(Interactable); Interactable->RegisterComponent(); Interactable->Activate();
	auto* Interaction = NewObject<UPlayerInteractionComponent>(F.PC); F.PC->AddInstanceComponent(Interaction); Interaction->RegisterComponent();
	FSovCorruptionTestAccess::Interact(*Interactable, F.Pawn, Interaction);
	TestEqual(TEXT("Actual Narrative interaction exposes once"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 30.0f);
	FSovCorruptionTestAccess::Interact(*Interactable, F.Pawn, Interaction);
	TestEqual(TEXT("Repeated machine interaction cannot duplicate its mission receipt"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 30.0f);
	F.Pawn->GetCorruptionComponent()->CleanseExposure(100.0f);
	auto* Ally = F.World->SpawnActor<ASovCorruptionRuntimeTestPawn>(); Ally->InitializeCombat(); F.Pawn->Attitude = ETeamAttitude::Friendly;
	auto* AllyProfile = F.Profile(TEXT("DistinctSignal"), 40.0f); AllyProfile->SourceKind = ESovCorruptionSourceKind::ContaminatedAlly;
	AllyProfile->Escape = ESovCorruptionEscape::ProtectSignal; AllyProfile->ContaminatedAllyState = FSovGameplayTags::Get().State_Corruption_Intrusion;
	Ally->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(AllyProfile->ContaminatedAllyState);
	auto* Producer = AttachProducer(Ally, AllyProfile); Producer->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Actual contaminated friendly signal exposes"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 40.0f);
	auto* Anchor = NewObject<USovCorruptionInteractableComponent>(Machine); Anchor->Action = ESovCorruptionInteraction::ProtectSignal;
	Anchor->Profile = AllyProfile; Anchor->SourceActor = Ally; Anchor->ProtectionSeconds = 1.0f;
	Machine->AddInstanceComponent(Anchor); Anchor->RegisterComponent(); Anchor->Activate();
	FSovCorruptionTestAccess::Interact(*Anchor, F.Pawn, Interaction);
	Anchor->TickComponent(0.5f, LEVELTICK_All, nullptr);
	auto* Attacker = F.World->SpawnActor<ASovCorruptionRuntimeTestPawn>(); Attacker->InitializeCombat();
	CorruptionHit(Attacker, Ally); // Authoritative incoming damage interrupts a continuous protection interval.
	Anchor->TickComponent(0.5f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Damage resets protection progress"), Anchor->GetProtectionProgress(), 0.0f);
	Anchor->TickComponent(1.0f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Uninterrupted protection clears exposure"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 0.0f);
	Producer->TickComponent(0.1f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Resolved signal cannot immediately expose again"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionOverwriteTest, "ProjectVelkorran.Campaign.Corruption.NativeOverwriteClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionOverwriteTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission());
	auto* Director = F.World->SpawnActor<ASovCorruptionRuntimeTestDirector>(); Director->Arm();
	TestTrue(TEXT("Fixture director owns the actual possessed player"), Director->HasEncounterPlayer(F.Pawn));
	auto* Profile = F.Profile(TEXT("Overwrite"), 90.0f); Profile->bPersistExposureAtCheckpoint = true;
	Profile->Escape = ESovCorruptionEscape::AuthoredCountermeasure;
	Profile->MissionPermissions[0].bAllowOverwriteEncounterFailure = true;
	Profile->MissionPermissions[0].OverwriteEncounterId = TEXT("CorruptionClock"); Profile->MissionPermissions[0].OverwriteSeconds = 5.0f;
	F.Source(Profile); auto* Component = F.Pawn->GetCorruptionComponent();
	Component->TickComponent(1.0f, LEVELTICK_All, nullptr);
	const auto Normal = Component->GetPresentationRequest(); Component->SetReducedEffects(true);
	const auto Reduced = Component->GetPresentationRequest();
	TestTrue(TEXT("Permissioned active encounter starts a visible clock"), Normal.bOverwriteClockActive);
	TestEqual(TEXT("First elapsed second retained"), Normal.OverwriteSecondsRemaining, 4.0f);
	TestEqual(TEXT("Reduced effects preserve exact deadline"), Reduced.OverwriteSecondsRemaining, Normal.OverwriteSecondsRemaining);
	F.Pawn->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().Damage_Immunity_Corruption);
	Component->TickComponent(2.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Corruption immunity prevents overwrite failure"), Director->GetEncounterState() == ESovEncounterState::Active);
	F.Pawn->GetNarrativeAbilitySystemComponent()->RemoveLooseGameplayTag(FSovGameplayTags::Get().Damage_Immunity_Corruption);
	Component->TickComponent(4.0f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Expiry routes through actual EncounterDirector failure"), Director->GetEncounterState() == ESovEncounterState::Failed);
	TestTrue(TEXT("Native clock cannot invent campaign facts"), !F.PC->State->IsBeatComplete(F.PC->State->GetActiveMission()->MissionId, TEXT("Exposed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCorruptionNodeRemedyTest, "ProjectVelkorran.Campaign.Corruption.NativeDestroyNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCorruptionNodeRemedyTest::RunTest(const FString& Parameters)
{
	FCorruptionWorld F; if (!F.Pawn || !F.PC) { AddError(TEXT("Fixture failed")); return false; }
	F.PC->State->BeginMission(F.Mission());
	auto* Node = F.World->SpawnActor<ASovCorruptionRuntimeTestPawn>(); Node->InitializeCombat();
	auto* Profile = F.Profile(TEXT("DestructibleField"), 30.0f); Profile->Escape = ESovCorruptionEscape::DestroyNode;
	AttachProducer(Node, Profile);
	auto* Field = F.World->SpawnActor<ASovCorruptionSourceVolume>(); FSovCorruptionTestAccess::SetNode(*Field, Node);
	Field->SetCorruptionProfile(Profile); FSovCorruptionTestAccess::Refresh(*Field);
	TestEqual(TEXT("Owned environmental field exposes"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 30.0f);
	CorruptionHit(F.Pawn, Node, 200.0f);
	TestEqual(TEXT("Actual node death applies its documented remedy"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 0.0f);
	FSovCorruptionTestAccess::Refresh(*Field);
	TestEqual(TEXT("Dead resolved node cannot reactivate its field"), F.Pawn->GetCorruptionComponent()->GetCorruptionState().Exposure, 0.0f);
	return true;
}
#endif
