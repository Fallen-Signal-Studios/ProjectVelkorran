// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovEncounterRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FEncounterRuntimeWorld
	{
		UWorld* World = nullptr;
		FEncounterRuntimeWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
		}
		~FEncounterRuntimeWorld()
		{
			if (!World) { return; }
			World->DestroyWorld(false);
			if (GEngine) { GEngine->DestroyWorldContext(World); }
		}
		ASovAxiomRuntimeTestCharacter* Character()
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Character = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params) : nullptr;
			if (Character) { Character->InitializeTestCombat(0); }
			return Character;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterResourceRestoreTest,
	"ProjectVelkorran.Campaign.Encounter.ResourceRestorePreservesDefinitionMaxima",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovEncounterResourceRestoreTest::RunTest(const FString& Parameters)
{
	FEncounterRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Real ASC fixture"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 70.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 60.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 80.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 50.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 35.f);
	FSovCombatResourceSnapshot Snapshot;
	TestTrue(TEXT("Capture all five resource pairs"), USovEncounterSnapshotLibrary::CaptureResources(ASC, Snapshot));
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 200.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 40.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 10.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), 0.f);
	TestTrue(TEXT("Restore snapshot"), USovEncounterSnapshotLibrary::RestoreResources(ASC, Snapshot));
	TestEqual(TEXT("Retuned max Health remains"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()), 200.f);
	TestEqual(TEXT("Absolute Health restored, not percentage"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 70.f);
	TestEqual(TEXT("Shield clamped to current max"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 40.f);
	TestEqual(TEXT("Echo restored"), Character->TestEcho->GetEcho(), 35.f);
	TestTrue(TEXT("Repeat restore is idempotent"), USovEncounterSnapshotLibrary::RestoreResources(ASC, Snapshot));
	Snapshot.Health = -1.f;
	TestFalse(TEXT("Malformed snapshot rejected before any mutation"), USovEncounterSnapshotLibrary::RestoreResources(ASC, Snapshot));
	TestEqual(TEXT("Rejected resource record leaves current Health"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 70.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterLedgerSaveTest,
	"ProjectVelkorran.Campaign.Encounter.NarrativeSaveAndRewardReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovEncounterLedgerSaveTest::RunTest(const FString& Parameters)
{
	FEncounterRuntimeWorld Fixture;
	if (!TestNotNull(TEXT("World"), Fixture.World)) { return false; }
	auto* Director = Fixture.World->SpawnActor<ASovEncounterRuntimeTestDirector>();
	if (!TestNotNull(TEXT("Director"), Director)) { return false; }
	Director->EncounterId = TEXT("Test.Courtyard");
	const FGuid StableId = Director->GetActorGUID_Implementation();
	TestTrue(TEXT("Authored identity has stable GUID"), StableId.IsValid());
	TestFalse(TEXT("Empty encounter cannot begin"), Director->BeginEncounter());
	FString Error;
	TestFalse(TEXT("Missing checkpoint cannot retry"), Director->RetryEncounter(Error));
	TestFalse(TEXT("Completion reward cannot claim while inactive"), Director->ClaimCompletionReward(TEXT("Technique")));
	Director->SeedState(ESovEncounterState::Active);
	TestTrue(TEXT("Validated attempt receipt claims once"), Director->ClaimAttemptReward(TEXT("Selene.UndetectedBypass")));
	TestFalse(TEXT("Second gate cannot claim same attempt receipt"), Director->ClaimAttemptReward(TEXT("Selene.UndetectedBypass")));
	Director->SeedState(ESovEncounterState::Succeeded);
	TestTrue(TEXT("Completion reward first claim"), Director->ClaimCompletionReward(TEXT("Technique")));
	TestFalse(TEXT("Completion reward second claim"), Director->ClaimCompletionReward(TEXT("Technique")));
	UNarrativeSaveSubsystem* Save = Fixture.World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!TestNotNull(TEXT("Existing Narrative save subsystem"), Save)) { return false; }
	FNarrativeActorRecord Record;
	TestTrue(TEXT("Narrative serializes director record"), Save->CreateActorRecord(Director, Record));
	auto* Restored = Fixture.World->SpawnActor<ASovEncounterRuntimeTestDirector>();
	Restored->EncounterId = Director->EncounterId;
	Save->LoadActorFromRecord(Restored, Record);
	TestEqual(TEXT("Stable authored identity restored"), Restored->GetActorGUID_Implementation(), StableId);
	TestEqual(TEXT("Completed state survives load"), Restored->GetEncounterState(), ESovEncounterState::Succeeded);
	TestFalse(TEXT("Saved reward cannot replay after load"), Restored->ClaimCompletionReward(TEXT("Technique")));
	Director->SeedState(ESovEncounterState::Active);
	Save->CreateActorRecord(Director, Record);
	Save->LoadActorFromRecord(Restored, Record);
	TestEqual(TEXT("Mid-attempt load requires explicit entry retry"), Restored->GetEncounterState(), ESovEncounterState::Failed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterCommandLinkRestoreTest,
	"ProjectVelkorran.Campaign.Encounter.CommandLinkRestoreHasNoSeverReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovEncounterCommandLinkRestoreTest::RunTest(const FString& Parameters)
{
	FEncounterRuntimeWorld Fixture;
	auto* Owner = Fixture.Character();
	auto* Participant = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner) || !TestNotNull(TEXT("Participant"), Participant)) { return false; }
	auto* Observer = Fixture.World->SpawnActor<ASovEncounterRuntimeTestDirector>();
	auto* Link = NewObject<USovAxiomRuntimeTestCommandLink>(Owner, TEXT("SavedCommandLink"));
	Owner->AddInstanceComponent(Link);
	Link->RegisterComponent();
	Link->OnCommandLinkSevered.AddDynamic(Observer, &ASovEncounterRuntimeTestDirector::ObserveSever);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	auto* ASC = Participant->GetNarrativeAbilitySystemComponent();
	ASC->AddLooseGameplayTag(Tags.State_CommandLink_Active); // unrelated contributor
	FSovCommandLinkSnapshot Snapshot;
	Snapshot.LinkId = Link->GetLinkId();
	Snapshot.State = ESovCommandLinkState::Active;
	Snapshot.LinkInstanceId = FGuid::NewGuid();
	TArray<AActor*> Participants { Participant };
	TestTrue(TEXT("Restore active link"), Link->RestoreCommandLinkState(Snapshot, Owner, Participants));
	TestEqual(TEXT("Link adds exactly one owned contribution"), ASC->GetTagCount(Tags.State_CommandLink_Active), 2);
	TestTrue(TEXT("Repeat active restore"), Link->RestoreCommandLinkState(Snapshot, Owner, Participants));
	TestEqual(TEXT("Repeat restore does not add tag counts"), ASC->GetTagCount(Tags.State_CommandLink_Active), 2);
	Snapshot.State = ESovCommandLinkState::Severed;
	Snapshot.LastSeverTransactionId = FGuid::NewGuid();
	TestTrue(TEXT("Restore saved severed state"), Link->RestoreCommandLinkState(Snapshot, Owner, Participants));
	TestEqual(TEXT("Unrelated active contribution preserved"), ASC->GetTagCount(Tags.State_CommandLink_Active), 1);
	TestTrue(TEXT("Saved severed capability state reconstructed"), ASC->HasMatchingGameplayTag(Tags.State_CommandLink_Severed));
	TestEqual(TEXT("Restore never emits a new sever transaction"), Observer->SeverEventCount, 0);
	FSovCommandLinkSnapshot Invalid = Snapshot;
	Invalid.LinkId = TEXT("WrongLink");
	TestFalse(TEXT("Wrong authored ID rejected"), Link->RestoreCommandLinkState(Invalid, Owner, Participants));
	TestEqual(TEXT("Invalid restore retains previous transaction"), Link->CaptureCommandLinkState().LastSeverTransactionId, Snapshot.LastSeverTransactionId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEncounterOwnedLifecycleResetTest,
	"ProjectVelkorran.Campaign.Encounter.ResourceLifecycleResetPreservesExternalTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovEncounterOwnedLifecycleResetTest::RunTest(const FString& Parameters)
{
	FEncounterRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Character"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	auto* Shield = NewObject<USovShieldComponent>(Character);
	auto* Poise = NewObject<USovPoiseComponent>(Character);
	Character->AddInstanceComponent(Shield); Shield->RegisterComponent();
	Character->AddInstanceComponent(Poise); Poise->RegisterComponent();
	Shield->InitializeWithAbilitySystem(ASC);
	Poise->InitializeWithAbilitySystem(ASC);
	const FGameplayTag ExternalTag = FSovGameplayTags::Get().State_Shield_Broken;
	ASC->AddLooseGameplayTag(ExternalTag);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	TestEqual(TEXT("Own and unrelated broken contributions"), ASC->GetTagCount(ExternalTag), 2);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 50.f);
	Shield->ResetForCheckpoint();
	TestEqual(TEXT("Reset preserves unrelated broken contributor"), ASC->GetTagCount(ExternalTag), 1);
	TestTrue(TEXT("Checkpoint starts a fresh shield delay"), Shield->GetSecondsUntilRecharge() > 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 80.f);
	Poise->ResetForCheckpoint();
	TestFalse(TEXT("Prior owned broken state removed"), Poise->IsPoiseBroken());
	TestFalse(TEXT("Prior owned recovery state removed"), Poise->IsPoiseRecovering());
	TestTrue(TEXT("Poise checkpoint delay restarts"), Poise->GetSecondsUntilRegeneration() > 0.f);
	return true;
}
#endif
