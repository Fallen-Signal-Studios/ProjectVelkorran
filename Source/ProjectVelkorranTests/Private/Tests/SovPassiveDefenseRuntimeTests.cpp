// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPassiveDefenseTestFixtures.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace SovPassiveDefenseTests
{
struct FWorld
{
	UWorld* World = nullptr;
	uint64 FrameNumber = GFrameCounter;
	FWorld()
	{
		const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
			.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		if (World)
		{
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->GetTimerManager().Tick(0.f);
		}
	}
	~FWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
	ASovPassiveDefenseTestActor* Actor(const bool bInitialize = true)
	{
		auto* Actor = World ? World->SpawnActor<ASovPassiveDefenseTestActor>() : nullptr;
		if (Actor) { Actor->InitializeCombat(bInitialize); }
		return Actor;
	}
	void Advance(const int32 Frames = 10)
	{
		for (int32 Index = 0; Index < Frames; ++Index)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++FrameNumber);
			World->Tick(LEVELTICK_TimeOnly, 0.05f);
		}
	}
};
void SetResources(ASovPassiveDefenseTestActor* Actor, const float Shield, const float Poise)
{
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), Shield);
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), Poise);
}
void MoveASC(ASovPassiveDefenseTestActor* Original, ASovPassiveDefenseTestActor* Avatar)
{
	Avatar->ActiveASC = Original->OwnedASC;
	Original->OwnedASC->ClearActorInfo();
	Original->OwnedASC->InitAbilityActorInfo(Original, Avatar);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseLivingTimersTest,
	"ProjectVelkorran.Campaign.PassiveDefense.LivingTimersAndBrokenRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseLivingTimersTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor();
	if (!TestNotNull(TEXT("Actor"), Actor)) { return false; }
	TestTrue(TEXT("Shield bound"), Actor->Shield->IsInitialized());
	TestTrue(TEXT("Poise bound"), Actor->Poise->IsInitialized());
	SovPassiveDefenseTests::SetResources(Actor, 50.f, 50.f); F.Advance(2);
	TestEqual(TEXT("Shield delay retained"), Actor->Attributes->GetShield(), 50.f);
	TestEqual(TEXT("Poise delay retained"), Actor->Attributes->GetPoise(), 50.f);
	F.Advance(8);
	TestTrue(TEXT("Living shield recharges"), Actor->Attributes->GetShield() > 50.f);
	TestTrue(TEXT("Living poise regenerates"), Actor->Attributes->GetPoise() > 50.f);
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	TestEqual(TEXT("Break event once"), Actor->PoiseBreaks, 1);
	F.Advance(10);
	TestEqual(TEXT("Fallback refills poise"), Actor->Attributes->GetPoise(), 100.f);
	TestEqual(TEXT("Recovery event once"), Actor->PoiseRecoveries, 1);
	TestFalse(TEXT("Recovery window finishes"), Actor->Poise->IsPoiseRecovering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseRetainedPawnTest,
	"ProjectVelkorran.Campaign.PassiveDefense.RetainedPawnCannotWriteSharedASC",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseRetainedPawnTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Old = F.Actor(); auto* New = F.Actor(false);
	if (!Old || !New) { return false; }
	SovPassiveDefenseTests::SetResources(Old, 50.f, 50.f);
	SovPassiveDefenseTests::MoveASC(Old, New); F.Advance(20);
	TestFalse(TEXT("Old shield invalidated"), Old->Shield->IsInitialized());
	TestFalse(TEXT("Old poise invalidated"), Old->Poise->IsInitialized());
	TestEqual(TEXT("Old shield timer did not recharge new pawn"), Old->Attributes->GetShield(), 50.f);
	TestEqual(TEXT("Old poise timer did not refill new pawn"), Old->Attributes->GetPoise(), 50.f);
	TestFalse(TEXT("Explicit old shield bind rejected"), Old->Shield->InitializeWithAbilitySystem(Old->OwnedASC));
	TestFalse(TEXT("Explicit old poise bind rejected"), Old->Poise->InitializeWithAbilitySystem(Old->OwnedASC));
	TestTrue(TEXT("New shield can bind shared ASC"), New->Shield->InitializeWithAbilitySystem(Old->OwnedASC));
	TestTrue(TEXT("New poise can bind shared ASC"), New->Poise->InitializeWithAbilitySystem(Old->OwnedASC));
	F.Advance(10);
	TestTrue(TEXT("Current owner resumes shield"), Old->Attributes->GetShield() > 50.f);
	TestTrue(TEXT("Current owner resumes poise"), Old->Attributes->GetPoise() > 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseActorInfoABATest,
	"ProjectVelkorran.Campaign.PassiveDefense.ActorInfoABADoesNotReviveOldTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseActorInfoABATest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); auto* Other = F.Actor(false);
	if (!Actor || !Other) { return false; }
	SovPassiveDefenseTests::SetResources(Actor, 50.f, 50.f);
	SovPassiveDefenseTests::MoveASC(Actor, Other); SovPassiveDefenseTests::MoveASC(Actor, Actor);
	F.Advance(20);
	TestEqual(TEXT("Returned avatar does not inherit old shield timer"), Actor->Attributes->GetShield(), 50.f);
	TestEqual(TEXT("Returned avatar does not inherit old poise timer"), Actor->Attributes->GetPoise(), 50.f);
	Actor->Shield->ResetForCheckpoint(); Actor->Poise->ResetForCheckpoint(); F.Advance(10);
	TestTrue(TEXT("Explicit ownership reset permits fresh shield timer"), Actor->Attributes->GetShield() > 50.f);
	TestTrue(TEXT("Explicit ownership reset permits fresh poise timer"), Actor->Attributes->GetPoise() > 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseHealthZeroTest,
	"ProjectVelkorran.Campaign.PassiveDefense.HealthZeroStopsBeforeDeathNotification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseHealthZeroTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	SovPassiveDefenseTests::SetResources(Actor, 50.f, 0.f);
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestFalse(TEXT("Fixture has not yet published Narrative death"), Actor->ActiveASC->IsDead());
	TestFalse(TEXT("Corpse cannot explicitly recover"), Actor->Poise->RecoverFromPoiseBreak());
	F.Advance(20);
	TestEqual(TEXT("No corpse shield recharge"), Actor->Attributes->GetShield(), 50.f);
	TestEqual(TEXT("No corpse poise refill"), Actor->Attributes->GetPoise(), 0.f);
	TestEqual(TEXT("Death never publishes recovery"), Actor->PoiseRecoveries, 0);
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	F.Advance(20);
	TestEqual(TEXT("Health resurrection does not resume old shield schedule"), Actor->Attributes->GetShield(), 50.f);
	TestEqual(TEXT("Health resurrection does not resume old break fallback"), Actor->Attributes->GetPoise(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseCheckpointBarrierTest,
	"ProjectVelkorran.Campaign.PassiveDefense.CheckpointRebindsLifeWithoutPrematureTimers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseCheckpointBarrierTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	SovPassiveDefenseTests::SetResources(Actor, 0.f, 0.f);
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	Actor->Shield->SetCheckpointRestoreInProgress(true); Actor->Poise->SetCheckpointRestoreInProgress(true);
	Actor->ActiveASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	SovPassiveDefenseTests::SetResources(Actor, 25.f, 25.f);
	Actor->Shield->ResetForCheckpoint(); Actor->Poise->ResetForCheckpoint(); F.Advance(20);
	TestTrue(TEXT("Shield rebound to restored life"), Actor->Shield->IsInitialized());
	TestTrue(TEXT("Poise rebound to restored life"), Actor->Poise->IsInitialized());
	TestEqual(TEXT("Held shield does not advance"), Actor->Attributes->GetShield(), 25.f);
	TestEqual(TEXT("Held poise does not advance"), Actor->Attributes->GetPoise(), 25.f);
	Actor->Shield->SetCheckpointRestoreInProgress(false); Actor->Poise->SetCheckpointRestoreInProgress(false);
	F.Advance(10);
	TestTrue(TEXT("Committed shield resumes"), Actor->Attributes->GetShield() > 25.f);
	TestTrue(TEXT("Committed poise resumes"), Actor->Attributes->GetPoise() > 25.f);
	TestEqual(TEXT("Restore adds no second break presentation"), Actor->ShieldBreaks, 1);
	TestEqual(TEXT("Restore adds no second poise break presentation"), Actor->PoiseBreaks, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseRecoveryHandoffTest,
	"ProjectVelkorran.Campaign.PassiveDefense.PoiseRefillCallbackRetiresContinuation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseRecoveryHandoffTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); auto* Other = F.Actor(false);
	if (!Actor || !Other) { return false; }
	SovPassiveDefenseTests::SetResources(Actor, 100.f, 0.f);
	bool bMoved = false;
	auto& Delegate = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetPoiseAttribute());
	const FDelegateHandle Handle = Delegate.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (!bMoved && Change.NewValue > Change.OldValue)
		{
			bMoved = true; SovPassiveDefenseTests::MoveASC(Actor, Other);
		}
	});
	TestFalse(TEXT("Interrupted refill does not report recovery completion"), Actor->Poise->RecoverFromPoiseBreak());
	Delegate.Remove(Handle);
	TestTrue(TEXT("Real attribute notification moved ASC"), bMoved);
	TestEqual(TEXT("Already committed refill is not rolled back on new owner"), Actor->Attributes->GetPoise(), 100.f);
	TestFalse(TEXT("Old continuation adds no recovering tag"), Actor->OwnedASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Poise_Recovering));
	TestEqual(TEXT("Old continuation emits no recovery event"), Actor->PoiseRecoveries, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseReentrantShieldTest,
	"ProjectVelkorran.Campaign.PassiveDefense.ShieldNotificationCannotCancelReplacementTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseReentrantShieldTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	SovPassiveDefenseTests::SetResources(Actor, 50.f, 100.f);
	bool bReplaced = false;
	Actor->OnNextShieldChange = [&]()
	{
		bReplaced = true;
		Actor->Shield->SetCheckpointRestoreInProgress(true);
		Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 25.f);
		Actor->Shield->ResetForCheckpoint(); Actor->Shield->SetCheckpointRestoreInProgress(false);
	};
	// The outer "full shield" callback would formerly stop the replacement's timer.
	Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	TestTrue(TEXT("Public production delegate performed replacement"), bReplaced);
	F.Advance(10);
	TestTrue(TEXT("Replacement recharge survived outer continuation"), Actor->Attributes->GetShield() > 25.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseTagReentryTest,
	"ProjectVelkorran.Campaign.PassiveDefense.DeathDuringTagAdditionPreservesOtherContributors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseTagReentryTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	for (const bool bShield : {false, true})
	{
		SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
		const auto Tag = bShield ? FSovGameplayTags::Get().State_Shield_Broken : FSovGameplayTags::Get().State_Poise_Broken;
		Actor->OwnedASC->AddLooseGameplayTag(Tag);
		bool bKilled = false;
		auto& Changed = Actor->OwnedASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange);
		const auto Handle = Changed.AddLambda([&](const FGameplayTag CallbackTag, const int32 Count)
		{
			if (!bKilled && Count == 2)
			{
				bKilled = true;
				Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
			}
		});
		Actor->OwnedASC->SetNumericAttributeBase(bShield ? UNarrativeAttributeSetBase::GetShieldAttribute()
			: UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
		Changed.Remove(Handle);
		TestTrue(TEXT("Death occurred inside tag publication"), bKilled);
		TestEqual(TEXT("Cleanup retires only this component's contribution"), Actor->OwnedASC->GetTagCount(Tag), 1);
		TestEqual(TEXT("Retired break continuation emits no presentation"), bShield ? Actor->ShieldBreaks : Actor->PoiseBreaks, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseAttributeReplacementTest,
	"ProjectVelkorran.Campaign.PassiveDefense.UnregisteredAttributeSetCannotReceivePassiveWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseAttributeReplacementTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	SovPassiveDefenseTests::SetResources(Actor, 50.f, 50.f);
	Actor->OwnedASC->RemoveSpawnedAttribute(Actor->Attributes.Get());
	F.Advance(20);
	TestFalse(TEXT("Shield rejects removed registered attributes"), Actor->Shield->IsInitialized());
	TestFalse(TEXT("Poise rejects removed registered attributes"), Actor->Poise->IsInitialized());
	TestEqual(TEXT("Retired Shield storage untouched"), Actor->Attributes->GetShield(), 50.f);
	TestEqual(TEXT("Retired Poise storage untouched"), Actor->Attributes->GetPoise(), 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseFatalTagTest,
	"ProjectVelkorran.Campaign.PassiveDefense.FatalAndDeadTagsBlockPositiveHealthWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseFatalTagTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	for (const auto Tag : {FSovGameplayTags::Get().State_Fatal, FNarrativeGameplayTags::Get().State_IsDead})
	{
		SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
		SovPassiveDefenseTests::SetResources(Actor, 50.f, 0.f);
		Actor->OwnedASC->AddLooseGameplayTag(Tag); F.Advance(20);
		TestEqual(TEXT("Tagged actor retains positive fixture health"), Actor->Attributes->GetHealth(), 100.f);
		TestEqual(TEXT("Fatal/dead tag blocks shield recharge"), Actor->Attributes->GetShield(), 50.f);
		TestEqual(TEXT("Fatal/dead tag blocks broken recovery"), Actor->Attributes->GetPoise(), 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseOrdinaryReviveTest,
	"ProjectVelkorran.Campaign.PassiveDefense.OrdinaryNarrativeReviveRebindsAfterAttributeObservers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseOrdinaryReviveTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	for (const bool bBindAttributeObserverFirst : {false, true})
	{
		SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(false); if (!Actor) { return false; }
		const auto BindResources = [&]()
		{
			Actor->OwnedASC->OnDeathStateChanged.AddUniqueDynamic(Actor, &ASovPassiveDefenseTestActor::InitializeResourcesOnRevive);
		};
		if (bBindAttributeObserverFirst) { BindResources(); }
		Actor->Shield->InitializeWithAbilitySystem(Actor->OwnedASC); Actor->Poise->InitializeWithAbilitySystem(Actor->OwnedASC);
		if (!bBindAttributeObserverFirst) { BindResources(); }
		SovPassiveDefenseTests::SetResources(Actor, 0.f, 0.f);
		Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
		CastChecked<USovPassiveDefenseTestASC>(Actor->OwnedASC)->PublishDeathForTest();
		Actor->OwnedASC->Revive(); F.Advance(10);
		TestFalse(TEXT("Actual Narrative revive completed"), Actor->OwnedASC->IsDead());
		TestTrue(TEXT("Ordinary revive explicitly rebound shield"), Actor->Shield->IsInitialized());
		TestTrue(TEXT("Ordinary revive explicitly rebound poise"), Actor->Poise->IsInitialized());
		TestTrue(TEXT("Fresh shield schedule after observer completion"), Actor->Attributes->GetShield() > 25.f);
		TestTrue(TEXT("Fresh poise schedule after observer completion"), Actor->Attributes->GetPoise() > 25.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveDefenseRecoveryDamageReentryTest,
	"ProjectVelkorran.Campaign.PassiveDefense.RecursivePoiseDamageRejectsRecoveryAndRetries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveDefenseRecoveryDamageReentryTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovPassiveDefenseTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	bool bDamaged = false;
	auto& Changed = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetPoiseAttribute());
	const auto Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (!bDamaged && Change.NewValue > Change.OldValue)
		{
			bDamaged = true; Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
		}
	});
	TestFalse(TEXT("Recursive damage invalidates refill completion"), Actor->Poise->RecoverFromPoiseBreak());
	Changed.Remove(Handle);
	TestTrue(TEXT("Real notification applied recursive damage"), bDamaged);
	TestEqual(TEXT("No false recovery presentation"), Actor->PoiseRecoveries, 0);
	TestTrue(TEXT("Remains broken until fresh authored retry"), Actor->Poise->IsPoiseBroken());
	F.Advance(10);
	TestEqual(TEXT("Fresh fallback eventually recovers"), Actor->Attributes->GetPoise(), 100.f);
	TestEqual(TEXT("Only successful recovery is presented"), Actor->PoiseRecoveries, 1);
	return true;
}
#endif
