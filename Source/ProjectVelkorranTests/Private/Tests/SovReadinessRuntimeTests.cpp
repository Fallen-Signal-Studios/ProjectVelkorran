// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovReadinessRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FReadinessWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovHandoffRuntimeTestController* PC = nullptr;
		ASovReadinessRuntimeTestPawn* Pawn = nullptr;
		ASovPlayerState* PS = nullptr;
		UNarrativeAbilitySystemComponent* ASC = nullptr;
		USovReadinessRuntimeProbe* Probe = nullptr;
		int32 GameplayReadyCount = 0;
		FDelegateHandle GameplayReadyHandle;
		FReadinessWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			PC = World->SpawnActor<ASovHandoffRuntimeTestController>();
			Pawn = World->SpawnActor<ASovReadinessRuntimeTestPawn>();
			PS = World->SpawnActor<ASovPlayerState>();
			if (!PC || !Pawn || !PS) { return; }
			auto* Definition = NewObject<UPlayerDefinition>(PC); PC->KeepAlive.Add(Definition);
			if (!Pawn->PrepareCampaignInitialization(Definition)) { return; }
			PC->SetTestPlayerState(PS); PC->Possess(Pawn);
			if (!Pawn->StageTestReadiness(PS, true)) { return; }
			Pawn->BindProductionReadiness();
			ASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
			if (!ASC) { return; }
			Probe = NewObject<USovReadinessRuntimeProbe>(PC); PC->KeepAlive.Add(Probe); Probe->ASC = ASC;
			ASC->OnCharacterReadyEpochChanged.AddDynamic(Probe, &USovReadinessRuntimeProbe::EpochChanged);
			Pawn->OnCharacterReady.AddDynamic(Probe, &USovReadinessRuntimeProbe::Ready);
			Pawn->OnCharacterReadinessChanged.AddDynamic(Probe, &USovReadinessRuntimeProbe::Changed);
			GameplayReadyHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(FSovGameplayTags::Get().Event_Character_Ready)
				.AddLambda([this](const FGameplayEventData*) { ++GameplayReadyCount; });
		}
		~FReadinessWorld()
		{
			if (Probe) { Probe->OnEpoch = nullptr; Probe->OnReady = nullptr; Probe->OnChanged = nullptr; }
			if (ASC) { ASC->GenericGameplayEventCallbacks.FindOrAdd(FSovGameplayTags::Get().Event_Character_Ready).Remove(GameplayReadyHandle); }
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReadyEpochAuthorityTest,
	"ProjectVelkorran.Campaign.Readiness.AuthorityEpochPublishesCommittedChangesExactlyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReadyEpochAuthorityTest::RunTest(const FString& Parameters)
{
	FReadinessWorld F;
	if (!TestNotNull(TEXT("Staged readiness probe"), F.Probe)) { return false; }
	F.ASC->SetCharacterReadyEpoch(1);
	TestEqual(TEXT("Authority notifies local listeners"), F.Probe->Epochs.Num(), 1);
	TestFalse(TEXT("Listeners observe the committed epoch"), F.Probe->bObservedUncommittedEpoch);
	F.ASC->SetCharacterReadyEpoch(1); F.ASC->SetCharacterReadyEpoch(0);
	TestEqual(TEXT("Duplicate and stale writes cannot publish again"), F.Probe->Epochs.Num(), 1);
	F.ASC->SetCharacterReadyEpoch(3);
	TestEqual(TEXT("A later authoritative epoch publishes once"), F.Probe->Epochs.Num(), 2);
	TestEqual(TEXT("Epoch advances monotonically"), F.ASC->GetCharacterReadyEpoch(), 3);
	TestFalse(TEXT("Epoch publication cannot bypass initial data admission"), F.Pawn->IsCharacterReady());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReadyEpochReentryTest,
	"ProjectVelkorran.Campaign.Readiness.AuthorityFinalizationSurvivesReentrantEpochListener",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReadyEpochReentryTest::RunTest(const FString& Parameters)
{
	FReadinessWorld F;
	if (!TestNotNull(TEXT("Staged readiness probe"), F.Probe)) { return false; }
	F.Probe->OnEpoch = [&](int32 Epoch) { F.Pawn->RetryReadiness(Epoch); };
	TestTrue(TEXT("Production completion succeeds through a reentrant local listener"), F.Pawn->CompleteCampaignDataInitialization(false));
	TestEqual(TEXT("Reentry cannot recursively advance the epoch"), F.ASC->GetCharacterReadyEpoch(), 1);
	TestEqual(TEXT("One epoch notification"), F.Probe->Epochs.Num(), 1);
	TestEqual(TEXT("Character ready published once"), F.Probe->ReadyCount, 1);
	TestEqual(TEXT("Readiness transition published once"), F.Probe->ReadyTrueCount, 1);
	TestEqual(TEXT("Ready gameplay event published once"), F.GameplayReadyCount, 1);
	TestTrue(TEXT("Shield binding survives the authority epoch"), F.Pawn->GetShieldComponent()->IsInitialized());
	TestTrue(TEXT("Poise binding survives the authority epoch"), F.Pawn->GetPoiseComponent()->IsInitialized());
	TestEqual(TEXT("Ready publication preserves staged Shield"), F.Pawn->GetShieldComponent()->GetShield(), 50.f);
	TestEqual(TEXT("Ready publication preserves staged Poise"), F.Pawn->GetPoiseComponent()->GetPoise(), 50.f);
	TestEqual(TEXT("Reentry does not refill or reset Echo"), F.ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute()), 50.f);
	F.Pawn->RetryReadiness(1);
	TestEqual(TEXT("Repeated arrival cannot republish character readiness"), F.Probe->ReadyCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReadyEpochRetirementTest,
	"ProjectVelkorran.Campaign.Readiness.EpochCallbacksCannotPublishRetiredInitialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReadyEpochRetirementTest::RunTest(const FString& Parameters)
{
	for (int32 Mode = 0; Mode < 3; ++Mode)
	{
		FReadinessWorld F;
		if (!TestNotNull(TEXT("Staged readiness probe"), F.Probe)) { return false; }
		// Narrative preserves vehicle/turret ownership; replacement must be a real character.
		AActor* Replacement = F.World->SpawnActor<ASovReadinessRuntimeTestPawn>();
		F.Probe->OnEpoch = [&](int32 Epoch)
		{
			if (Mode == 0) { F.Pawn->FailCampaignInitialization(); }
			else if (Mode == 1) { F.ASC->InitAbilityActorInfo(F.PS, Replacement); }
			else if (Epoch == 1) { F.ASC->SetCharacterReadyEpoch(2); }
		};
		TestFalse(TEXT("Retired initialization cannot report successful completion"), F.Pawn->CompleteCampaignDataInitialization(false));
		TestFalse(TEXT("Retired pawn remains unready"), F.Pawn->IsCharacterReady());
		TestFalse(TEXT("Retired epoch cannot publish the authority ready gate"), F.Pawn->HasAuthorityReadyGate());
		TestEqual(TEXT("No old character ready callback"), F.Probe->ReadyCount, 0);
		TestEqual(TEXT("No old readiness transition"), F.Probe->ReadyTrueCount, 0);
		TestEqual(TEXT("No old gameplay ready event"), F.GameplayReadyCount, 0);
		if (Mode == 1) { TestTrue(TEXT("The epoch listener's replacement avatar remains authoritative"), F.ASC->GetAvatarActor() == Replacement); }
		if (Mode == 2) { TestEqual(TEXT("The newer epoch survives retired cleanup"), F.ASC->GetCharacterReadyEpoch(), 2); }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovReadyPublicationRetirementTest,
	"ProjectVelkorran.Campaign.Readiness.ReadyCallbacksCannotPublishLaterSuccessAfterFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovReadyPublicationRetirementTest::RunTest(const FString& Parameters)
{
	for (bool bFailOnTransition : {false, true})
	{
		FReadinessWorld F;
		if (!TestNotNull(TEXT("Staged readiness probe"), F.Probe)) { return false; }
		if (bFailOnTransition) { F.Probe->OnChanged = [&](bool bReady) { if (bReady) { F.Pawn->FailCampaignInitialization(); } }; }
		else { F.Probe->OnReady = [&]() { F.Pawn->FailCampaignInitialization(); }; }
		TestFalse(TEXT("Readiness callback failure is reflected in the completion result"), F.Pawn->CompleteCampaignDataInitialization(false));
		TestFalse(TEXT("Failure stays unready after the outer callback returns"), F.Pawn->IsCharacterReady());
		TestFalse(TEXT("Failure retires the published authority gate"), F.Pawn->HasAuthorityReadyGate());
		TestEqual(TEXT("Only the original ready callback ran"), F.Probe->ReadyCount, 1);
		TestEqual(TEXT("No later successful transition follows a failed ready callback"), F.Probe->ReadyTrueCount, bFailOnTransition ? 1 : 0);
		TestEqual(TEXT("Failure transition is emitted exactly once"), F.Probe->ReadyFalseCount, 1);
		TestEqual(TEXT("A failed callback cannot supply the gameplay ready event"), F.GameplayReadyCount, 0);
	}
	return true;
}
#endif
