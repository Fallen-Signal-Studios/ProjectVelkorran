// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovPassiveResourceTests
{
	struct FWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard AllowProductionReceivers;
#endif
		UWorld* World = nullptr;
		uint64 FixtureFrame = GFrameCounter;
		FWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
				.ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
				World->GetTimerManager().Tick(0.f);
			}
		}
		~FWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovAxiomRuntimeTestCharacter* Character()
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Result = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn) : nullptr;
			if (Result) { Result->InitializeTestCombat(0); }
			return Result;
		}
		void Advance(const float Seconds)
		{
			for (float Elapsed = 0.f; Elapsed + KINDA_SMALL_NUMBER < Seconds; Elapsed += 0.1f)
			{
				TGuardValue<uint64> Frame(GFrameCounter, ++FixtureFrame);
				World->Tick(LEVELTICK_TimeOnly, FMath::Min(0.1f, Seconds - Elapsed));
			}
		}
	};

	template <typename T> T* Attach(AActor* Owner, UAbilitySystemComponent* ASC)
	{
		auto* Component = NewObject<T>(Owner);
		Owner->AddInstanceComponent(Component);
		Component->RegisterComponent();
		Component->InitializeWithAbilitySystem(ASC);
		return Component;
	}

	UNarrativeAbilitySystemComponent* ReplacementASC(AActor* Owner, const float Shield, const float Poise)
	{
		auto* ASC = NewObject<UNarrativeAbilitySystemComponent>(Owner);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->AddAttributeSetSubobject(NewObject<UNarrativeAttributeSetBase>(ASC));
		ASC->InitAbilityActorInfo(Owner, Owner);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), Shield);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), Poise);
		return ASC;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveResourceAvatarEpochTest,
	"ProjectVelkorran.Campaign.PassiveResources.RetiredAvatarAndReturnRequireFreshBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveResourceAvatarEpochTest::RunTest(const FString& Parameters)
{
	using namespace SovPassiveResourceTests;
	FWorld Fixture;
	auto* Owner = Fixture.Character();
	auto* Other = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner) || !TestNotNull(TEXT("Other avatar"), Other)) { return false; }
	auto* ASC = Owner->GetNarrativeAbilitySystemComponent();
	auto* Shield = Attach<USovShieldComponent>(Owner, ASC);
	auto* Poise = Attach<USovPoiseComponent>(Owner, ASC);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 30.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 40.f);
	ASC->InitAbilityActorInfo(Owner, Other);
	TestFalse(TEXT("Retired shield no longer reads replacement avatar"), Shield->IsInitialized());
	TestFalse(TEXT("Retired poise cannot accept break recovery"), Poise->RecoverFromPoiseBreak());
	Fixture.Advance(4.f);
	TestEqual(TEXT("Retired shield delay cannot refill replacement ASC"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), 30.f);
	TestEqual(TEXT("Retired poise delay cannot refill replacement ASC"), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute()), 40.f);
	ASC->InitAbilityActorInfo(Owner, Owner);
	TestFalse(TEXT("Same pointer after away/back still needs fresh shield binding"), Shield->IsInitialized());
	TestFalse(TEXT("Same pointer after away/back still needs fresh poise binding"), Poise->IsInitialized());
	TestTrue(TEXT("Explicit shield readiness binds new epoch"), Shield->InitializeWithAbilitySystem(ASC));
	TestTrue(TEXT("Explicit poise readiness binds new epoch"), Poise->InitializeWithAbilitySystem(ASC));
	Fixture.Advance(0.5f);
	TestTrue(TEXT("Fresh shield binding can regenerate"), Shield->GetShield() > 30.f);
	TestTrue(TEXT("Fresh poise binding can regenerate"), Poise->GetPoise() > 40.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveResourceDeathTest,
	"ProjectVelkorran.Campaign.PassiveResources.ZeroHealthRetiresRecoveryUntilCheckpointReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveResourceDeathTest::RunTest(const FString& Parameters)
{
	using namespace SovPassiveResourceTests;
	FWorld Fixture;
	auto* Owner = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
	auto* ASC = Owner->GetNarrativeAbilitySystemComponent();
	auto* Shield = Attach<USovShieldComponent>(Owner, ASC);
	auto* Poise = Attach<USovPoiseComponent>(Owner, ASC);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 30.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	TestTrue(TEXT("Break fallback is armed before death"), Poise->GetSecondsUntilBreakRecovery() > 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 0.f);
	TestFalse(TEXT("Zero health rejects authored break completion"), Poise->RecoverFromPoiseBreak());
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	Fixture.Advance(4.f);
	TestEqual(TEXT("Health revival cannot resume old shield timer"), Shield->GetShield(), 30.f);
	TestEqual(TEXT("Health revival cannot resume old break fallback"), Poise->GetPoise(), 0.f);
	Shield->ResetForCheckpoint();
	Poise->ResetForCheckpoint();
	TestTrue(TEXT("Explicit checkpoint starts a fresh shield delay"), Shield->GetSecondsUntilRecharge() > 0.f);
	Fixture.Advance(1.f);
	TestEqual(TEXT("Fresh break fallback can recover living owner"), Poise->GetPoise(), 100.f);
	TestTrue(TEXT("Fresh recovery owns immunity window"), Poise->IsPoiseRecovering());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveResourceCheckpointHoldTest,
	"ProjectVelkorran.Campaign.PassiveResources.CheckpointHoldDefersAllRegeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveResourceCheckpointHoldTest::RunTest(const FString& Parameters)
{
	using namespace SovPassiveResourceTests;
	FWorld Fixture;
	auto* Owner = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
	auto* ASC = Owner->GetNarrativeAbilitySystemComponent();
	auto* Shield = Attach<USovShieldComponent>(Owner, ASC);
	auto* Poise = Attach<USovPoiseComponent>(Owner, ASC);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 30.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	Shield->SetCheckpointRestoreInProgress(true);
	Poise->SetCheckpointRestoreInProgress(true);
	Fixture.Advance(4.f);
	TestEqual(TEXT("Hold cancels shield delay immediately"), Shield->GetShield(), 30.f);
	TestEqual(TEXT("Hold cancels broken fallback immediately"), Poise->GetPoise(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 60.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 80.f);
	Shield->ResetForCheckpoint();
	Poise->ResetForCheckpoint();
	Fixture.Advance(4.f);
	TestEqual(TEXT("Shield reset waits for accepted restore completion"), Shield->GetShield(), 60.f);
	TestEqual(TEXT("Poise reset waits for accepted restore completion"), Poise->GetPoise(), 80.f);
	Shield->SetCheckpointRestoreInProgress(false);
	Poise->SetCheckpointRestoreInProgress(false);
	TestTrue(TEXT("Shield completion starts full new delay"), Shield->GetSecondsUntilRecharge() > 2.9f);
	TestTrue(TEXT("Poise completion starts full new delay"), Poise->GetSecondsUntilRegeneration() > 1.9f);
	Fixture.Advance(3.5f);
	TestTrue(TEXT("Shield resumes only after new delay"), Shield->GetShield() > 60.f);
	TestTrue(TEXT("Poise resumes only after new delay"), Poise->GetPoise() > 80.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveResourceReadyEpochTest,
	"ProjectVelkorran.Campaign.PassiveResources.ReadyEpochRebindRejectsRetiredRestoreScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveResourceReadyEpochTest::RunTest(const FString& Parameters)
{
	using namespace SovPassiveResourceTests;
	FWorld Fixture;
	auto* Owner = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
	auto* ASC = Owner->GetNarrativeAbilitySystemComponent();
	auto* Shield = Attach<USovShieldComponent>(Owner, ASC);
	auto* Poise = Attach<USovPoiseComponent>(Owner, ASC);
	Shield->SetCheckpointRestoreInProgress(true);
	Poise->SetCheckpointRestoreInProgress(true);
	const uint64 OldShieldBinding = Shield->GetCheckpointRestoreGeneration();
	const uint64 OldPoiseBinding = Poise->GetCheckpointRestoreGeneration();
	ASC->SetCharacterReadyEpoch(ASC->GetCharacterReadyEpoch() + 1);
	TestTrue(TEXT("Shield rebinds after initial ready epoch publication"), Shield->IsInitialized());
	TestTrue(TEXT("Poise rebinds after initial ready epoch publication"), Poise->IsInitialized());
	TestTrue(TEXT("Shield binding generation advances"), Shield->GetCheckpointRestoreGeneration() != OldShieldBinding);
	TestTrue(TEXT("Poise binding generation advances"), Poise->GetCheckpointRestoreGeneration() != OldPoiseBinding);
	Shield->SetCheckpointRestoreInProgress(true);
	Poise->SetCheckpointRestoreInProgress(true);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 30.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 40.f);
	Shield->ResetForCheckpoint();
	Poise->ResetForCheckpoint();
	Shield->EndCheckpointRestore(OldShieldBinding, true);
	Poise->EndCheckpointRestore(OldPoiseBinding, true);
	Fixture.Advance(4.f);
	TestEqual(TEXT("Old shield restore cleanup cannot release new hold"), Shield->GetShield(), 30.f);
	TestEqual(TEXT("Old poise restore cleanup cannot release new hold"), Poise->GetPoise(), 40.f);
	Shield->EndCheckpointRestore(Shield->GetCheckpointRestoreGeneration(), true);
	Poise->EndCheckpointRestore(Poise->GetCheckpointRestoreGeneration(), true);
	TestTrue(TEXT("Current shield hold can complete"), Shield->GetSecondsUntilRecharge() > 0.f);
	TestTrue(TEXT("Current poise hold can complete"), Poise->GetSecondsUntilRegeneration() > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveResourceReentrantTagTest,
	"ProjectVelkorran.Campaign.PassiveResources.TagCallbacksCannotContinueOnReplacementBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveResourceReentrantTagTest::RunTest(const FString& Parameters)
{
	using namespace SovPassiveResourceTests;
	FWorld Fixture;
	auto* Owner = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner)) { return false; }
	auto* ASC = Owner->GetNarrativeAbilitySystemComponent();
	auto* Replacement = ReplacementASC(Owner, 75.f, 40.f);
	const auto& Tags = FSovGameplayTags::Get();
	Replacement->AddLooseGameplayTag(Tags.State_Shield_RechargeBlocked);
	Replacement->AddLooseGameplayTag(Tags.State_Poise_RegenBlocked);
	auto* Shield = Attach<USovShieldComponent>(Owner, ASC);
	auto* Poise = Attach<USovPoiseComponent>(Owner, ASC);
	ASC->AddLooseGameplayTag(Tags.State_Shield_Broken); // unrelated contributor survives rebind
	bool bShieldRebound = false;
	const auto ShieldListener = ASC->RegisterGameplayTagEvent(Tags.State_Shield_Broken, EGameplayTagEventType::AnyCountChange)
		.AddLambda([&](FGameplayTag, int32 Count)
		{
			if (Count == 2 && !bShieldRebound)
			{
				bShieldRebound = true;
				Shield->InitializeWithAbilitySystem(Replacement);
			}
		});
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	TestTrue(TEXT("Shield tag listener rebound during break transition"), bShieldRebound);
	TestEqual(TEXT("Old shield payload cannot replace new state"), Shield->GetShield(), 75.f);
	TestFalse(TEXT("New shield binding is not broken"), Shield->IsShieldBroken());
	TestEqual(TEXT("Only original owned shield tag is released"), ASC->GetTagCount(Tags.State_Shield_Broken), 1);
	ASC->RegisterGameplayTagEvent(Tags.State_Shield_Broken, EGameplayTagEventType::AnyCountChange).Remove(ShieldListener);
	bool bPoiseRebound = false;
	const auto PoiseListener = ASC->RegisterGameplayTagEvent(Tags.State_Poise_Recovering, EGameplayTagEventType::NewOrRemoved)
		.AddLambda([&](FGameplayTag, int32 Count)
		{
			if (Count > 0 && !bPoiseRebound)
			{
				bPoiseRebound = true;
				Poise->InitializeWithAbilitySystem(Replacement);
			}
		});
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 0.f);
	TestFalse(TEXT("Old recovery reports interruption after callback rebind"), Poise->RecoverFromPoiseBreak());
	TestTrue(TEXT("Poise listener rebound during recovery transition"), bPoiseRebound);
	Fixture.Advance(3.f);
	TestEqual(TEXT("Retired fallback/recovery cannot refill replacement poise"), Poise->GetPoise(), 40.f);
	TestEqual(TEXT("Replacement owns its actual state"), Poise->GetPoiseState(), ESovPoiseState::Pressured);
	TestEqual(TEXT("Old recovering contribution released"), ASC->GetTagCount(Tags.State_Poise_Recovering), 0);
	TestEqual(TEXT("Old recovery cannot add replacement immunity"), Replacement->GetTagCount(Tags.State_Poise_Recovering), 0);
	ASC->RegisterGameplayTagEvent(Tags.State_Poise_Recovering, EGameplayTagEventType::NewOrRemoved).Remove(PoiseListener);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPassiveResourceDamageReceiptTest,
	"ProjectVelkorran.Campaign.PassiveResources.ShieldRechargeRejectsReplayedAndRetiredDamageReceipts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovPassiveResourceDamageReceiptTest::RunTest(const FString& Parameters)
{
	using namespace SovPassiveResourceTests;
	FWorld Fixture;
	auto* Owner = Fixture.Character();
	auto* Source = Fixture.Character();
	if (!TestNotNull(TEXT("Owner"), Owner) || !TestNotNull(TEXT("Source"), Source)) { return false; }
	Source->TestTeam = 1;
	auto* ASC = Owner->GetNarrativeAbilitySystemComponent();
	auto* SourceASC = Source->GetNarrativeAbilitySystemComponent();
	auto* Shield = Attach<USovShieldComponent>(Owner, ASC);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	Fixture.Advance(1.f);
	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(Source, Source);
	FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
	Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 5.f);
	SourceASC->ApplyGameplayEffectSpecToTarget(Spec, ASC);
	const FSovDamageResult Receipt = Owner->LastDamageResult;
	TestTrue(TEXT("Real hit against depleted shield carries a receipt"), Receipt.HasNativeReceipt());
	TestTrue(TEXT("First accepted hit starts full shield recharge delay"), Shield->GetSecondsUntilRecharge() > 2.9f);
	Fixture.Advance(1.f);
	const float RemainingBeforeReplay = Shield->GetSecondsUntilRecharge();
	ASC->OnDamageResolvedAsTarget.Broadcast(Receipt);
	TestEqual(TEXT("Same receipt cannot restart recharge a second time"), Shield->GetSecondsUntilRecharge(), RemainingBeforeReplay);
	ASC->SetCharacterReadyEpoch(ASC->GetCharacterReadyEpoch() + 1);
	Shield->ResetForCheckpoint();
	Fixture.Advance(1.f);
	const float RemainingAfterRebind = Shield->GetSecondsUntilRecharge();
	ASC->OnDamageResolvedAsTarget.Broadcast(Receipt);
	TestEqual(TEXT("Rebinding cannot make an old-life receipt reusable"), Shield->GetSecondsUntilRecharge(), RemainingAfterRebind);
	// A copied native multicast models a broadcaster already traversing its listener list.
	// This consumer did not observe the original hit, so receipt replay protection alone
	// cannot hide a wrong-ASC delivery after its same-avatar rebind.
	auto* UnconsumedShield = Attach<USovShieldComponent>(Owner, ASC);
	const auto RetiredCallbacks = ASC->OnDamageResolvedAsTarget;
	auto* Replacement = ReplacementASC(Owner, 75.f, 40.f);
	TestTrue(TEXT("Original receipt still identifies its real ASC"), Receipt.IsCurrentTargetLife(ASC));
	TestFalse(TEXT("Same avatar does not make the receipt belong to replacement ASC"), Receipt.IsCurrentTargetLife(Replacement));
	UnconsumedShield->InitializeWithAbilitySystem(Replacement);
	UnconsumedShield->ResetForCheckpoint();
	Fixture.Advance(1.f);
	const float ReplacementRemaining = UnconsumedShield->GetSecondsUntilRecharge();
	RetiredCallbacks.Broadcast(Receipt);
	TestEqual(TEXT("An in-flight old-ASC callback cannot restart the new binding's delay"),
		UnconsumedShield->GetSecondsUntilRecharge(), ReplacementRemaining);
	return true;
}
#endif
