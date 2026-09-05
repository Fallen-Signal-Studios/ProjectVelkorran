// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Character/PlayerDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Progression/SovTechniqueComponent.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FHandoffWorld
	{
		UWorld* World = nullptr;
		FHandoffWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
		}
		~FHandoffWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHandoffIdentityTest, "ProjectVelkorran.Campaign.Handoff.StableActorIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHandoffIdentityTest::RunTest(const FString& Parameters)
{
	FHandoffWorld F; if (!F.World) { AddError(TEXT("World creation failed")); return false; }
	auto* First = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	auto* Second = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	auto* PC1 = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
	auto* PC2 = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
	if (!First || !Second || !PC1 || !PC2) { AddError(TEXT("Actor creation failed")); return false; }
	const FGuid PawnId = First->GetActorGUID_Implementation();
	TestTrue(TEXT("Live pawn gets a valid identity"), PawnId.IsValid());
	TestTrue(TEXT("Different pawns have different identities"), PawnId != Second->GetActorGUID_Implementation());
	TestTrue(TEXT("Pawn identity is stable across reads"), PawnId == First->GetActorGUID_Implementation());
	const FGuid ControllerId = PC1->GetActorGUID_Implementation();
	TestTrue(TEXT("Controller identity is valid and per-instance"), ControllerId.IsValid() && ControllerId != PC2->GetActorGUID_Implementation());
	TestTrue(TEXT("Controller identity is stable across reads"), ControllerId == PC1->GetActorGUID_Implementation());
	TestFalse(TEXT("Pawn CDO never receives a runtime GUID"), GetDefault<ASovHandoffRuntimeTestPawn>()->GetActorGUID_Implementation().IsValid());
	TestFalse(TEXT("Controller CDO never receives a runtime GUID"), GetDefault<ASovHandoffRuntimeTestController>()->GetActorGUID_Implementation().IsValid());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovHandoffEarlyRejectionTest, "ProjectVelkorran.Campaign.Handoff.EarlyAdmissionAndSaveRejection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovHandoffEarlyRejectionTest::RunTest(const FString& Parameters)
{
	FHandoffWorld F; if (!F.World) { AddError(TEXT("World creation failed")); return false; }
	auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
	auto* Pawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	if (!PC || !Pawn) { AddError(TEXT("Actor creation failed")); return false; }
	PC->SetTestPlayerState(nullptr); PC->Possess(Pawn);
	FString Error;
	TestFalse(TEXT("Missing mission definition rejected"), ASovPlayerController::ValidateMissionPawn(nullptr, Error));
	auto* BadMission = NewObject<USovCampaignDefinition>(PC); PC->KeepAlive.Add(BadMission);
	TestFalse(TEXT("Unready source and invalid destination cannot handoff"), PC->HandoffToMission(BadMission, FTransform::Identity, Error));
	TestTrue(TEXT("Rejected handoff retains original possession"), PC->GetPawn() == Pawn);
	TestEqual(TEXT("Rejected handoff leaves transition idle"), PC->GetCampaignTransitionState(), ESovCampaignTransitionState::Idle);
	UNarrativeSaveSubsystem* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!TestNotNull(TEXT("Real Narrative save subsystem"), Save)) { return false; }
	const FString UnwrittenSlot = TEXT("SovAutomationMissingPS_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TestFalse(TEXT("Explicit-slot save rejects missing PlayerState before any disk call"), Save->CreatePlayerOnlySaveInSlot(PC, UnwrittenSlot));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovManagedReadinessRestoreTest, "ProjectVelkorran.Campaign.Handoff.ManagedReadinessAndSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovManagedReadinessRestoreTest::RunTest(const FString& Parameters)
{
	FHandoffWorld F; if (!F.World) { AddError(TEXT("World creation failed")); return false; }
	auto* PC = F.World->SpawnActor<ASovHandoffRuntimeTestController>();
	auto* Pawn = F.World->SpawnActor<ASovHandoffRuntimeTestPawn>();
	auto* PS = F.World->SpawnActor<ASovPlayerState>();
	if (!PC || !Pawn || !PS) { AddError(TEXT("Actor creation failed")); return false; }
	auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
	TestTrue(TEXT("Managed definition can stage before possession"), Pawn->PrepareCampaignInitialization(Definition));
	PC->SetTestPlayerState(PS); PC->Possess(Pawn);
	TestTrue(TEXT("Actual pawn owns the staged PlayerState"), Pawn->GetPlayerState() == PS);
	if (!TestTrue(TEXT("Staging initializes the actual empty Technique profile"), Pawn->StageTestReadiness(PS, false))) { return false; }
	auto* Techniques = Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent());
	if (!TestNotNull(TEXT("Campaign uses the real Technique component"), Techniques)) { return false; }
	TestEqual(TEXT("Content-free fixture has no authored branches"), Techniques->GetActiveTechniqueBranches().Num(), 0);
	TestTrue(TEXT("Empty first-entry Technique profile is valid"), Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Empty profile starts with zero earned points"), Techniques->GetEarnedTechniquePoints(), 0);
	TestEqual(TEXT("Empty profile starts with zero available points"), Techniques->GetAvailableTechniquePoints(), 0);
	TestFalse(TEXT("Missing visual prerequisite blocks restore gate"), Pawn->IsCampaignDataReadyToApply());
	TestFalse(TEXT("Initial data cannot publish readiness prematurely"), Pawn->CompleteCampaignDataInitialization(false));
	Pawn->SetTestVisualReady(true);
	TestTrue(TEXT("Actual project component and ASC prerequisites open data gate"), Pawn->IsCampaignDataReadyToApply());
	TestFalse(TEXT("Data gate is not public gameplay readiness"), Pawn->IsCharacterReady());
	UNarrativeSaveSubsystem* Save = F.World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!TestNotNull(TEXT("Narrative save subsystem"), Save)) { return false; }
	FSovProtagonistSnapshot Snapshot;
	Snapshot.ProtagonistTag = Pawn->GetProtagonistIdentityTag(); Snapshot.PawnClass = Pawn->GetClass(); Snapshot.PlayerDefinition = Definition;
	auto* ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	if (!TestNotNull(TEXT("Shared PlayerState ASC"), ASC)) { return false; }
	TestTrue(TEXT("Actual resources captured"), USovEncounterSnapshotLibrary::CaptureResources(ASC, Snapshot.Resources));
	Snapshot.Resources.Health = 77.f; Snapshot.Resources.Shield = 22.f; Snapshot.Resources.Echo = 17.f;
	TestTrue(TEXT("Native actor record captured without disk I/O"), Save->CreateActorRecord(Pawn, Snapshot.PawnRecord));
	TestTrue(TEXT("Real skill component record captured"), USovEncounterSnapshotLibrary::CaptureComponent(PS->GetSkillTreeComponent(), Snapshot.SkillTreeRecord));
	TestTrue(TEXT("Snapshot satisfies native identity/schema validation"), Snapshot.IsValid());
	FString Error;
	auto WrongHero = Snapshot; WrongHero.ProtagonistTag = FSovGameplayTags::Get().Character_Player_Selene;
	TestFalse(TEXT("Cross-hero snapshot is rejected before mutation"), PS->RestoreProtagonistSnapshot(Pawn, WrongHero, false, Error));
	TestEqual(TEXT("Rejected snapshot preserves currents"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 50.f);
	if (!TestTrue(TEXT("Matching snapshot restores before public readiness"), PS->RestoreProtagonistSnapshot(Pawn, Snapshot, false, Error)))
	{ AddError(Error); return false; }
	TestTrue(TEXT("Empty Technique snapshot remains valid after native restore"), Techniques->IsTechniqueStateValid());
	TestEqual(TEXT("Restored empty profile has zero earned points"), Techniques->GetEarnedTechniquePoints(), 0);
	TestEqual(TEXT("Restored empty profile has zero available points"), Techniques->GetAvailableTechniquePoints(), 0);
	TestEqual(TEXT("Saved Health restored"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 77.f);
	TestEqual(TEXT("Saved Shield restored"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 22.f);
	TestEqual(TEXT("Saved Echo restored"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 17.f);
	TestFalse(TEXT("Snapshot application does not publish ready itself"), Pawn->IsCharacterReady());
	TestTrue(TEXT("Production completion gate publishes ready once"), Pawn->CompleteCampaignDataInitialization(false));
	TestTrue(TEXT("Pawn is now actually ready"), Pawn->IsCharacterReady());
	TestFalse(TEXT("Repeated completion cannot reinitialize data"), Pawn->CompleteCampaignDataInitialization(false));
	Pawn->FailCampaignInitialization();
	TestFalse(TEXT("Failure invalidates published readiness"), Pawn->IsCharacterReady());
	TestFalse(TEXT("Failed initialization cannot reopen data gate"), Pawn->IsCampaignDataReadyToApply());
	return true;
}
#endif
