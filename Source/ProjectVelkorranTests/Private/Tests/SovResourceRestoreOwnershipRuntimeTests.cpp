// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPassiveDefenseTestFixtures.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ScopeExit.h"
#include "UObject/Script.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
namespace SovResourceRestoreOwnershipTests
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
	{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	ASovPassiveDefenseTestActor* Actor()
	{
		auto* Result = World ? World->SpawnActor<ASovPassiveDefenseTestActor>() : nullptr;
		if (Result) { Result->InitializeCombat(); }
		return Result;
	}
	void Advance()
	{
		for (int32 Index = 0; Index < 12; ++Index)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++FrameNumber);
			World->Tick(LEVELTICK_TimeOnly, 0.05f);
			// UE 5.7 TimeOnly world ticks intentionally skip the timer manager.
			World->GetTimerManager().Tick(0.05f);
		}
	}
};
FSovCombatResourceSnapshot Snapshot(ASovPassiveDefenseTestActor* Actor)
{
	FSovCombatResourceSnapshot Result;
	USovEncounterSnapshotLibrary::CaptureResources(Actor->OwnedASC, Result);
	Result.Health = 70.f; Result.Shield = 25.f; Result.Poise = 35.f; Result.Stamina = 45.f; Result.Echo = 20.f;
	USovEncounterSnapshotLibrary::RebaseAuthoredResourceCurrents(Actor->OwnedASC, Result);
	return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceRestoreFrozenInputTest,
	"ProjectVelkorran.Campaign.ResourceRestore.FrozenInputAndPerASCReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceRestoreFrozenInputTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovResourceRestoreOwnershipTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	auto Snapshot = SovResourceRestoreOwnershipTests::Snapshot(Actor);
	bool bNestedAttempted = false; bool bNestedAccepted = true;
	auto& Changed = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute());
	const auto Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (bNestedAttempted) { return; }
		bNestedAttempted = true;
		Snapshot.Health = 1.f; Snapshot.Echo = 90.f;
		bNestedAccepted = USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot);
	});
	TestTrue(TEXT("Frozen outer transaction completes"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	Changed.Remove(Handle);
	TestTrue(TEXT("Real attribute callback attempted nested restore"), bNestedAttempted);
	TestFalse(TEXT("Same ASC cannot overlap restore transactions"), bNestedAccepted);
	TestEqual(TEXT("Caller mutation cannot replace queued Health"), Actor->Attributes->GetHealth(), 70.f);
	TestEqual(TEXT("Caller mutation cannot replace queued Echo"), Actor->Attributes->GetEcho(), 20.f);
	TestTrue(TEXT("Reservation released after completion"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceRestoreActorInfoABATest,
	"ProjectVelkorran.Campaign.ResourceRestore.ActorInfoABAAbortsWithoutPoisoningPassiveBarrier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceRestoreActorInfoABATest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovResourceRestoreOwnershipTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	const auto Snapshot = SovResourceRestoreOwnershipTests::Snapshot(Actor);
	bool bRebound = false;
	auto& Changed = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute());
	const auto Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (bRebound) { return; } bRebound = true;
		Actor->OwnedASC->ClearActorInfo(); Actor->OwnedASC->InitAbilityActorInfo(Actor, Actor);
	});
	TestFalse(TEXT("Same pointer cannot conceal actor-info replacement"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	Changed.Remove(Handle);
	TestTrue(TEXT("Rebind occurred"), bRebound);
	TestEqual(TEXT("No continuation into later Stamina write"), Actor->Attributes->GetStamina(), 100.f);
	TestEqual(TEXT("Already committed Shield remains"), Actor->Attributes->GetShield(), 25.f);
	Actor->Shield->ResetForCheckpoint(); Actor->Poise->ResetForCheckpoint(); F.Advance();
	TestTrue(TEXT("Aborted lease did not poison explicit fresh Shield reset"), Actor->Attributes->GetShield() > 25.f);
	TestTrue(TEXT("New transaction can succeed after ABA failure"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceRestoreLifeABATest,
	"ProjectVelkorran.Campaign.ResourceRestore.ResourceCallbackLifeABAAborts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceRestoreLifeABATest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	for (const int32 Scenario : {0, 1, 2})
	{
		const bool bInitiallyDead = Scenario == 1;
		const bool bZeroOnly = Scenario == 2;
		SovResourceRestoreOwnershipTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
		const auto Snapshot = SovResourceRestoreOwnershipTests::Snapshot(Actor);
		if (bInitiallyDead)
		{
			Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
			CastChecked<USovPassiveDefenseTestASC>(Actor->OwnedASC)->PublishDeathForTest();
		}
		bool bChangedLife = false;
		const FGameplayAttribute Trigger = (bInitiallyDead || bZeroOnly) ? UNarrativeAttributeSetBase::GetShieldAttribute() : UNarrativeAttributeSetBase::GetEchoAttribute();
		auto& Changed = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(Trigger);
		const auto Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
		{
			if (bChangedLife) { return; } bChangedLife = true;
			Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
			if (!bZeroOnly) { Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f); }
		});
		TestFalse(TEXT("Only the controlled revive/Health stage may advance life"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
		Changed.Remove(Handle);
		TestTrue(TEXT("Actual resource callback changed life"), bChangedLife);
		if (bZeroOnly)
		{
			TestEqual(TEXT("Zero Health aborts before the later restore Health write can resurrect"), Actor->Attributes->GetHealth(), 0.f);
			TestEqual(TEXT("Zero Health aborts before later Stamina writes"), Actor->Attributes->GetStamina(), 100.f);
		}
		const float StoppedShield = Actor->Attributes->GetShield(); F.Advance();
		TestEqual(TEXT("Failure release does not resume passive Shield"), Actor->Attributes->GetShield(), StoppedShield);
		TestTrue(TEXT("Fresh explicit restore rebinds current life"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceRestoreControlledRevivalTest,
	"ProjectVelkorran.Campaign.ResourceRestore.ControlledRevivalAllowsExactlyOneLifeAdvance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceRestoreControlledRevivalTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	for (const bool bNPCAttributeObserver : {false, true})
	{
		SovResourceRestoreOwnershipTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
		const auto Snapshot = SovResourceRestoreOwnershipTests::Snapshot(Actor);
		Actor->OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
		CastChecked<USovPassiveDefenseTestASC>(Actor->OwnedASC)->PublishDeathForTest();
		const uint64 BeforeLife = Actor->Attributes->GetCombatLifeEpoch();
		if (bNPCAttributeObserver)
		{
			Actor->OwnedASC->OnDeathStateChanged.AddUniqueDynamic(Actor, &ASovPassiveDefenseTestActor::InitializeResourcesOnRevive);
		}
		TestTrue(TEXT("Canonical restore revives and commits resources"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
		TestEqual(TEXT("Exactly one accepted life advance"), Actor->Attributes->GetCombatLifeEpoch(), BeforeLife + 1);
		TestEqual(TEXT("Snapshot current wins after NPC default attributes"), Actor->Attributes->GetHealth(), 70.f);
		TestEqual(TEXT("Snapshot Shield current wins"), Actor->Attributes->GetShield(), 25.f);
		F.Advance();
		TestTrue(TEXT("Committed passive lifecycle resumes"), Actor->Attributes->GetShield() > 25.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceRestoreRegisteredAttributesTest,
	"ProjectVelkorran.Campaign.ResourceRestore.RemovedAttributesAbortRemainingWrites",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceRestoreRegisteredAttributesTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	SovResourceRestoreOwnershipTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
	const auto Snapshot = SovResourceRestoreOwnershipTests::Snapshot(Actor);
	bool bRemoved = false;
	auto& Changed = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute());
	const auto Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
	{
		if (!bRemoved) { bRemoved = true; Actor->OwnedASC->RemoveSpawnedAttribute(Actor->Attributes.Get()); }
	});
	// GAS reads the base once more after the value-change callback. Removing its
	// registered set deliberately triggers this engine diagnostic before our
	// transaction can reject the retired storage; later writes must still stop.
	int32 MissingSetEnsures = 0;
	{
		// Verify this exact, deliberately induced engine ensure without treating its
		// multi-line stack as unrelated test errors. Other ensures retain their handler.
		auto PreviousHandler = GetEnsureHandler();
		ON_SCOPE_EXIT { SetEnsureHandler(MoveTemp(PreviousHandler)); };
		SetEnsureHandler([&](const FEnsureHandlerArgs& Args)
		{
			if (FCStringAnsi::Strcmp(Args.Expression, "AttributeSet") == 0
				&& FCString::Strcmp(Args.Message, TEXT("FActiveGameplayEffectsContainer::SetAttributeBaseValue: Unable to get attribute set for attribute Shield")) == 0)
			{
				++MissingSetEnsures; return true;
			}
			return PreviousHandler ? PreviousHandler(Args) : false;
		});
		TestFalse(TEXT("Registered attributes cannot be replaced mid-restore"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	}
	TestEqual(TEXT("GAS reports exactly the intentionally retired Shield storage"), MissingSetEnsures, 1);
	Changed.Remove(Handle);
	TestTrue(TEXT("Attribute registration removed in real callback"), bRemoved);
	TestEqual(TEXT("Detached later resource not written"), Actor->Attributes->GetPoise(), 100.f);
	Actor->OwnedASC->AddAttributeSetSubobject(Actor->Attributes.Get());
	TestTrue(TEXT("Failed restore barrier was retired for valid retry"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceRestoreEndToEndConsistencyTest,
	"ProjectVelkorran.Campaign.ResourceRestore.LaterCallbackCannotRewriteEarlierResourceOrMaxima",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceRestoreEndToEndConsistencyTest::RunTest(const FString& Parameters)
{
	FEditorScriptExecutionGuard Script;
	for (const bool bChangeMaxima : {false, true})
	{
		SovResourceRestoreOwnershipTests::FWorld F; auto* Actor = F.Actor(); if (!Actor) { return false; }
		const auto Snapshot = SovResourceRestoreOwnershipTests::Snapshot(Actor);
		bool bMutated = false;
		auto& Changed = Actor->OwnedASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetEchoAttribute());
		const auto Handle = Changed.AddLambda([&](const FOnAttributeChangeData& Change)
		{
			if (bMutated) { return; } bMutated = true;
			Actor->OwnedASC->SetNumericAttributeBase(bChangeMaxima ? UNarrativeAttributeSetBase::GetMaxShieldAttribute()
				: UNarrativeAttributeSetBase::GetShieldAttribute(), bChangeMaxima ? 200.f : 5.f);
		});
		TestFalse(TEXT("Final resource vector must remain coherent"), USovEncounterSnapshotLibrary::RestoreResources(Actor->OwnedASC, Snapshot));
		Changed.Remove(Handle);
		TestTrue(TEXT("Later resource callback changed earlier state"), bMutated);
		const float Stopped = Actor->Attributes->GetShield(); F.Advance();
		TestEqual(TEXT("Failure does not restart reconciled passive timer"), Actor->Attributes->GetShield(), Stopped);
	}
	return true;
}
#endif
