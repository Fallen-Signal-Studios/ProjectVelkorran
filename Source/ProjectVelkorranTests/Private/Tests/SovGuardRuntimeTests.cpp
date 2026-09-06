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

#if WITH_AUTOMATION_TESTS
namespace
{
	struct FGuardRuntimeWorld
	{
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
	};

	FGameplayAbilitySpecHandle GrantGuard(ASovGuardRuntimeTestCharacter* Character, bool bOwnBusy = false)
	{
		const TSubclassOf<UGameplayAbility> AbilityClass = bOwnBusy
			? USovGuardRuntimeBusyTestAbility::StaticClass() : USovGameplayAbility_TarrikGuard::StaticClass();
		FGameplayAbilitySpec Spec(AbilityClass, 1);
		Spec.InputPressed = true;
		return Character->GetNarrativeAbilitySystemComponent()->GiveAbility(Spec);
	}

	bool GuardAbilityActive(UNarrativeAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle Handle)
	{
		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		return Spec && Spec->IsActive();
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
		const bool bGuardCounter = false)
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
		Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_GuardClass_Standard);
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
#endif
