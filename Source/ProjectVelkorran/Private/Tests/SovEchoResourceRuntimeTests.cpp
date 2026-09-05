// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovEchoResourceTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovCombatRoutingTestFixtures.h"
#include "Combat/SovEchoAttackReceipt.h"
#include "Campaign/SovEchoBypassGate.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovStatusComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovSeleneEchoGenerationComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include <limits>

#if WITH_AUTOMATION_TESTS
struct FSovEchoResourceTestAccess
{
	static void Gate(ASovTransformingWeaponVisual* Visual, bool bActive) { Visual->SetTransitionGateActive(bActive); }
};
namespace
{
	struct FEchoWorld
	{
		UWorld* World = nullptr;
		FEchoWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
				.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World)
			{
				if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			}
		}
		~FEchoWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); }
		}
		ASovAxiomRuntimeTestCharacter* Character(float X, int32 Team)
		{
			if (!World) return nullptr;
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),
				FVector(X, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
			if (Actor) Actor->InitializeTestCombat(Team);
			return Actor;
		}
	};
	void Hit(ASovAxiomRuntimeTestCharacter* Source, ASovAxiomRuntimeTestCharacter* Target,
		float Body = 1.f, float Poise = 0.f, FName Bone = NAME_None, USovEchoAttackReceipt* Receipt = nullptr,
		UGameplayAbility* Ability = nullptr)
	{
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddInstigator(Source, Source);
		if (Receipt) Context.AddSourceObject(Receipt);
		if (Ability) Context.SetAbility(Ability);
		if (!Bone.IsNone()) { FHitResult H; H.BoneName = Bone; Context.AddHitResult(H); }
		FGameplayEffectSpec Spec(GetDefault<USovCombatRoutingTestEffect>(), Context, 1.f);
		Spec.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Body);
		Spec.SetSetByCallerMagnitude(FSovGameplayTags::Get().SetByCaller_Damage_PoiseDamage, Poise);
		Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic);
		if (Receipt) Spec.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Heavy);
		ASC->ApplyGameplayEffectSpecToTarget(Spec, Target->GetNarrativeAbilitySystemComponent());
	}
	void AddEchoResourceWeakPoints(ASovAxiomRuntimeTestCharacter* Actor)
	{
		auto* C = NewObject<USovWeakPointRoutingTestComponent>(Actor);
		Actor->AddInstanceComponent(C); C->RegisterComponent();
		C->InitializeWithAbilitySystem(Actor->GetNarrativeAbilitySystemComponent());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoSignatureReadinessTest,
	"ProjectVelkorran.Campaign.Echo.SignatureReadiness", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEchoSignatureReadinessTest::RunTest(const FString&)
{
	FEchoWorld F;
	auto* Player = F.Character(0.f, 0);
	if (!TestNotNull(TEXT("Player"), Player)) return false;
	auto* ASC = Player->GetNarrativeAbilitySystemComponent();
	UWeaponItem* Weapon = Player->SetTestWeapon();
	auto* Echo = Player->TestEcho.Get();
	Echo->RestoreEchoFromCheckpoint(90.f);
	TestFalse(TEXT("No granted signature is unavailable even at90"), Echo->IsSignatureReady());
	const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovEchoReadyTestSignature::StaticClass(), 1, INDEX_NONE, Weapon));
	TestEqual(TEXT("Requirement respects child threshold greater than cost"), Echo->GetSignatureEchoRequirement(), 95.f);
	TestFalse(TEXT("90 cannot advertise a95 threshold"), Echo->IsSignatureReady());
	Echo->RestoreEchoFromCheckpoint(95.f);
	TestTrue(TEXT("95 satisfies authored signature"), Echo->IsSignatureReady());
	Player->RemoveTestWeapon();
	TestFalse(TEXT("Stale wield tag on removed source cannot advertise readiness"), Echo->IsSignatureReady());
	ASC->ClearAbility(Handle);
	Weapon = Player->SetTestWeapon();
	Player->RemoveTestWeapon();
	ASC->GiveAbility(FGameplayAbilitySpec(USovEchoSummonedTestSignature::StaticClass(), 1, INDEX_NONE, Player));
	Echo->RestoreEchoFromCheckpoint(75.f);
	TestTrue(TEXT("Resonance remains75"), Echo->IsResonant());
	TestFalse(TEXT("75 does not advertise90 signature"), Echo->IsSignatureReady());
	Echo->RestoreEchoFromCheckpoint(90.f);
	TestTrue(TEXT("Definition-granted Dispatch without weapon advertises90 signature"), Echo->IsSignatureReady());
	const float Before = Echo->GetEcho();
	TestEqual(TEXT("NaN award rejected"), Echo->AddEcho(std::numeric_limits<float>::quiet_NaN(), FGameplayTag()), 0.f);
	TestFalse(TEXT("Infinite spend rejected"), Echo->TrySpendEcho(std::numeric_limits<float>::infinity(), FGameplayTag()));
	TestEqual(TEXT("Invalid values do not poison meter"), Echo->GetEcho(), Before);
	Echo->RestoreEchoFromCheckpoint(0.f);
	auto* Observer = NewObject<USovEchoCallbackTestObserver>(Player);
	Observer->Echo = Echo;
	Observer->bSpendOnNextChange = true;
	Echo->OnEchoChanged.AddUniqueDynamic(Observer, &USovEchoCallbackTestObserver::HandleEchoChanged);
	TestEqual(TEXT("Award reports its own5 write when callback spends2"), Echo->AddEcho(5.f, FGameplayTag()), 5.f);
	TestEqual(TEXT("Nested spend applies exactly once"), Echo->GetEcho(), 3.f);
	ASC->ClearActorInfo();
	TestFalse(TEXT("Readiness tolerates cleared actor info during teardown"), Echo->IsSignatureReady());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoDeflectionActivityTest,
	"ProjectVelkorran.Campaign.Echo.FullMeterDeflection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEchoDeflectionActivityTest::RunTest(const FString&)
{
	FEchoWorld F;
	auto* Player = F.Character(0.f, 0);
	auto* Enemy = F.Character(100.f, 1);
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Enemy"), Enemy)) return false;
	Player->TestEcho->RestoreEchoFromCheckpoint(100.f);
	TestTrue(TEXT("Real deflection window opens"), Player->TestDeflection->BeginDeflection());
	Hit(Enemy, Player, 10.f);
	TestTrue(TEXT("Damage pipeline confirms perfect defense"), Player->LastDamageResult.bPerfectDefense);
	TestEqual(TEXT("Full-meter action refreshes activity source"), Player->TestEcho->GetLastActivityTag(),
		FSovGameplayTags::Get().Echo_Source_PerfectDeflection);
	TestEqual(TEXT("No overflow"), Player->TestEcho->GetEcho(), 100.f);
	Player->TestEcho->TrySpendEcho(10.f, FGameplayTag());
	Player->TestDeflection->OnPerfectDeflection.Broadcast(Player->LastDamageResult);
	TestEqual(TEXT("A consumed full-meter receipt cannot pay after spending"), Player->TestEcho->GetEcho(), 90.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoAttackReceiptRuntimeTest,
	"ProjectVelkorran.Campaign.Echo.AttackReceiptsAndTarrik", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEchoAttackReceiptRuntimeTest::RunTest(const FString&)
{
	FEchoWorld F;
	auto* Player = F.Character(0.f, 0);
	auto* A = F.Character(200.f, 1);
	auto* B = F.Character(400.f, 1);
	auto* C = F.Character(600.f, 1);
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("A"), A)
		|| !TestNotNull(TEXT("B"), B) || !TestNotNull(TEXT("C"), C)) return false;
	auto* ASC = Player->GetNarrativeAbilitySystemComponent();
	const auto& Tags = FSovGameplayTags::Get();
	ASC->RemoveLooseGameplayTag(Tags.Character_Player_Selene);
	ASC->AddLooseGameplayTag(Tags.Character_Player_Tarrik);
	auto* Generator = NewObject<USovTarrikEchoGenerationComponent>(Player);
	Player->AddInstanceComponent(Generator); Generator->RegisterComponent();
	TestTrue(TEXT("Tarrik generator initialized"), Generator->InitializeWithAbilitySystem(ASC));
	Player->TestEcho->RestoreEchoFromCheckpoint(0.f);
	const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovEchoHeavyTestAbility::StaticClass(), 1, INDEX_NONE, Player->SetTestWeapon()));
	if (!TestTrue(TEXT("Real heavy ability activates"), ASC->TryActivateAbility(Handle))) return false;
	auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	auto* Ability = Spec ? Cast<USovEchoHeavyTestAbility>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Instanced heavy"), Ability)) return false;
	auto* Receipt = USovEchoAttackReceipt::CreateForActiveAbility(Ability);
	auto* SecondReceipt = USovEchoAttackReceipt::CreateForActiveAbility(Ability);
	if (!TestNotNull(TEXT("Receipt"), Receipt) || !TestNotNull(TEXT("Second receipt"), SecondReceipt)) return false;
	FGuid Id, OtherId;
	TestTrue(TEXT("Live source accepted"), Receipt->GetSovAttackIdentity(Player, Id));
	TestTrue(TEXT("Second factory accepted"), SecondReceipt->GetSovAttackIdentity(Player, OtherId));
	TestEqual(TEXT("Factories share activation identity"), Id, OtherId);
	TestFalse(TEXT("Wrong source rejected"), Receipt->GetSovAttackIdentity(A, OtherId));
	TestNull(TEXT("CDO cannot mint receipt"), USovEchoAttackReceipt::CreateForActiveAbility(GetMutableDefault<USovEchoHeavyTestAbility>()));
	Hit(Player, A, 1.f, 0.f, NAME_None, Receipt, Ability);
	const FGuid FirstTransaction = A->LastDamageResult.TransactionId;
	Hit(Player, A, 1.f, 0.f, NAME_None, SecondReceipt, Ability);
	Hit(Player, B, 1.f, 0.f, NAME_None, Receipt, Ability);
	TestEqual(TEXT("Repeated first victim is not a third target"), Player->TestEcho->GetEcho(), 0.f);
	USovEchoCallbackTestObserver* EndObserver = NewObject<USovEchoCallbackTestObserver>(C);
	EndObserver->SourceAbility = Ability;
	C->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsTarget.AddUniqueDynamic(EndObserver, &USovEchoCallbackTestObserver::EndSource);
	Hit(Player, C, 1.f, 0.f, NAME_None, Receipt, Ability);
	TestFalse(TEXT("Target callback ended source before source reward broadcast"), Ability->IsActive());
	TestEqual(TEXT("Three distinct heavy victims award8 once"), Player->TestEcho->GetEcho(), 8.f);
	TestTrue(TEXT("Per-target transactions remain distinct"), FirstTransaction != C->LastDamageResult.TransactionId);
	TestEqual(TEXT("Same attack identity on all victims"), C->LastDamageResult.AttackId, Id);
	ASC->OnDamageResolvedAsSource.Broadcast(C->LastDamageResult);
	TestEqual(TEXT("Result replay does not double award"), Player->TestEcho->GetEcho(), 8.f);
	A->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 5.f);
	Hit(Player, A, 0.f, 10.f);
	TestEqual(TEXT("General confirmed poise break awards15"), Player->TestEcho->GetEcho(), 23.f);
	auto* TargetASC = B->GetNarrativeAbilitySystemComponent();
	TargetASC->AddLooseGameplayTag(Tags.State_CommandTarget_Window);
	TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	TargetASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 1.f);
	Hit(Player, B, 10.f);
	TestEqual(TEXT("Fatal hit in pre-hit command window awards8"), Player->TestEcho->GetEcho(), 31.f);
	Ability->Finish();
	TestFalse(TEXT("End invalidates immutable receipt"), Receipt->GetSovAttackIdentity(Player, OtherId));
	TestTrue(TEXT("Second activation starts"), ASC->TryActivateAbility(Handle));
	TestFalse(TEXT("Old receipt cannot adopt new activation"), Receipt->GetSovAttackIdentity(Player, OtherId));
	Ability->Finish();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovEchoSelenePrecisionRuntimeTest,
	"ProjectVelkorran.Campaign.Echo.SelenePrecision", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovEchoSelenePrecisionRuntimeTest::RunTest(const FString&)
{
	FEchoWorld F;
	auto* Player = F.Character(0.f, 0);
	auto* A = F.Character(200.f, 1);
	auto* B = F.Character(400.f, 1);
	auto* C = F.Character(600.f, 1);
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("A"), A)
		|| !TestNotNull(TEXT("B"), B) || !TestNotNull(TEXT("C"), C)) return false;
	AddEchoResourceWeakPoints(A); AddEchoResourceWeakPoints(B); AddEchoResourceWeakPoints(C);
	Player->TestEcho->RestoreEchoFromCheckpoint(0.f);
	Hit(Player, A, 1.f, 0.f, TEXT("weapon"));
	TestEqual(TEXT("First precision target awards8"), Player->TestEcho->GetEcho(), 8.f);
	Hit(Player, A, 1.f, 0.f, TEXT("sensor"));
	TestEqual(TEXT("Second zone on same target grants no chain bonus"), Player->TestEcho->GetEcho(), 16.f);
	Hit(Player, B, 1.f, 0.f, TEXT("weapon"));
	TestEqual(TEXT("Distinct next target adds8+4"), Player->TestEcho->GetEcho(), 28.f);
	Player->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsSource.Broadcast(B->LastDamageResult);
	TestEqual(TEXT("Precision result replay is ignored"), Player->TestEcho->GetEcho(), 28.f);
	Player->TestEcho->BeginEncounter();
	Hit(Player, C, 1.f, 0.f, TEXT("weapon"));
	TestEqual(TEXT("Encounter boundary resets chain"), Player->TestEcho->GetEcho(), 36.f);
	auto* ASC = B->GetNarrativeAbilitySystemComponent();
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Target_Marked);
	ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Target_Exposed);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 1.f);
	Hit(Player, B, 10.f);
	TestEqual(TEXT("Both pre-hit windows still award one6 kill"), Player->TestEcho->GetEcho(), 42.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovUnbrokenWeakPointHitRewardTest,
	"ProjectVelkorran.Campaign.Echo.UnbrokenWeakPointHit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovUnbrokenWeakPointHitRewardTest::RunTest(const FString&)
{
	FEchoWorld F;
	auto* Player = F.Character(100.f, 0);
	auto* Target = F.Character(0.f, 1);
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Target"), Target)) return false;
	auto* Weak = NewObject<USovEchoUnbrokenTestWeakPoint>(Target);
	Target->AddInstanceComponent(Weak); Weak->RegisterComponent();
	Weak->InitializeWithAbilitySystem(Target->GetNarrativeAbilitySystemComponent());
	Player->TestEcho->RestoreEchoFromCheckpoint(0.f);
	Hit(Player, Target, 1.f, 0.f, TEXT("joint"));
	TestFalse(TEXT("Accepted first hit need not break authored threshold"), Weak->IsWeakPointBroken(TEXT("ArmourJoint")));
	TestEqual(TEXT("First unbroken hit grants8"), Player->TestEcho->GetEcho(), 8.f);
	Hit(Player, Target, 1.f, 0.f, TEXT("joint"));
	TestEqual(TEXT("Second actual unbroken hit grants8, no same-target chain"), Player->TestEcho->GetEcho(), 16.f);
	Player->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsSource.Broadcast(Target->LastDamageResult);
	TestEqual(TEXT("Exact accepted-hit replay rejected"), Player->TestEcho->GetEcho(), 16.f);
	Hit(Player, Target, 1.f);
	Hit(Player, Target, 0.f, 1.f, TEXT("joint"));
	TestEqual(TEXT("Body and pure-poise transactions are not weak-point hits"), Player->TestEcho->GetEcho(), 16.f);
	auto* SourceASC = Player->GetNarrativeAbilitySystemComponent();
	FGameplayEffectContextHandle PeriodicContext = SourceASC->MakeEffectContext();
	PeriodicContext.AddInstigator(Player, Player);
	FHitResult PeriodicHit; PeriodicHit.BoneName = TEXT("joint"); PeriodicContext.AddHitResult(PeriodicHit);
	FGameplayEffectSpec Periodic(GetDefault<USovEchoPeriodicTestEffect>(), PeriodicContext, 1.f);
	Periodic.AddDynamicAssetTag(FSovGameplayTags::Get().Damage_Channel_Kinetic);
	Periodic.SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, 1.f);
	const int32 BeforePeriodic = Target->ResolvedHitCount;
	const FActiveGameplayEffectHandle PeriodicHandle = SourceASC->ApplyGameplayEffectSpecToTarget(Periodic, Target->GetNarrativeAbilitySystemComponent());
	F.World->GetTimerManager().Tick(0.01f);
	TestTrue(TEXT("Real periodic effect produces a typed result"), Target->ResolvedHitCount > BeforePeriodic);
	TestTrue(TEXT("Typed result captures periodic delivery"), Target->LastDamageResult.bPeriodicDamage);
	TestEqual(TEXT("Periodic effect's original bone does not create precision awards"), Player->TestEcho->GetEcho(), 16.f);
	Target->GetNarrativeAbilitySystemComponent()->RemoveActiveGameplayEffect(PeriodicHandle);
	Hit(Player, Target, 60.f, 0.f, TEXT("joint"));
	TestTrue(TEXT("Large accepted hit breaks zone"), Weak->IsWeakPointBroken(TEXT("ArmourJoint")));
	TestEqual(TEXT("Break and hit proofs do not double-pay"), Player->TestEcho->GetEcho(), 24.f);
	Hit(Player, Target, 1.f, 0.f, TEXT("joint"));
	TestEqual(TEXT("Already-broken zone gives no reward"), Player->TestEcho->GetEcho(), 24.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovWeaponTransitionOwnerCleanupTest,
	"ProjectVelkorran.Campaign.Echo.TransitionOwnerCleanup", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovWeaponTransitionOwnerCleanupTest::RunTest(const FString&)
{
	FEchoWorld F;
	auto* A = F.Character(0.f, 0);
	auto* B = F.Character(200.f, 0);
	if (!TestNotNull(TEXT("A"), A) || !TestNotNull(TEXT("B"), B)) return false;
	auto* Visual = F.World->SpawnActor<ASovEchoVisualTestActor>();
	if (!TestNotNull(TEXT("Visual"), Visual)) return false;
	const FGameplayTag Tag = FNarrativeGameplayTags::Get().State_Weapon_Equipping;
	auto* AASC = A->GetNarrativeAbilitySystemComponent();
	auto* BASC = B->GetNarrativeAbilitySystemComponent();
	AASC->AddLooseGameplayTag(Tag); // unrelated owner must survive our cleanup
	Visual->SetTestCharacter(A);
	FSovEchoResourceTestAccess::Gate(Visual, true);
	Visual->SetTestCharacter(B);
	FSovEchoResourceTestAccess::Gate(Visual, true);
	TestEqual(TEXT("Old ASC retains only unrelated equipping tag"), AASC->GetTagCount(Tag), 1);
	TestTrue(TEXT("New ASC acquires transition gate"), BASC->HasMatchingGameplayTag(Tag));
	Visual->SetTestCharacter(nullptr);
	FSovEchoResourceTestAccess::Gate(Visual, false);
	TestFalse(TEXT("Missing owner still releases cached gate ASC"), BASC->HasMatchingGameplayTag(Tag));
	TestEqual(TEXT("Other owners remain untouched"), AASC->GetTagCount(Tag), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSeleneExposureMergeTest,
	"ProjectVelkorran.Campaign.Echo.ExposureAndMarkSingleReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovSeleneExposureMergeTest::RunTest(const FString& Parameters)
{
	FEchoWorld F;
	auto* Player = F.Character(0.0f, 0);
	auto* Target = F.Character(200.0f, 1);
	if (!Player || !Target) { AddError(TEXT("Fixture creation failed")); return false; }
	const auto& Tags = FSovGameplayTags::Get();
	auto* ASC = Target->GetNarrativeAbilitySystemComponent();
	auto* Status = NewObject<USovStatusComponent>(Target);
	Target->AddInstanceComponent(Status); Status->RegisterComponent();
	if (!TestTrue(TEXT("Status initializes"), Status->InitializeWithAbilitySystem(ASC))) { return false; }
	FSovStatusApplicationRequest Request;
	Request.RequestId = FGuid::NewGuid(); Request.StatusTag = Tags.Status_Apply_Exposed;
	Request.SourceActor = Player; Request.TargetActor = Target;
	Request.Magnitude = 1.0f; Request.Duration = 4.0f;
	Request.Context = Player->GetNarrativeAbilitySystemComponent()->MakeEffectContext();
	TestTrue(TEXT("Selene applies an exposure window"), Status->ApplyStatus(Request) == ESovStatusApplicationResult::Applied);
	ASC->AddLooseGameplayTag(Tags.State_Target_Marked);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 0.0f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 1.0f);
	Player->TestEcho->RestoreEchoFromCheckpoint(0.0f);
	Hit(Player, Target, 10.0f);
	TestEqual(TEXT("Status exposure plus mark awards only one six-point payoff"), Player->TestEcho->GetEcho(), 6.0f);
	Player->GetNarrativeAbilitySystemComponent()->OnDamageResolvedAsSource.Broadcast(Target->LastDamageResult);
	TestEqual(TEXT("Fatal result replay cannot award exposure again"), Player->TestEcho->GetEcho(), 6.0f);
	return true;
}

#endif
