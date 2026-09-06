// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDroneContinuationTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"

#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Presentation/SovReformationDroneSelfDestructPresentation.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FDroneWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		ASovAxiomRuntimeTestCharacter* Source = nullptr;
		ASovAxiomRuntimeTestCharacter* Target = nullptr;
		uint64 TimerFrame = GFrameCounter;
		FDroneWorld()
		{
			// One initialization only, including on UE 5.7 Mac. No BeginPlay/content dependency.
			const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->GetTimerManager().Tick(0.f);
			Source = Character(FVector::ZeroVector, 1);
			Target = Character(FVector(150., 0., 0.), 2);
		}
		~FDroneWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovAxiomRuntimeTestCharacter* Character(FVector Location, int32 Team)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor = World->SpawnActor<ASovDroneContinuationCharacter>(
				ASovDroneContinuationCharacter::StaticClass(), Location, FRotator::ZeroRotator, Params);
			if (Actor)
			{
				Actor->InitializeTestCombat(Team);
				Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(
					UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel, ECR_Block);
			}
			return Actor;
		}
		bool Valid() const { return World && Source && Target; }
		UNarrativeAbilitySystemComponent* ASC() const { return Source->GetNarrativeAbilitySystemComponent(); }
		void Tick(float Seconds)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++TimerFrame);
			World->GetTimerManager().Tick(Seconds);
		}
		template<class T> T* Grant(FGameplayAbilitySpecHandle& Handle)
		{
			Handle = ASC()->GiveAbility(FGameplayAbilitySpec(T::StaticClass(), 1));
			auto* Spec = ASC()->FindAbilitySpecFromHandle(Handle);
			return Spec ? Cast<T>(Spec->GetPrimaryInstance()) : nullptr;
		}
		float TargetShield() const
		{
			return Target->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
		}
		float SourceHealth() const { return ASC()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()); }
	};

	void Release(USovGameplayAbility_ReformationDroneWeaponBase* Ability)
	{
		if (auto* Gun = Cast<USovDroneContinuationGun>(Ability)) { Gun->FireGunBurstFromAim(); }
		else if (auto* Rocket = Cast<USovDroneContinuationRocket>(Ability)) { Rocket->LaunchRocketFromAim(); }
		else if (auto* Exploder = Cast<USovDroneContinuationExploder>(Ability)) { Exploder->StartSelfDestructRun(); }
	}

	template<class T> int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<T> It(World); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Count; } }
		return Count;
	}

	template<class T> bool ReleaseDisable(FAutomationTestBase& Test)
	{
		FDroneWorld F;
		if (!F.Valid()) { return false; }
		FGameplayAbilitySpecHandle Handle;
		T* Ability = F.Grant<T>(Handle);
		if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
		Ability->Hooks.OnRelease = [&F]() { F.ASC()->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled); };
		Release(Ability);
		Test.TestEqual(TEXT("The real release hook executed once"), Ability->Hooks.Releases, 1);
		Test.TestFalse(TEXT("Release-hook DeviceDisabled interrupts immediately"), Ability->IsActive());
		F.Tick(.5f);
		Test.TestEqual(TEXT("No stale burst, rocket or fuse reaches target"), F.TargetShield(), 100.f);
		Test.TestEqual(TEXT("No gunshot or rocket was spawned"), Ability->Hooks.Spawns, 0);
		Test.TestEqual(TEXT("Source is not killed"), F.SourceHealth(), 100.f);
		Test.TestEqual(TEXT("Interrupted self destruct never creates presentation"), CountActors<ASovReformationDroneSelfDestructPresentation>(F.World), 0);
		return true;
	}

	template<class T> bool ReleaseRestart(FAutomationTestBase& Test)
	{
		FDroneWorld F;
		if (!F.Valid()) { return false; }
		FGameplayAbilitySpecHandle Handle;
		T* Ability = F.Grant<T>(Handle);
		if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
		bool bRestarted = false;
		Ability->Hooks.OnRelease = [&F, Handle, &bRestarted]()
		{
			F.ASC()->CancelAbilityHandle(Handle);
			bRestarted = F.ASC()->TryActivateAbility(Handle, false);
		};
		Release(Ability);
		Test.TestTrue(TEXT("The same GAS spec restarts inside release hook"), bRestarted);
		Test.TestTrue(TEXT("Outer release cannot cancel the replacement"), Ability->IsActive());
		F.Tick(.5f);
		Test.TestEqual(TEXT("Unreleased replacement remains payload-free"), F.TargetShield(), 100.f);
		Test.TestEqual(TEXT("Retired release did not spawn a shot/rocket"), Ability->Hooks.Spawns, 0);
		Test.TestEqual(TEXT("Replacement receives no invented release"), Ability->Hooks.Releases, 1);
		Test.TestEqual(TEXT("Both starts use the real ability"), Ability->Hooks.Started, 2);
		F.ASC()->CancelAbilityHandle(Handle);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneReleaseDisableTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.ReleaseHookDeviceDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneReleaseDisableTest::RunTest(const FString& Parameters)
{
	return ReleaseDisable<USovDroneContinuationGun>(*this)
		&& ReleaseDisable<USovDroneContinuationRocket>(*this)
		&& ReleaseDisable<USovDroneContinuationExploder>(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneReleaseRestartTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.ReleaseHookRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneReleaseRestartTest::RunTest(const FString& Parameters)
{
	return ReleaseRestart<USovDroneContinuationGun>(*this)
		&& ReleaseRestart<USovDroneContinuationRocket>(*this)
		&& ReleaseRestart<USovDroneContinuationExploder>(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneReleaseOwnerTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.ReleaseHookOwnerAndLifeReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneReleaseOwnerTest::RunTest(const FString& Parameters)
{
	for (int32 Mutation = 0; Mutation < 3; ++Mutation)
	{
		FDroneWorld F;
		if (!F.Valid()) { return false; }
		auto* Replacement = F.Character(FVector(1000., 0., 0.), 1);
		if (!Replacement) { return false; }
		FGameplayAbilitySpecHandle Handle;
		auto* Ability = F.Grant<USovDroneContinuationRocket>(Handle);
		if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
		Ability->Hooks.OnRelease = [&F, Replacement, Mutation]()
		{
			if (Mutation < 2)
			{
				F.ASC()->InitAbilityActorInfo(Replacement, Replacement);
				if (Mutation == 1) { F.ASC()->InitAbilityActorInfo(F.Source, F.Source); }
			}
			else
			{
				F.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
				F.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
			}
		};
		Ability->LaunchRocketFromAim();
		TestEqual(TEXT("Rebind, ABA and restored life create no old rocket"), Ability->Hooks.Spawns, 0);
		TestFalse(TEXT("Stale owner action is retired"), Ability->IsActive());
		F.ASC()->InitAbilityActorInfo(F.Source, F.Source);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneImmediateInterruptionTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.ImmediateInterruptionsAndExternalTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneImmediateInterruptionTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationGun>(Handle);
	if (!Ability) { return false; }
	const auto& N = FNarrativeGameplayTags::Get();
	const auto& S = FSovGameplayTags::Get();
	for (FGameplayTag Tag : {N.State_IsDead, N.State_Interacting, N.State_SequencerControlled,
		N.State_Movement_Ragdoll, N.State_Weapon_BlockFiring, S.State_Fatal, S.State_Poise_Broken,
		S.State_Status_Frozen, S.State_Status_DeviceDisabled})
	{
		if (!TestTrue(TEXT("Clean spec can activate"), F.ASC()->TryActivateAbility(Handle, false))) { return false; }
		F.ASC()->AddLooseGameplayTag(Tag);
		TestFalse(TEXT("Interruption takes effect without advancing timers"), Ability->IsActive());
		TestEqual(TEXT("Cleanup preserves externally owned interruption tag"), F.ASC()->GetGameplayTagCount(Tag), 1);
		F.ASC()->RemoveLooseGameplayTag(Tag);
	}
	TestFalse(TEXT("Ended action releases its firing lane"), F.ASC()->HasMatchingGameplayTag(N.State_Weapon_IsFiring));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneBurstInterruptTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.BurstPresentationInterrupt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneBurstInterruptTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationGun>(Handle);
	if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	Ability->Hooks.OnSpawned = [&F]() { F.ASC()->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled); };
	Ability->FireGunBurstFromAim();
	const float AfterFirst = F.TargetShield();
	TestTrue(TEXT("First real trace/damage commits"), AfterFirst < 100.f);
	TestEqual(TEXT("Actual presentation construction callback executed"), Ability->Hooks.Spawns, 1);
	TestFalse(TEXT("Presentation callback interruption immediately ends burst"), Ability->IsActive());
	F.Tick(.5f);
	TestEqual(TEXT("No later shot after interruption"), F.TargetShield(), AfterFirst);
	F.ASC()->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneBurstRestartTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.BurstPresentationRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneBurstRestartTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationGun>(Handle);
	if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	bool bRestarted = false;
	Ability->Hooks.OnSpawned = [&F, Handle, &bRestarted]()
	{
		F.ASC()->CancelAbilityHandle(Handle);
		bRestarted = F.ASC()->TryActivateAbility(Handle, false);
	};
	Ability->FireGunBurstFromAim();
	const float AfterFirst = F.TargetShield();
	TestTrue(TEXT("Presentation callback restarts same instance"), bRestarted);
	F.Tick(.15f);
	TestTrue(TEXT("Old shot cannot schedule a timer that cancels the unreleased new burst"), Ability->IsActive());
	TestEqual(TEXT("Retired burst does not fire extra shot"), F.TargetShield(), AfterFirst);
	Ability->FireGunBurstFromAim();
	F.Tick(.11f); F.Tick(.11f); F.Tick(.21f);
	TestEqual(TEXT("Replacement retains all three shots and old presentation only one"), Ability->Hooks.Spawns, 4);
	TestTrue(TEXT("The final shot starts its own recovery interval"), Ability->IsActive());
	// The recovery timer is created by the last shot's timer callback, so its
	// 0.2-second interval starts after that dispatch completes.
	F.Tick(.21f);
	TestFalse(TEXT("Replacement owns and completes its recovery"), Ability->IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneRocketSpawnRestartTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.RocketCommitDoesNotFinishReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneRocketSpawnRestartTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationRocket>(Handle);
	if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	bool bRestarted = false;
	Ability->Hooks.OnSpawned = [&F, Handle, &bRestarted]()
	{
		F.ASC()->CancelAbilityHandle(Handle);
		bRestarted = F.ASC()->TryActivateAbility(Handle, false);
	};
	auto* Rocket = Ability->LaunchRocketFromAim();
	TestTrue(TEXT("Actual FinishSpawning callback restarts action"), bRestarted);
	TestNotNull(TEXT("Already committed projectile remains independently owned"), Rocket);
	F.Tick(.6f);
	TestTrue(TEXT("Old projectile return cannot finish new action recovery"), Ability->IsActive());
	TestEqual(TEXT("Only original rocket exists before new release"), Ability->Hooks.Spawns, 1);
	TestNotNull(TEXT("Replacement retains its own release gate"), Ability->LaunchRocketFromAim());
	TestEqual(TEXT("Exactly one rocket per activation"), Ability->Hooks.Spawns, 2);
	F.ASC()->CancelAbilityHandle(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneFuseInterruptionTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.WarningInterruptionAndRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneFuseInterruptionTest::RunTest(const FString& Parameters)
{
	for (bool bRestart : {false, true})
	{
		FDroneWorld F;
		if (!F.Valid()) { return false; }
		FGameplayAbilitySpecHandle Handle;
		auto* Ability = F.Grant<USovDroneContinuationExploder>(Handle);
		if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
		bool bRestarted = false;
		Ability->Hooks.OnWarning = [&F, Handle, bRestart, &bRestarted]()
		{
			if (bRestart)
			{
				F.ASC()->CancelAbilityHandle(Handle);
				bRestarted = F.ASC()->TryActivateAbility(Handle, false);
			}
			else { F.ASC()->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled); }
		};
		Ability->StartSelfDestructRun();
		TestEqual(TEXT("Real in-range pursuit reaches warning"), Ability->Hooks.Warnings, 1);
		if (bRestart) { TestTrue(TEXT("Warning hook restarts action"), bRestarted); }
		F.Tick(.5f);
		TestEqual(TEXT("Old fuse causes no outward blast"), F.TargetShield(), 100.f);
		TestEqual(TEXT("Old fuse causes no self death"), F.SourceHealth(), 100.f);
		TestEqual(TEXT("Replacement stays active but unreleased"), Ability->IsActive(), bRestart);
		for (TActorIterator<ASovReformationDroneSelfDestructPresentation> It(F.World); It; ++It)
		{
			TestTrue(TEXT("Retired presentation has explicit cancelled phase"),
				It->GetPresentationState().Phase == ESovReformationDroneSelfDestructPhase::Cancelled);
		}
		F.ASC()->CancelAbilityHandle(Handle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneFuseLifeReplacementTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.CommittedBlastCannotKillRestoredSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneFuseLifeReplacementTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationExploder>(Handle);
	if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	bool bRestored = false;
	auto& ShieldChanged = F.Target->GetNarrativeAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(
		UNarrativeAttributeSetBase::GetShieldAttribute());
	const FDelegateHandle Observer = ShieldChanged.AddLambda([&F, &bRestored](const FOnAttributeChangeData& Change)
	{
		if (!bRestored && Change.NewValue < Change.OldValue)
		{
			bRestored = true;
			F.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
			F.ASC()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
		}
	});
	Ability->StartSelfDestructRun();
	F.Tick(.25f);
	ShieldChanged.Remove(Observer);
	TestTrue(TEXT("Real outward damage callback restores source life"), bRestored);
	TestTrue(TEXT("Committed blast remains delivered"), F.TargetShield() < 100.f);
	TestEqual(TEXT("Old blast does not fatal-hit restored life"), F.SourceHealth(), 50.f);
	TestFalse(TEXT("Old action remains ended"), Ability->IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneCommittedDetonationTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.CommittedBlastAndArmedTargetLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneCommittedDetonationTest::RunTest(const FString& Parameters)
{
	for (bool bLoseTarget : {false, true})
	{
		FDroneWorld F;
		if (!F.Valid()) { return false; }
		FGameplayAbilitySpecHandle Handle;
		auto* Ability = F.Grant<USovDroneContinuationExploder>(Handle);
		if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
		Ability->StartSelfDestructRun();
		TestEqual(TEXT("Telegraph begins before damage"), Ability->Hooks.Warnings, 1);
		TestEqual(TEXT("Warning itself does no damage"), F.TargetShield(), 100.f);
		if (bLoseTarget) { F.Target->Destroy(); }
		F.Tick(.25f);
		TestEqual(TEXT("Committed blast uses ordinary fatal self damage"), F.SourceHealth(), 0.f);
		TestFalse(TEXT("Detonated action releases firing lane"), Ability->IsActive());
		if (!bLoseTarget) { TestTrue(TEXT("Outward blast hits living hostile once"), F.TargetShield() < 100.f); }
		int32 Finalized = 0;
		for (TActorIterator<ASovReformationDroneSelfDestructPresentation> It(F.World); It; ++It)
		{
			const auto State = It->GetPresentationState();
			if (State.Phase == ESovReformationDroneSelfDestructPhase::Detonated && State.bDetonationFinalized) { ++Finalized; }
		}
		TestEqual(TEXT("Committed presentation survives synchronous fatal cleanup"), Finalized, 1);
		F.Tick(.5f);
		TestEqual(TEXT("No second warning or blast"), Ability->Hooks.Warnings, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneWindupOwnershipTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.WindupTimerRejectsAvatarABA",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneWindupOwnershipTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	auto* Replacement = F.Character(FVector(1000., 0., 0.), 1);
	if (!Replacement) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationGun>(Handle);
	if (!Ability) { return false; }
	Ability->UseAutomaticRelease();
	if (!F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	F.ASC()->InitAbilityActorInfo(Replacement, Replacement);
	F.ASC()->InitAbilityActorInfo(F.Source, F.Source);
	F.Tick(.2f);
	TestFalse(TEXT("Automatic timer retires old ownership even after ABA rebind"), Ability->IsActive());
	TestEqual(TEXT("Old release event is not dispatched"), Ability->Hooks.Releases, 0);
	TestEqual(TEXT("No stale windup damage"), F.TargetShield(), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneDeferredEndTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.InvalidAndScopeLockedEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneDeferredEndTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationGun>(Handle);
	if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	Ability->InvalidEnd();
	TestTrue(TEXT("Invalid end cannot clear active payload state"), Ability->CanContinueForTest());
	Ability->LockEnd();
	Ability->RequestEnd();
	TestTrue(TEXT("GAS flag remains active until unlock"), Ability->IsActive());
	TestFalse(TEXT("Continuation is retired before scope unlock"), Ability->CanContinueForTest());
	Ability->FireGunBurstFromAim();
	TestEqual(TEXT("Deferred end cannot release"), Ability->Hooks.Releases, 0);
	Ability->UnlockEnd();
	TestFalse(TEXT("Queued virtual end clears GAS action"), Ability->IsActive());
	TestEqual(TEXT("Cleanup occurs once"), Ability->Hooks.Ends, 1);
	TestFalse(TEXT("Queued end removes owned firing tag"), F.ASC()->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsFiring));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDronePointSpecContinuationTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.PointSpecConstructionInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDronePointSpecContinuationTest::RunTest(const FString& Parameters)
{
	for (bool bRestart : {false, true})
	{
		FDroneWorld F;
		if (!F.Valid()) { return false; }
		auto* ASC = Cast<USovDroneContinuationASC>(F.ASC());
		FGameplayAbilitySpecHandle Handle;
		auto* Ability = F.Grant<USovDroneContinuationGun>(Handle);
		if (!ASC || !Ability || !ASC->TryActivateAbility(Handle, false)) { return false; }
		bool bSpecCallback = false;
		bool bRestarted = false;
		ASC->OnMakeSpec = [&, Handle]()
		{
			bSpecCallback = true;
			if (bRestart)
			{
				ASC->CancelAbilityHandle(Handle);
				bRestarted = ASC->TryActivateAbility(Handle, false);
			}
			else { ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled); }
		};
		Ability->FireGunBurstFromAim();
		TestTrue(TEXT("Production invokes the real virtual MakeOutgoingSpec boundary"), bSpecCallback);
		if (bRestart) { TestTrue(TEXT("Spec callback restarts the same action"), bRestarted); }
		TestEqual(TEXT("No old spec is applied after interruption"), F.TargetShield(), 100.f);
		TestEqual(TEXT("Old shot publishes no presentation"), Ability->Hooks.Spawns, 0);
		F.Tick(.5f);
		TestEqual(TEXT("Stale spec cannot finish replacement or revive interrupted action"), Ability->IsActive(), bRestart);
		ASC->CancelAbilityHandle(Handle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneBlastSpecOwnershipTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.BlastAndFatalSpecConstructionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneBlastSpecOwnershipTest::RunTest(const FString& Parameters)
{
	for (int32 SpecsBeforeCallback : {0, 1})
	{
		for (bool bAvatarABA : {false, true})
		{
			FDroneWorld F;
			if (!F.Valid()) { return false; }
			auto* Replacement = F.Character(FVector(1000., 0., 0.), 1);
			auto* ASC = Cast<USovDroneContinuationASC>(F.ASC());
			FGameplayAbilitySpecHandle Handle;
			auto* Ability = F.Grant<USovDroneContinuationExploder>(Handle);
			if (!ASC || !Replacement || !Ability || !ASC->TryActivateAbility(Handle, false)) { return false; }
			bool bSpecCallback = false;
			ASC->SpecsBeforeCallback = SpecsBeforeCallback;
			ASC->OnMakeSpec = [&]()
			{
				bSpecCallback = true;
				if (bAvatarABA)
				{
					ASC->InitAbilityActorInfo(Replacement, Replacement);
					ASC->InitAbilityActorInfo(F.Source, F.Source);
				}
				else
				{
					ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
					ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 50.f);
				}
			};
			Ability->StartSelfDestructRun();
			F.Tick(.25f);
			TestTrue(TEXT("Real outward/fatal spec construction callback executed"), bSpecCallback);
			TestEqual(TEXT("Neither outward nor fatal spec construction can kill replacement source life"), F.SourceHealth(), bAvatarABA ? 100.f : 50.f);
			if (SpecsBeforeCallback == 0) { TestEqual(TEXT("Uncommitted outward spec is rejected"), F.TargetShield(), 100.f); }
			else { TestTrue(TEXT("Already committed outward damage is retained"), F.TargetShield() < 100.f); }
			ASC->CancelAbilityHandle(Handle);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneFatalConfigurationSnapshotTest,
	"ProjectVelkorran.Campaign.Drone.Continuation.FatalConfigurationSurvivesPresentationMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneFatalConfigurationSnapshotTest::RunTest(const FString& Parameters)
{
	FDroneWorld F;
	if (!F.Valid()) { return false; }
	FGameplayAbilitySpecHandle Handle;
	auto* Ability = F.Grant<USovDroneContinuationExploder>(Handle);
	if (!Ability || !F.ASC()->TryActivateAbility(Handle, false)) { return false; }
	Ability->Hooks.OnDetonated = [Ability]() { Ability->DiscardDamageConfiguration(); };
	Ability->StartSelfDestructRun();
	F.Tick(.25f);
	TestEqual(TEXT("Real FinalizeDetonation authored event ran"), Ability->Hooks.Detonations, 1);
	TestTrue(TEXT("Outward committed damage remains"), F.TargetShield() < 100.f);
	TestEqual(TEXT("Fatal self hit uses frozen effect class despite authored mutation"), F.SourceHealth(), 0.f);
	TestFalse(TEXT("Fatal cleanup still ends action"), Ability->IsActive());
	return true;
}
#endif
