// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDefenseLifecycleTestFixtures.h"

#include "Components/SovDeflectionComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"

#if WITH_AUTOMATION_TESTS
namespace
{
struct FDefenseLifecycleWorld
{
	UWorld* World = nullptr;
	uint64 FrameNumber = GFrameCounter;
	FDefenseLifecycleWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false));
		World->GetTimerManager().Tick(0.f);
	}
	~FDefenseLifecycleWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
	ASovDefenseLifecycleTestCharacter* Character()
	{
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = World ? World->SpawnActor<ASovDefenseLifecycleTestCharacter>(
			ASovDefenseLifecycleTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn) : nullptr;
		if (Actor) { Actor->InitializeDefenseCombat(); }
		return Actor;
	}
	void Advance(const float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed + KINDA_SMALL_NUMBER < Seconds; Elapsed += .1f)
		{
			TGuardValue<uint64> Frame(GFrameCounter, ++FrameNumber);
			World->Tick(LEVELTICK_TimeOnly, FMath::Min(.1f, Seconds - Elapsed));
		}
	}
};
FActiveGameplayEffectHandle GrantDefenseTag(UNarrativeAbilitySystemComponent* ASC, const FGameplayTag Tag)
{
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
	Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.f);
	Spec.DynamicGrantedTags.AddTag(Tag);
	return ASC->ApplyGameplayEffectSpecToSelf(Spec);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDeflectionReentrantStartTest,
	"ProjectVelkorran.Campaign.Deflection.ReentrantStartAndOwnedTags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDeflectionReentrantStartTest::RunTest(const FString& Parameters)
{
	FDefenseLifecycleWorld F;
	auto* Character = F.Character();
	if (!TestNotNull(TEXT("Real defense character"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
	const auto TagCallback = ASC->RegisterGameplayTagEvent(S.State_Deflecting, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([Character](FGameplayTag Tag, int32 Count) { if (Count > 0) { Character->TestDeflection->EndDeflection(); } });
	TestFalse(TEXT("Synchronous tag-add cancellation rejects window start"), Character->TestDeflection->BeginDeflection());
	TestEqual(TEXT("Cancellation removes the owned tag before Begin returns"), ASC->GetGameplayTagCount(S.State_Deflecting), 0);
	ASC->RegisterGameplayTagEvent(S.State_Deflecting, EGameplayTagEventType::NewOrRemoved).Remove(TagCallback);

	Character->DeflectionHandle = ASC->GiveAbility(FGameplayAbilitySpec(USovDefenseLifecycleDeflection::StaticClass(), 1));
	Character->bCancelDeflectionOnStart = true;
	ASC->TryActivateAbility(Character->DeflectionHandle);
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Character->DeflectionHandle);
	auto* Ability = Spec ? Cast<USovDefenseLifecycleDeflection>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Real GAS deflection instance"), Ability)) { return false; }
	TestFalse(TEXT("Start delegate cancellation ends GAS activation"), Ability->IsActive());
	TestFalse(TEXT("Start delegate cancellation closes its component window"), Character->TestDeflection->IsDeflectionWindowOpen());
	TestEqual(TEXT("Cancelled startup never reaches ability-start Blueprint hook"), Ability->StartedHookCount, 0);
	TestEqual(TEXT("Cancellation releases GAS Busy"), ASC->GetGameplayTagCount(N.State_Busy), 0);
	F.Advance(.5f);
	TestFalse(TEXT("Cancelled recovery task cannot reopen defense"), Character->TestDeflection->IsDeflectionWindowOpen());

	TestTrue(TEXT("Subsequent activation remains usable"), ASC->TryActivateAbility(Character->DeflectionHandle));
	TestTrue(TEXT("Ability's own Busy contribution admits its component window"), Character->TestDeflection->IsDeflectionWindowOpen());
	const auto ForeignDeflection = GrantDefenseTag(ASC, S.State_Deflecting);
	const auto Interrupt = GrantDefenseTag(ASC, S.State_Status_Frozen);
	TestFalse(TEXT("Gameplay-effect interruption ends active ability"), Ability->IsActive());
	TestEqual(TEXT("Owned cleanup preserves foreign Deflecting contribution"), ASC->GetGameplayTagCount(S.State_Deflecting), 1);
	ASC->RemoveActiveGameplayEffect(ForeignDeflection); ASC->RemoveActiveGameplayEffect(Interrupt);
	TestTrue(TEXT("Fresh activation after removing effects"), ASC->TryActivateAbility(Character->DeflectionHandle));
	ASC->AddLooseGameplayTag(N.State_Busy);
	TestFalse(TEXT("Additional Busy cancels without confusing ability's own count"), Ability->IsActive());
	TestEqual(TEXT("External Busy survives cleanup"), ASC->GetGameplayTagCount(N.State_Busy), 1);
	ASC->RemoveLooseGameplayTag(N.State_Busy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovResourceDeathReviveTest,
	"ProjectVelkorran.Campaign.Resources.DeathReviveAndRetiredAvatar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovResourceDeathReviveTest::RunTest(const FString& Parameters)
{
	FDefenseLifecycleWorld F; auto* Character = F.Character(); auto* Replacement = F.Character();
	if (!Character || !Replacement) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 20.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 40.f);
	F.Advance(3.5f);
	TestTrue(TEXT("Living owner actually recharges Shield before death"), Character->TestShield->GetShield() > 20.f);
	TestTrue(TEXT("Living owner actually regenerates Poise before death"), Character->TestPoise->GetPoise() > 40.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	ASC->AddLooseGameplayTag(N.State_IsDead); ASC->AddLooseGameplayTag(S.State_Fatal);
	const float DeadShield = Character->TestShield->GetShield(), DeadPoise = Character->TestPoise->GetPoise();
	TestFalse(TEXT("Health-zero event immediately stops Shield timer"), Character->TestShield->IsRecharging());
	TestFalse(TEXT("Health-zero event immediately stops Poise timer"), Character->TestPoise->IsRegenerating());
	TestFalse(TEXT("Dead actor rejects authored Poise-recovery notify"), Character->TestPoise->RecoverFromPoiseBreak());
	F.Advance(5.f);
	TestEqual(TEXT("Dead Shield unchanged after old timers would fire"), Character->TestShield->GetShield(), DeadShield);
	TestEqual(TEXT("Dead Poise unchanged after old timers would fire"), Character->TestPoise->GetPoise(), DeadPoise);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	ASC->RemoveLooseGameplayTag(N.State_IsDead);
	F.Advance(.2f);
	TestFalse(TEXT("Fatal contribution still blocks revived Health"), Character->TestShield->IsRecharging());
	ASC->RemoveLooseGameplayTag(S.State_Fatal);
	TestTrue(TEXT("Revive starts a fresh Shield delay"), Character->TestShield->GetSecondsUntilRecharge() > 2.8f);
	TestTrue(TEXT("Revive starts a fresh Poise delay"), Character->TestPoise->GetSecondsUntilRegeneration() > 1.8f);
	F.Advance(1.f);
	TestEqual(TEXT("Revive does not catch up Shield for dead elapsed time"), Character->TestShield->GetShield(), DeadShield);
	TestEqual(TEXT("Revive does not catch up Poise for dead elapsed time"), Character->TestPoise->GetPoise(), DeadPoise);
	F.Advance(3.f);
	TestTrue(TEXT("Revived Shield eventually recharges"), Character->TestShield->GetShield() > DeadShield);
	TestTrue(TEXT("Revived Poise eventually regenerates"), Character->TestPoise->GetPoise() > DeadPoise);

	const float RetiredShield = Character->TestShield->GetShield(), RetiredPoise = Character->TestPoise->GetPoise();
	ASC->InitAbilityActorInfo(Character, Replacement);
	F.Advance(4.f);
	TestEqual(TEXT("Retired avatar cannot recharge new ASC avatar's Shield"), Character->TestShield->GetShield(), RetiredShield);
	TestEqual(TEXT("Retired avatar cannot regenerate new ASC avatar's Poise"), Character->TestPoise->GetPoise(), RetiredPoise);
	TestFalse(TEXT("Retired-avatar timers stop"), Character->TestPoise->IsRegenerating() || Character->TestShield->IsRecharging());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPoiseDeathFallbackTest,
	"ProjectVelkorran.Campaign.Resources.BrokenDeathDoesNotRecover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPoiseDeathFallbackTest::RunTest(const FString& Parameters)
{
	FDefenseLifecycleWorld F; auto* Character = F.Character(); if (!Character) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent(); const auto& S = FSovGameplayTags::Get();
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	TestTrue(TEXT("Native Poise decrease enters Broken"), Character->TestPoise->IsPoiseBroken());
	const auto ForeignBroken = GrantDefenseTag(ASC, S.State_Poise_Broken);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestEqual(TEXT("Death removes only component-owned Broken count"), ASC->GetGameplayTagCount(S.State_Poise_Broken), 1);
	F.Advance(3.f);
	TestEqual(TEXT("Dead broken fallback cannot refill Poise"), Character->TestPoise->GetPoise(), 0.f);
	TestFalse(TEXT("Dead broken fallback cannot grant recovery immunity"), ASC->HasMatchingGameplayTag(S.State_Poise_Recovering));
	ASC->RemoveActiveGameplayEffect(ForeignBroken);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneReleaseInterruptionTest,
	"ProjectVelkorran.Campaign.Drone.SynchronousReleaseInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneReleaseInterruptionTest::RunTest(const FString& Parameters)
{
	FDefenseLifecycleWorld F; auto* Character = F.Character(); if (!Character) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent(); const auto& S = FSovGameplayTags::Get();
	const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovDefenseLifecycleRocket::StaticClass(), 1));
	TestTrue(TEXT("Native rocket activation"), ASC->TryActivateAbility(Handle));
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	auto* Ability = Spec ? Cast<USovDefenseLifecycleRocket>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Real native rocket ability"), Ability)) { return false; }
	Ability->bInterruptOnRelease = true;
	TestNull(TEXT("Release hook transient disable prevents projectile spawn"), Ability->LaunchRocket(
		FTransform(FRotator::ZeroRotator, FVector(300.f, 0.f, 0.f)), FVector(1000.f, 0.f, 0.f)));
	TestEqual(TEXT("Release hook ran through actual ProcessEvent"), Ability->ReleasedHookCount, 1);
	TestFalse(TEXT("Transient disable ends immediately even after tag removal"), Ability->IsActive());
	int32 Rockets = 0; for (TActorIterator<ASovReformationDroneRocketProjectile> It(F.World); It; ++It) { ++Rockets; }
	TestEqual(TEXT("Interrupted release leaves no deferred rocket actor"), Rockets, 0);
	TestTrue(TEXT("Attack lane is usable after cancellation"), ASC->TryActivateAbility(Handle));
	const auto InterruptedByEffect = GrantDefenseTag(ASC, S.State_Status_DeviceDisabled);
	TestFalse(TEXT("Effect-owned disable cancels during windup immediately"), Ability->IsActive());
	TestTrue(TEXT("Ability cleanup preserves external disable"), ASC->HasMatchingGameplayTag(S.State_Status_DeviceDisabled));
	ASC->RemoveActiveGameplayEffect(InterruptedByEffect);
	F.Advance(1.f);
	TestFalse(TEXT("Cancelled windup leaves no active lifecycle"), Ability->IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneCommittedBlastTest,
	"ProjectVelkorran.Campaign.Drone.CommittedBlastSurvivesSourceInterruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneCommittedBlastTest::RunTest(const FString& Parameters)
{
	FDefenseLifecycleWorld F;
	auto* Source = F.Character(); auto* First = F.Character(); auto* Second = F.Character();
	if (!Source || !First || !Second) { return false; }
	First->InitializeTestCombat(1); Second->InitializeTestCombat(1);
	First->SetActorLocation(FVector(100.f, 100.f, 0.f)); Second->SetActorLocation(FVector(100.f, -100.f, 0.f));
	for (auto* Target : {First, Second})
	{
		Target->BlastSourceToInterrupt = Source;
		Target->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsTarget.AddUniqueDynamic(
			Target, &ASovDefenseLifecycleTestCharacter::InterruptBlastSource);
	}
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovDefenseLifecycleSelfDestruct::StaticClass(), 1));
	TestTrue(TEXT("Native self-destruct activates"), ASC->TryActivateAbility(Handle));
	const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	auto* Ability = Spec ? Cast<USovDefenseLifecycleSelfDestruct>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Real self-destruct ability"), Ability)) { return false; }
	Ability->StartSelfDestructRun();
	F.Advance(1.f);
	TestFalse(TEXT("First outward hit synchronously interrupts source ability"), Ability->IsActive());
	TestTrue(TEXT("First hostile receives committed blast"), First->ResolvedHitCount > 0);
	TestTrue(TEXT("Other hostile still receives committed blast after source interruption"), Second->ResolvedHitCount > 0);
	TestTrue(TEXT("First hostile lost Shield"), First->TestShield->GetShield() < 100.f);
	TestTrue(TEXT("Other hostile lost Shield"), Second->TestShield->GetShield() < 100.f);
	return true;
}
#endif
