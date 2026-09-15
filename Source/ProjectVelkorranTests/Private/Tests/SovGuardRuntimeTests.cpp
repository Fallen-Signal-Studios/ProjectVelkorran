// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovGuardRuntimeTestFixtures.h"

#include "Components/SovEchoComponent.h"
#include "Components/SovGuardComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FGuardRuntimeWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard AllowProductionReceivers;
#endif
		UWorld* World = nullptr;
		FGuardRuntimeWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			}
		}
		~FGuardRuntimeWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovGuardRuntimeTestCharacter* Character(FVector Location = FVector::ZeroVector)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Character = World ? World->SpawnActor<ASovGuardRuntimeTestCharacter>(
				ASovGuardRuntimeTestCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn) : nullptr;
			if (Character) { Character->InitializeGuardCombat(); }
			return Character;
		}
		/**
		 * Advances production timers only; no actor tick or authored content is involved. A timer armed
		 * outside a timer tick stays pending until the next tick, so callers leave one step of margin.
		 */
		void Advance(const float Seconds)
		{
			for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += .05f)
			{
				TGuardValue<uint64> ScopedFrame(GFrameCounter, ++Frame);
				World->GetTimerManager().Tick(.05f);
			}
		}
		uint64 Frame = GFrameCounter;
	};

	FGameplayAbilitySpecHandle GrantGuard(ASovGuardRuntimeTestCharacter* Character, bool bOwnBusy = false)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = bOwnBusy
			? USovGuardRuntimeBusyTestAbility::StaticClass() : USovGameplayAbility_TarrikGuard::StaticClass();
		FGameplayAbilitySpec Spec(AbilityClass, 1);
		Spec.InputPressed = true;
		Spec.GetDynamicSpecSourceTags().AddTag(FNarrativeGameplayTags::Get().Narrative_Input_AltAttack);
		return Character->GetNarrativeAbilitySystemComponent()->GiveAbility(Spec);
	}

	bool GuardAbilityActive(UNarrativeAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle)
	{
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		return Spec && Spec->IsActive();
	}

	/** A fresh hold: the held-input task checks the spec's pressed state on activation. */
	bool PressGuard(UNarrativeAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle)
	{
		if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle)) { Spec->InputPressed = true; }
		ASC->TryActivateAbility(Handle);
		return GuardAbilityActive(ASC, Handle);
	}

	/** Narrative's production tag-input release path, as the player controller routes it. */
	void ReleaseGuard(UNarrativeAbilitySystemComponent* ASC)
	{
		ASC->AbilityInputTagReleased(FNarrativeGameplayTags::Get().Narrative_Input_AltAttack);
	}

	int32 OwnedGuardTagCount(UNarrativeAbilitySystemComponent* ASC)
	{
		const auto& Tags = FSovGameplayTags::Get();
		return ASC->GetGameplayTagCount(Tags.State_Guarding) + ASC->GetGameplayTagCount(Tags.State_PerfectGuard)
			+ ASC->GetGameplayTagCount(Tags.State_Guard_CounterWindow) + ASC->GetGameplayTagCount(Tags.State_Guard_Broken)
			+ ASC->GetGameplayTagCount(FNarrativeGameplayTags::Get().State_Busy);
	}

	float Stamina(UNarrativeAbilitySystemComponent* ASC)
	{
		return ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute());
	}

	void SetStamina(UNarrativeAbilitySystemComponent* ASC, const float Value)
	{
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), Value);
	}

	FActiveGameplayEffectHandle GrantEffectTag(UNarrativeAbilitySystemComponent* ASC, FGameplayTag Tag)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.f);
		Spec.DynamicGrantedTags.AddTag(Tag);
		return ASC->ApplyGameplayEffectSpecToSelf(Spec);
	}

	void DealGuardTestDamage(ASovGuardRuntimeTestCharacter* Source, ASovGuardRuntimeTestCharacter* Target,
		const bool bGuardCounter = false, const FGameplayTag& GuardClass = FGameplayTag())
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = UNarrativeAttributeSetBase::GetDamageAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(20.f);
		Effect->Modifiers.Add(Modifier);
		UNarrativeAbilitySystemComponent* SourceASC = Source->GetNarrativeAbilitySystemComponent();
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(Source, Source);
		FGameplayEffectSpec Spec(Effect, Context, 1.f);
		Spec.AddDynamicAssetTag(GuardClass.IsValid() ? GuardClass : FSovGameplayTags::Get().Damage_GuardClass_Standard);
		if (bGuardCounter) { Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Source_GuardCounter); }
		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeAdmissionTest,
	"ProjectVelkorran.Campaign.Guard.AdmissionAndDirectInterrupts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeAdmissionTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Guard runtime fixture"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto& Narrative = FNarrativeGameplayTags::Get();
	const auto& Sov = FSovGameplayTags::Get();
	const FGameplayTag Interrupts[] = { Narrative.State_IsDead, Narrative.State_Interacting,
		Narrative.State_Movement_Ragdoll, Narrative.State_SequencerControlled, Narrative.State_Busy,
		Sov.State_Fatal, Sov.State_Poise_Broken, Sov.State_Guard_Broken,
		Sov.State_EchoAbility_Active, Sov.State_Deflecting };
	for (const FGameplayTag Tag : Interrupts)
	{
		ASC->AddLooseGameplayTag(Tag);
		TestFalse(FString::Printf(TEXT("Direct guard rejects %s"), *Tag.ToString()), Character->TestGuard->BeginGuard());
		ASC->RemoveLooseGameplayTag(Tag);
		TestTrue(TEXT("Guard begins after blocker clears"), Character->TestGuard->BeginGuard());
		ASC->AddLooseGameplayTag(Tag);
		TestFalse(TEXT("Direct guard ends when blocker arrives"), Character->TestGuard->IsGuarding());
		TestFalse(TEXT("Interrupt clears perfect timing tag"), Character->TestGuard->IsPerfectDefenseWindowOpen());
		ASC->RemoveLooseGameplayTag(Tag);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeEffectCancellationTest,
	"ProjectVelkorran.Campaign.Guard.GASCancellationAndEffectOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeEffectCancellationTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Guard fixture"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
	const auto Handle = GrantGuard(Character, true);
	TestTrue(TEXT("Child owning Busy can activate through native component admission"), ASC->TryActivateAbility(Handle));
	TestTrue(TEXT("Held guard is active"), GuardAbilityActive(ASC, Handle));
	TestEqual(TEXT("Only the ability contributes Busy"), ASC->GetGameplayTagCount(Busy), 1);
	const auto BusyEffect = GrantEffectTag(ASC, Busy);
	TestTrue(TEXT("External active effect was applied"), BusyEffect.IsValid());
	TestFalse(TEXT("A second Busy contribution cancels active GAS guard"), GuardAbilityActive(ASC, Handle));
	TestFalse(TEXT("Cancellation clears component guarding"), Character->TestGuard->IsGuarding());
	TestEqual(TEXT("External effect Busy survives guard cleanup"), ASC->GetGameplayTagCount(Busy), 1);
	TestFalse(TEXT("Busy still prevents reactivation"), ASC->TryActivateAbility(Handle));
	ASC->RemoveActiveGameplayEffect(BusyEffect);
	TestTrue(TEXT("Ability restarts after external Busy expires"), ASC->TryActivateAbility(Handle));
	const auto Ragdoll = GrantEffectTag(ASC, FNarrativeGameplayTags::Get().State_Movement_Ragdoll);
	TestFalse(TEXT("Effect-granted ragdoll cancels guard"), GuardAbilityActive(ASC, Handle));
	ASC->RemoveActiveGameplayEffect(Ragdoll);
	TestTrue(TEXT("Guard starts again after interruption ends"), ASC->TryActivateAbility(Handle));
	ASC->ClearAbility(Handle);
	TestFalse(TEXT("Removing active ability clears guard"), Character->TestGuard->IsGuarding());
	TestEqual(TEXT("Ability removal removes owned Busy"), ASC->GetGameplayTagCount(Busy), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeReentrantStartTest,
	"ProjectVelkorran.Campaign.Guard.ReentrantStartAndTagOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeReentrantStartTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	if (!TestNotNull(TEXT("Guard fixture"), Character)) { return false; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const FGameplayTag GuardingTag = FSovGameplayTags::Get().State_Guarding;
	const FDelegateHandle ReentrantTagHandle = ASC
		->RegisterGameplayTagEvent(GuardingTag, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([Character](FGameplayTag Tag, int32 NewCount)
		{
			if (NewCount > 0) { Character->TestGuard->EndGuard(); }
		});
	TestFalse(TEXT("Cancellation during initial GAS tag dispatch rejects begin"), Character->TestGuard->BeginGuard());
	TestEqual(TEXT("Reentrant tag removal leaks no owned contribution"), ASC->GetGameplayTagCount(GuardingTag), 0);
	TestFalse(TEXT("Cancelled tag dispatch cannot open perfect timing later"), Character->TestGuard->IsPerfectDefenseWindowOpen());
	ASC->RegisterGameplayTagEvent(GuardingTag, EGameplayTagEventType::NewOrRemoved).Remove(ReentrantTagHandle);
	Character->GuardEndCount = 0;
	const auto Handle = GrantGuard(Character);
	Character->bInterruptGuardOnStart = true;
	ASC->TryActivateAbility(Handle);
	TestEqual(TEXT("Start callback ran once"), Character->GuardStartCount, 1);
	TestEqual(TEXT("Reentrant cancellation ends component once"), Character->GuardEndCount, 1);
	TestFalse(TEXT("Interrupted begin does not continue the ability"), GuardAbilityActive(ASC, Handle));
	TestFalse(TEXT("Interrupted begin leaves no guard tag"), Character->TestGuard->IsGuarding());
	TestFalse(TEXT("Interrupted begin leaves no perfect tag"), Character->TestGuard->IsPerfectDefenseWindowOpen());
	Character->bInterruptGuardOnStart = false;
	ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Ragdoll);
	TestTrue(TEXT("Clean next activation succeeds"), ASC->TryActivateAbility(Handle));
	const auto& Tags = FSovGameplayTags::Get();
	ASC->AddLooseGameplayTag(Tags.State_Guarding);
	ASC->AddLooseGameplayTag(Tags.State_PerfectGuard);
	ASC->CancelAbilityHandle(Handle);
	TestEqual(TEXT("Cleanup preserves external guarding contribution"), ASC->GetGameplayTagCount(Tags.State_Guarding), 1);
	TestEqual(TEXT("Cleanup preserves external perfect contribution"), ASC->GetGameplayTagCount(Tags.State_PerfectGuard), 1);
	ASC->RemoveLooseGameplayTag(Tags.State_Guarding);
	ASC->RemoveLooseGameplayTag(Tags.State_PerfectGuard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeLastStaminaTest,
	"ProjectVelkorran.Campaign.Guard.PerfectDefenseLastStamina",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeLastStaminaTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	auto* Source = Fixture.Character(FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Defender"), Character) || !TestNotNull(TEXT("Attacker"), Source)) { return false; }
	Source->TestTeam = 0; // Damage routing requires a hostile source, including guard/counter packets.
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto Handle = GrantGuard(Character);
	TestTrue(TEXT("Guard begins with sufficient start stamina"), ASC->TryActivateAbility(Handle));
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 5.f);
	DealGuardTestDamage(Source, Character);
	TestEqual(TEXT("Real damage transaction resolved"), Character->ResolvedHitCount, 1);
	TestTrue(TEXT("Last-stamina defense still succeeds perfectly"), Character->LastDamageResult.bPerfectDefense);
	TestTrue(TEXT("Last-stamina defense uses normal guard-break flow"), Character->LastDamageResult.bGuardBroken);
	TestEqual(TEXT("Perfect guard spends exactly the remaining five stamina"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), 0.f);
	TestEqual(TEXT("Perfect guard blocks shield damage"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
	TestEqual(TEXT("Perfect guard blocks health damage"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	TestEqual(TEXT("Successful timing grants its Echo reward"), Character->TestEcho->GetEcho(), 12.f);
	TestFalse(TEXT("Exhaustion opens no counter window"), Character->TestGuard->IsCounterWindowOpen());
	TestFalse(TEXT("Exhaustion ends the active GAS guard"), GuardAbilityActive(ASC, Handle));
	TestFalse(TEXT("Exhaustion clears guarding"), Character->TestGuard->IsGuarding());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeCounterTransitionTest,
	"ProjectVelkorran.Campaign.Guard.CounterSurvivesAttackBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeCounterTransitionTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	auto* Source = Fixture.Character(FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Defender"), Character) || !TestNotNull(TEXT("Attacker"), Source)) { return false; }
	Source->TestTeam = 0; // Damage routing requires a hostile source, including guard/counter packets.
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto Handle = GrantGuard(Character);
	TestTrue(TEXT("Guard activation succeeds"), ASC->TryActivateAbility(Handle));
	DealGuardTestDamage(Source, Character);
	TestTrue(TEXT("Successful perfect guard opens counter"), Character->TestGuard->IsCounterWindowOpen());
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
	TestFalse(TEXT("Counter attack Busy ends sustained Guard"), GuardAbilityActive(ASC, Handle));
	TestTrue(TEXT("Counter opportunity survives the attack transition"), Character->TestGuard->IsCounterWindowOpen());
	DealGuardTestDamage(Character, Source, true);
	TestFalse(TEXT("Successful counter consumes its window"), Character->TestGuard->IsCounterWindowOpen());
	TestEqual(TEXT("Perfect plus counter rewards are preserved"), Character->TestEcho->GetEcho(), 22.f);
	DealGuardTestDamage(Character, Source, true);
	TestEqual(TEXT("Another hit cannot reuse the consumed counter reward"), Character->TestEcho->GetEcho(), 22.f);
	ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeStaminaRecoveryTest,
	"ProjectVelkorran.Campaign.Guard.StaminaAdmissionImpactAndBreakRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeStaminaRecoveryTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	auto* Source = Fixture.Character(FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Defender"), Character) || !TestNotNull(TEXT("Attacker"), Source)) { return false; }
	Source->TestTeam = 0; // Damage routing requires a hostile source.
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto& Tags = FSovGameplayTags::Get();
	const auto Handle = GrantGuard(Character);

	SetStamina(ASC, 7.9f);
	TestFalse(TEXT("Direct guard refuses start below the minimum Stamina"), Character->TestGuard->BeginGuard());
	TestFalse(TEXT("GAS guard refuses start below the minimum Stamina"), PressGuard(ASC, Handle));
	TestEqual(TEXT("A refused start leaves no owned tag or Busy"), OwnedGuardTagCount(ASC), 0);
	TestEqual(TEXT("A refused start broadcasts no guard start"), Character->GuardStartCount, 0);
	SetStamina(ASC, 8.f);
	TestTrue(TEXT("Guard starts at exactly the minimum Stamina"), PressGuard(ASC, Handle));
	ReleaseGuard(ASC);
	TestFalse(TEXT("Production input release ends Guard"), GuardAbilityActive(ASC, Handle));
	TestFalse(TEXT("Release clears guarding"), Character->TestGuard->IsGuarding());

	SetStamina(ASC, 100.f);
	DealGuardTestDamage(Source, Character);
	const float UnguardedShieldLoss = 100.f - ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
	TestTrue(TEXT("Unguarded baseline hit damages Shield"), UnguardedShieldLoss > 0.f);
	TestTrue(TEXT("Guard restarts after release"), PressGuard(ASC, Handle));
	Fixture.Advance(.3f);
	TestFalse(TEXT("Perfect timing expires on its timer"), Character->TestGuard->IsPerfectDefenseWindowOpen());
	TestTrue(TEXT("Perfect timing expiry does not end the held Guard"), GuardAbilityActive(ASC, Handle));
	const float ShieldBeforeGuardedHit = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
	DealGuardTestDamage(Source, Character);
	TestTrue(TEXT("Ordinary impact is guarded"), Character->LastDamageResult.bGuarded);
	TestFalse(TEXT("Ordinary impact is not perfect"), Character->LastDamageResult.bPerfectDefense);
	TestEqual(TEXT("Ordinary impact spends the clamped 8-20 cost"), Stamina(ASC), 90.f);
	TestEqual(TEXT("Guard mitigates Shield damage to the configured quarter"),
		ShieldBeforeGuardedHit - ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), UnguardedShieldLoss * .25f);
	TestFalse(TEXT("Ordinary impact opens no counter"), Character->TestGuard->IsCounterWindowOpen());
	TestTrue(TEXT("Affordable impact keeps Guard held"), GuardAbilityActive(ASC, Handle));

	SetStamina(ASC, 9.f);
	DealGuardTestDamage(Source, Character);
	TestTrue(TEXT("An unaffordable impact breaks posture"), Character->LastDamageResult.bGuardBroken);
	TestFalse(TEXT("An exhausted guard does not mitigate"), Character->LastDamageResult.bGuarded);
	TestEqual(TEXT("Exhaustion spends only the remaining Stamina"), Stamina(ASC), 0.f);
	TestFalse(TEXT("Exhaustion ends the GAS guard"), GuardAbilityActive(ASC, Handle));
	TestFalse(TEXT("Exhaustion clears guarding"), Character->TestGuard->IsGuarding());
	TestEqual(TEXT("Exhaustion owns one broken-state contribution"), ASC->GetGameplayTagCount(Tags.State_Guard_Broken), 1);
	SetStamina(ASC, 100.f);
	TestFalse(TEXT("Broken posture refuses GAS restart"), PressGuard(ASC, Handle));
	TestFalse(TEXT("Broken posture refuses direct restart"), Character->TestGuard->BeginGuard());
	Fixture.Advance(1.f);
	TestEqual(TEXT("Broken posture expires rather than sticking"), ASC->GetGameplayTagCount(Tags.State_Guard_Broken), 0);
	SetStamina(ASC, 0.f);
	TestFalse(TEXT("After the break, empty Stamina still refuses start"), PressGuard(ASC, Handle));
	SetStamina(ASC, 8.f);
	TestTrue(TEXT("Recovered Stamina restarts Guard"), PressGuard(ASC, Handle));
	ReleaseGuard(ASC);

	SetStamina(ASC, 100.f);
	TestTrue(TEXT("Guard starts before a heavy attack"), PressGuard(ASC, Handle));
	Fixture.Advance(.3f);
	DealGuardTestDamage(Source, Character, false, Tags.Damage_GuardClass_Heavy);
	TestTrue(TEXT("A heavy attack outside perfect timing breaks Guard"), Character->LastDamageResult.bGuardBroken);
	TestFalse(TEXT("A heavy attack receives no ordinary mitigation"), Character->LastDamageResult.bGuarded);
	TestEqual(TEXT("Heavy break still spends the impact cost"), Stamina(ASC), 90.f);
	TestFalse(TEXT("Heavy break ends the GAS guard"), GuardAbilityActive(ASC, Handle));
	Fixture.Advance(1.f);
	TestTrue(TEXT("Guard restarts after a heavy break expires"), PressGuard(ASC, Handle));
	ReleaseGuard(ASC);
	TestEqual(TEXT("No guard, perfect, counter, broken or Busy state remains"), OwnedGuardTagCount(ASC), 0);
	TestEqual(TEXT("Every guard start has exactly one end"), Character->GuardEndCount, Character->GuardStartCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeCounterLifecycleTest,
	"ProjectVelkorran.Campaign.Guard.CounterWindowReleaseExpiryAndSingleOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeCounterLifecycleTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	auto* Source = Fixture.Character(FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Defender"), Character) || !TestNotNull(TEXT("Attacker"), Source)) { return false; }
	Source->TestTeam = 0; // Damage routing requires a hostile source, including counter packets.
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	const auto& Tags = FSovGameplayTags::Get();
	const FGameplayTag Busy = FNarrativeGameplayTags::Get().State_Busy;
	const auto Handle = GrantGuard(Character);
	const auto PerfectThenRelease = [&](const TCHAR* Step)
	{
		TestTrue(FString::Printf(TEXT("%s: guard starts"), Step), PressGuard(ASC, Handle));
		DealGuardTestDamage(Source, Character);
		TestTrue(FString::Printf(TEXT("%s: perfect defense"), Step), Character->LastDamageResult.bPerfectDefense);
		TestEqual(FString::Printf(TEXT("%s: exactly one counter contribution"), Step),
			ASC->GetGameplayTagCount(Tags.State_Guard_CounterWindow), 1);
		ReleaseGuard(ASC);
		TestFalse(FString::Printf(TEXT("%s: release ends Guard"), Step), GuardAbilityActive(ASC, Handle));
		TestTrue(FString::Printf(TEXT("%s: release preserves the counter"), Step), Character->TestGuard->IsCounterWindowOpen());
	};

	PerfectThenRelease(TEXT("First"));
	TestEqual(TEXT("Perfect guard rewards Echo"), Character->TestEcho->GetEcho(), 12.f);
	ASC->AddLooseGameplayTag(Busy);
	ASC->AddLooseGameplayTag(Busy);
	TestTrue(TEXT("A second attack-state contribution does not kill the counter"), Character->TestGuard->IsCounterWindowOpen());
	ASC->RemoveLooseGameplayTag(Busy);
	ASC->RemoveLooseGameplayTag(Busy);
	TestTrue(TEXT("Attack states ending do not kill the counter"), Character->TestGuard->IsCounterWindowOpen());

	PerfectThenRelease(TEXT("Re-guard during counter"));
	TestEqual(TEXT("A second perfect guard rewards once more"), Character->TestEcho->GetEcho(), 24.f);
	DealGuardTestDamage(Character, Source, true);
	TestFalse(TEXT("The single counter is consumed"), Character->TestGuard->IsCounterWindowOpen());
	TestEqual(TEXT("A refreshed window pays one counter reward"), Character->TestEcho->GetEcho(), 34.f);
	DealGuardTestDamage(Character, Source, true);
	TestEqual(TEXT("No duplicate counter remains to pay"), Character->TestEcho->GetEcho(), 34.f);

	PerfectThenRelease(TEXT("Expiry"));
	Fixture.Advance(1.f);
	TestEqual(TEXT("Counter expires and removes its contribution"), ASC->GetGameplayTagCount(Tags.State_Guard_CounterWindow), 0);
	DealGuardTestDamage(Character, Source, true);
	TestEqual(TEXT("An expired counter pays nothing"), Character->TestEcho->GetEcho(), 46.f);

	PerfectThenRelease(TEXT("Incapacity"));
	ASC->AddLooseGameplayTag(Tags.State_Poise_Broken);
	TestFalse(TEXT("Poise break clears the counter"), Character->TestGuard->IsCounterWindowOpen());
	ASC->RemoveLooseGameplayTag(Tags.State_Poise_Broken);

	PerfectThenRelease(TEXT("Another defense"));
	ASC->AddLooseGameplayTag(Tags.State_Deflecting);
	TestFalse(TEXT("Another defense clears the counter"), Character->TestGuard->IsCounterWindowOpen());
	ASC->RemoveLooseGameplayTag(Tags.State_Deflecting);

	TestEqual(TEXT("No guard, perfect, counter, broken or Busy state remains"), OwnedGuardTagCount(ASC), 0);
	TestTrue(TEXT("Guard can activate again after every counter path"), PressGuard(ASC, Handle));
	ReleaseGuard(ASC);
	TestEqual(TEXT("Every guard start has exactly one end"), Character->GuardEndCount, Character->GuardStartCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovGuardRuntimeRebindCleanupTest,
	"ProjectVelkorran.Campaign.Guard.RebindingReleasesOwnedStateAndReactivates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovGuardRuntimeRebindCleanupTest::RunTest(const FString& Parameters)
{
	FGuardRuntimeWorld Fixture;
	auto* Character = Fixture.Character();
	auto* Source = Fixture.Character(FVector(200.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Defender"), Character) || !TestNotNull(TEXT("Attacker"), Source)) { return false; }
	Source->TestTeam = 0;
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	auto* OtherASC = Source->GetNarrativeAbilitySystemComponent();
	const auto& Tags = FSovGameplayTags::Get();
	const auto Handle = GrantGuard(Character);

	TestTrue(TEXT("Guard starts"), PressGuard(ASC, Handle));
	DealGuardTestDamage(Source, Character);
	TestTrue(TEXT("Held guard owns a counter"), Character->TestGuard->IsCounterWindowOpen() && Character->TestGuard->IsGuarding());
	TestTrue(TEXT("Component rebinds to a replacement ASC"), Character->TestGuard->InitializeWithAbilitySystem(OtherASC));
	TestEqual(TEXT("Rebinding removes every owned tag and Busy from the old ASC"), OwnedGuardTagCount(ASC), 0);
	TestFalse(TEXT("Rebinding ends the held GAS guard"), GuardAbilityActive(ASC, Handle));
	TestEqual(TEXT("No owned state moves to the replacement ASC"), OwnedGuardTagCount(OtherASC), 0);
	TestTrue(TEXT("Component rebinds to the original ASC"), Character->TestGuard->InitializeWithAbilitySystem(ASC));
	TestTrue(TEXT("Guard reactivates after rebinding"), PressGuard(ASC, Handle));

	Fixture.Advance(.3f);
	SetStamina(ASC, 9.f);
	DealGuardTestDamage(Source, Character);
	TestEqual(TEXT("Exhaustion owns broken posture"), ASC->GetGameplayTagCount(Tags.State_Guard_Broken), 1);
	TestTrue(TEXT("Component rebinds while broken"), Character->TestGuard->InitializeWithAbilitySystem(OtherASC));
	TestEqual(TEXT("Rebinding clears broken posture from the old ASC"), OwnedGuardTagCount(ASC), 0);
	TestTrue(TEXT("Component returns to the original ASC"), Character->TestGuard->InitializeWithAbilitySystem(ASC));
	Fixture.Advance(1.f);
	TestEqual(TEXT("A cancelled break timer applies nothing later"), OwnedGuardTagCount(ASC) + OwnedGuardTagCount(OtherASC), 0);
	SetStamina(ASC, 100.f);
	TestTrue(TEXT("Guard reactivates after a rebinding during break"), PressGuard(ASC, Handle));
	ReleaseGuard(ASC);
	TestEqual(TEXT("Every guard start has exactly one end"), Character->GuardEndCount, Character->GuardStartCount);
	return true;
}
#endif
