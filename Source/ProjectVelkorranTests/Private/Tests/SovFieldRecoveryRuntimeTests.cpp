// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovFieldRecoveryRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "Character/PlayerDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FieldRecovery/SovFieldRecoveryComponent.h"
#include "FieldRecovery/SovGameplayAbility_FieldRecovery.h"
#include "FieldRecovery/SovFieldRecoveryStation.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovFieldRecoveryTestAccess
{
	static void Finish(USovGameplayAbility_FieldRecovery* Ability)
	{
		if (Ability->Recovery.IsValid()) { Ability->Recovery->StartedAt -= Ability->Recovery->CommittedDuration; }
		Ability->Poll();
	}
	static void Poll(USovGameplayAbility_FieldRecovery* Ability) { Ability->Poll(); }
	static void SetCharges(USovFieldRecoveryComponent* Recovery, int32 Charges) { Recovery->Charges = Charges; }
};
namespace
{
	struct FFieldRecoveryWorld
	{
		UWorld* World;
		ASovFieldRecoveryTestCharacter* Player;
		ASovPlayerState* State;
		UNarrativeAbilitySystemComponent* ASC;
		USovFieldRecoveryComponent* Recovery;
		FFieldRecoveryWorld(bool bSelene = false)
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			State = World->SpawnActor<ASovPlayerState>();
			Player = World->SpawnActor<ASovFieldRecoveryTestCharacter>(); Player->InitializeSharedState(State, bSelene);
			ASC = Player->GetNarrativeAbilitySystemComponent(); Recovery = Player->GetFieldRecoveryComponent();
		}
		~FFieldRecoveryWorld() { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		USovGameplayAbility_FieldRecovery* Grant(FGameplayAbilitySpecHandle& Handle)
		{
			Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovGameplayAbility_FieldRecovery::StaticClass(), 1));
			return Cast<USovGameplayAbility_FieldRecovery>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		}
		void SetHealth(float Value) { ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), Value); }
		float Health() const { return ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()); }
	};
}

namespace
{
	// Physical supply authorization needs the same possessed, ready, grounded
	// campaign protagonist as Technique safe points. Ability-only fixtures above
	// deliberately do not stage this wider gameplay boundary.
	struct FReadyFieldRecoveryWorld
	{
		UWorld* World = nullptr;
		ASovPlayerState* State = nullptr;
		ASovHandoffRuntimeTestPawn* Player = nullptr;
		USovFieldRecoveryComponent* Recovery = nullptr;
		FReadyFieldRecoveryWorld()
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			State = World->SpawnActor<ASovPlayerState>();
			Player = World->SpawnActor<ASovHandoffRuntimeTestPawn>();
			auto* Controller = World->SpawnActor<ASovHandoffRuntimeTestController>();
			if (!State || !Player || !Controller) { return; }
			auto* Definition = NewObject<UPlayerDefinition>(Controller); Controller->KeepAlive.Add(Definition);
			Player->PrepareCampaignInitialization(Definition);
			Controller->SetTestPlayerState(State); Controller->Possess(Player);
			if (!Player->StageTestReadiness(State, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Recovery = Player->GetFieldRecoveryComponent();
		}
		~FReadyFieldRecoveryWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFieldRecoveryUseTest, "ProjectVelkorran.Campaign.FieldRecovery.CompletedHealCancellationAndExhaustion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovFieldRecoveryUseTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters); FFieldRecoveryWorld Test;
	FGameplayAbilitySpecHandle Handle; auto* Ability = Test.Grant(Handle);
	TestEqual(TEXT("Standard begins with two charges"), Test.Recovery->GetCharges(), 2);
	TestFalse(TEXT("Full health rejects use without spending"), Test.ASC->TryActivateAbility(Handle, false));
	Test.SetHealth(20.f);
	TestTrue(TEXT("Injured protagonist starts recovery"), Test.ASC->TryActivateAbility(Handle, false));
	TestEqual(TEXT("Default use does not consume before completion"), Test.Recovery->GetCharges(), 2);
	FSovFieldRecoveryTestAccess::Poll(Ability);
	TestEqual(TEXT("No healing before elapsed animation interval"), Test.Health(), 20.f);
	FNarrativeSaveComponent Unsafe;
	TestFalse(TEXT("In-progress healing cannot be represented as a safe component save"), USovEncounterSnapshotLibrary::CaptureComponent(Test.Recovery, Unsafe));
	FSovFieldRecoveryTestAccess::Finish(Ability);
	TestEqual(TEXT("Completed heal restores the fixed maximum-health fraction"), Test.Health(), 55.f);
	TestEqual(TEXT("Completed heal consumes exactly one charge"), Test.Recovery->GetCharges(), 1);
	TestFalse(TEXT("Completed use releases its reservation"), Test.Recovery->IsUsing());
	TestFalse(TEXT("Completed use releases its Busy contribution"), Test.ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy));
	TestTrue(TEXT("Second use starts"), Test.ASC->TryActivateAbility(Handle, false));
	Test.ASC->ApplyModToAttributeUnsafe(UNarrativeAttributeSetBase::GetHealthAttribute(), EGameplayModOp::Additive, -5.f);
	TestFalse(TEXT("Actual health loss interrupts recovery immediately"), Ability->IsActive());
	TestEqual(TEXT("Interrupted default use preserves charge"), Test.Recovery->GetCharges(), 1);
	TestTrue(TEXT("Recovery can retry after interruption"), Test.ASC->TryActivateAbility(Handle, false));
	FSovFieldRecoveryTestAccess::Finish(Ability);
	TestEqual(TEXT("Final charge is consumed"), Test.Recovery->GetCharges(), 0);
	TestFalse(TEXT("Empty recovery cannot activate"), Test.ASC->TryActivateAbility(Handle, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFieldRecoverySaveAndRefillTest, "ProjectVelkorran.Campaign.FieldRecovery.PawnRecordAndPhysicalRefill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovFieldRecoverySaveAndRefillTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters); FReadyFieldRecoveryWorld Test;
	if (!TestNotNull(TEXT("Ready campaign pawn owns field recovery"), Test.Recovery)) { return false; }
	TestTrue(TEXT("Physical refill fixture is ready and grounded"), Test.Player->IsCharacterReady()
		&& Test.Player->GetCharacterMovement()->IsMovingOnGround());
	FSovFieldRecoveryTestAccess::SetCharges(Test.Recovery, 1);
	FNarrativeSaveComponent Record;
	TestTrue(TEXT("Charges use Narrative's existing component record"), USovEncounterSnapshotLibrary::CaptureComponent(Test.Recovery, Record));
	FSovFieldRecoveryTestAccess::SetCharges(Test.Recovery, 0);
	TestTrue(TEXT("Saved charge count restores"), USovEncounterSnapshotLibrary::RestoreComponent(Test.Recovery, Record));
	TestEqual(TEXT("Reload does not mint missing charges"), Test.Recovery->GetCharges(), 1);
	auto* Point = Test.World->SpawnActor<ASovFieldRecoveryStation>(); Point->SafePointId = TEXT("Test.FieldSupply");
	FString Error;
	Point->SetActorLocation(FVector(1000.f, 0.f, 0.f));
	TestFalse(TEXT("Distant caller cannot refill"), Test.Recovery->RefillAtSafePoint(Point, Error));
	Point->SetActorLocation(FVector(150.f, 0.f, 0.f));
	Point->bRefillsFieldRecovery = false;
	TestFalse(TEXT("Ordinary safe point does not imply field refill"), Test.Recovery->RefillAtSafePoint(Point, Error));
	Point->bRefillsFieldRecovery = true;
	TestTrue(TEXT("Marked physical safe point refills charges"), Test.Recovery->RefillAtSafePoint(Point, Error));
	TestEqual(TEXT("Refill stops at capacity"), Test.Recovery->GetCharges(), 2);
	FSovFieldRecoveryTestAccess::SetCharges(Test.Recovery, 0);
	auto* Wall = Test.World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Wall);
	Wall->AddInstanceComponent(Box); Wall->SetRootComponent(Box); Box->SetBoxExtent(FVector(10.f, 200.f, 200.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent(); Wall->SetActorLocation(FVector(75.f, 0.f, 0.f));
	TestFalse(TEXT("Wall blocks direct refill API as well as the interaction"), Test.Recovery->RefillAtSafePoint(Point, Error));
	TestEqual(TEXT("Obstructed request changes no charges"), Test.Recovery->GetCharges(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovFieldRecoveryCallbackTest, "ProjectVelkorran.Campaign.FieldRecovery.CallbackOwnershipAndEarlyConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovFieldRecoveryCallbackTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters); FFieldRecoveryWorld Test(true);
	FGameplayAbilitySpecHandle Handle; auto* Ability = Test.Grant(Handle); Test.SetHealth(20.f);
	Test.Recovery->bConsumeOnStart = true;
	TestTrue(TEXT("Authored early-consumption mode activates"), Test.ASC->TryActivateAbility(Handle, false));
	TestEqual(TEXT("Explicit early mode spends at start"), Test.Recovery->GetCharges(), 1);
	Test.ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	TestFalse(TEXT("Cinematic interrupts the held recovery"), Ability->IsActive());
	TestEqual(TEXT("Explicit early consumption does not refund on cancellation"), Test.Recovery->GetCharges(), 1);
	Test.ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
	Test.Recovery->bConsumeOnStart = false;
	auto* Observer = NewObject<USovFieldRecoveryTestObserver>(Test.Player);
	Test.Recovery->OnChargesChanged.AddDynamic(Observer, &USovFieldRecoveryTestObserver::Changed);
	TestTrue(TEXT("Last default charge activates"), Test.ASC->TryActivateAbility(Handle, false));
	bool bRestoreOnce = true;
	const FDelegateHandle Change = Test.ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddLambda([&Test, &bRestoreOnce](const FOnAttributeChangeData& Data)
		{
			if (bRestoreOnce && Data.NewValue > Data.OldValue) { bRestoreOnce = false; Test.Recovery->Load_Implementation(); }
		});
	FSovFieldRecoveryTestAccess::Finish(Ability);
	Test.ASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(Change);
	TestEqual(TEXT("Heal stays committed despite a synchronous restored-state callback"), Test.Health(), 55.f);
	TestEqual(TEXT("Committed charge is not refunded onto restored state"), Test.Recovery->GetCharges(), 0);
	TestEqual(TEXT("Only the new load state notifies; stale completion does not publish again"), Observer->Notifications, 1);
	TestFalse(TEXT("Superseded recovery leaves no active ability"), Ability->IsActive());
	return true;
}
#endif
