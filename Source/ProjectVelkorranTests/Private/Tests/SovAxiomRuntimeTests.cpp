// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAxiomRuntimeTestFixtures.h"

#include "Characters/SovDroneNPCBase.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovEchoComponent.h"
#include "Effects/SovGameplayEffect_AxiomNullPulse.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameplayEffect.h"
#include "Misc/AutomationTest.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_AUTOMATION_TESTS

/** Test access adjusts elapsed time, never marks an inactive ability as active. */
struct FSovAxiomNullPulseTestAccess
{
	static void SetFullCharge(USovGameplayAbility_SeleneAxiomNullPulse& Ability)
	{
		Ability.ChargeStartWorldTime = Ability.GetWorld()->GetTimeSeconds() - Ability.FullChargeDuration;
	}
	static bool AddSuppression(USovGameplayAbility_SeleneAxiomNullPulse& Ability,
		UAbilitySystemComponent* Target, float Duration)
	{
		return Ability.ApplyAxiomDurationEffect(Target,
			USovGameplayEffect_AxiomShieldSuppression::StaticClass(),
			FSovGameplayTags::Get().State_Shield_RechargeBlocked, Duration);
	}
	static bool IsDevice(USovGameplayAbility_SeleneAxiomNullPulse& Ability,
		AActor* Target, UAbilitySystemComponent* ASC)
	{
		return Ability.IsAxiomDeviceEligible(Target, ASC);
	}
};

namespace
{
	/** No BeginPlay: intentionally excludes mission/definition/appearance loaders. */
	struct FAxiomRuntimeWorld
	{
		UWorld* World = nullptr;
		FAxiomRuntimeWorld()
		{
			const UWorld::InitializationValues IVS = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false)
				.SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
			if (World)
			{
				if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			}
		}
		~FAxiomRuntimeWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovAxiomRuntimeTestCharacter* Character(FVector Location, int32 Team = 1)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Actor = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
				ASovAxiomRuntimeTestCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn) : nullptr;
			if (Actor) { Actor->InitializeTestCombat(Team); }
			return Actor;
		}
		AActor* Wall(FVector Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			if (!Actor) { return nullptr; }
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Actor->AddInstanceComponent(Box);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(FVector(30.f, 30.f, 160.f));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->RegisterComponent();
			Actor->SetActorLocation(Location);
			return Actor;
		}
	};

	float Value(UAbilitySystemComponent* ASC, FGameplayAttribute Attribute)
	{
		return ASC->GetNumericAttribute(Attribute);
	}

	USovAxiomRuntimeTestAbility* ActivatePulse(FAutomationTestBase& Test,
		ASovAxiomRuntimeTestCharacter* Source)
	{
		if (!Source) { Test.AddError(TEXT("Source fixture failed to spawn")); return nullptr; }
		UNarrativeAbilitySystemComponent* ASC = Source->GetNarrativeAbilitySystemComponent();
		UWeaponItem* Weapon = Source->SetTestWeapon();
		FGameplayAbilitySpec Spec(USovAxiomRuntimeTestAbility::StaticClass(), 1, INDEX_NONE, Weapon);
		// WaitInputRelease must observe a held input during initial activation.
		Spec.InputPressed = true;
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (!Test.TestTrue(TEXT("Real GAS Axiom activation succeeds"), ASC->TryActivateAbility(Handle))) { return nullptr; }
		FGameplayAbilitySpec* Granted = ASC->FindAbilitySpecFromHandle(Handle);
		auto* Ability = Granted ? Cast<USovAxiomRuntimeTestAbility>(Granted->GetPrimaryInstance()) : nullptr;
		if (!Test.TestNotNull(TEXT("GAS created a runtime Axiom instance"), Ability)) { return nullptr; }
		if (!Test.TestTrue(TEXT("Native charge remains active with held input"), Ability->IsActive())) { return nullptr; }
		return Ability;
	}

	USovAxiomRuntimeTestCommandLink* AddPulseLink(FAutomationTestBase& Test,
		ASovAxiomRuntimeTestCharacter* Node, ASovAxiomRuntimeTestCharacter* Source, FName Id)
	{
		const FName ComponentName(*Id.ToString().Replace(TEXT("."), TEXT("_")));
		auto* Link = NewObject<USovAxiomRuntimeTestCommandLink>(Node, ComponentName);
		Node->AddInstanceComponent(Link);
		Link->RegisterComponent();
		Test.TestTrue(TEXT("Authored link identity is accepted before activation"), Link->ConfigureLinkId(Id));
		Link->OnCommandLinkSevered.AddDynamic(Source, &ASovAxiomRuntimeTestCharacter::RecordSever);
		return Link;
	}

	bool ReleasePulseInput(FAutomationTestBase& Test, ASovAxiomRuntimeTestCharacter* Source,
		USovAxiomRuntimeTestAbility* Pulse)
	{
		// Drive the same GAS event as Narrative's input processing. The real
		// WaitInputRelease task invokes the ability's normal release callback.
		FGameplayAbilitySpec* Spec = Pulse ? Pulse->GetCurrentAbilitySpec() : nullptr;
		if (!Test.TestNotNull(TEXT("Held native ability has an active input spec"), Spec)) { return false; }
		const FGameplayAbilitySpecHandle Handle = Pulse->GetCurrentAbilitySpecHandle();
		const FPredictionKey PredictionKey = Pulse->GetCurrentActivationInfoRef().GetActivationPredictionKey();
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		Spec->InputPressed = false;
		ASC->AbilitySpecInputReleased(*Spec);
		return Test.TestTrue(TEXT("Real input-release task receives the ordinary GAS event"),
			ASC->InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Handle, PredictionKey));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeReleaseTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.ReleaseAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeReleaseTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!TestNotNull(TEXT("Transient physics world"), Fixture.World)) { return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	if (!TestNotNull(TEXT("Target fixture"), Target)) { return false; }
	UNarrativeAbilitySystemComponent* ASC = Target->GetNarrativeAbilitySystemComponent();
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	Target->SetActorRotation(FRotator(0.f, 180.f, 0.f));
	ASC->AddLooseGameplayTag(Tags.State_Guarding);
	ASC->AddLooseGameplayTag(Tags.State_PerfectGuard);
	ASC->AddLooseGameplayTag(Tags.State_Deflecting);
	UBoxComponent* ExtraBody = NewObject<UBoxComponent>(Target);
	Target->AddInstanceComponent(ExtraBody);
	ExtraBody->SetupAttachment(Target->GetRootComponent());
	ExtraBody->SetBoxExtent(FVector(45.f));
	ExtraBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ExtraBody->SetCollisionObjectType(ECC_Pawn);
	ExtraBody->SetCollisionResponseToAllChannels(ECR_Ignore);
	ExtraBody->RegisterComponent();
	USovAxiomRuntimeTestAbility* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	TestEqual(TEXT("Activation spends exactly 30 Echo"), Source->TestEcho->GetEcho(), 70.f);
	Target->ReentrantPulse = Pulse;
	TestTrue(TEXT("First authority release succeeds"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestFalse(TEXT("Damage callback cannot release the same pulse recursively"), Target->bReentrantReleaseAccepted);
	TestFalse(TEXT("Second release is rejected"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("Multiple overlap bodies still produce one routed damage transaction"), Target->ResolvedHitCount, 1);
	TestEqual(TEXT("Shield collapses"), Value(ASC, UNarrativeAttributeSetBase::GetShieldAttribute()), 0.f);
	TestEqual(TEXT("Health is untouched"), Value(ASC, UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	TestEqual(TEXT("Poise is untouched"), Value(ASC, UNarrativeAttributeSetBase::GetPoiseAttribute()), 100.f);
	TestEqual(TEXT("Guard/deflection spend no Stamina on EMP"), Value(ASC, UNarrativeAttributeSetBase::GetStaminaAttribute()), 100.f);
	TestFalse(TEXT("EMP is not a perfect-defense transaction"), Target->LastDamageResult.bPerfectDefense);
	TestTrue(TEXT("Recharge suppression is granted"), ASC->HasMatchingGameplayTag(Tags.State_Shield_RechargeBlocked));
	TestFalse(TEXT("Biological target is not hard-disabled"), ASC->HasMatchingGameplayTag(Tags.State_Status_DeviceDisabled));
	TestEqual(TEXT("Repeated release spends no additional Echo"), Source->TestEcho->GetEcho(), 70.f);
	Pulse->FinishEchoAbility();
	TestFalse(TEXT("End clears Echo active tag"), Source->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_EchoAbility_Active));
	TestFalse(TEXT("Ended activation cannot release late"), Pulse->ReleaseAxiomNullPulseFromAim());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeTargetingTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.TargetingAndImmunity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeTargetingTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Hostile = Fixture.Character(FVector(1800.f, 0.f, 0.f));
	auto* Friendly = Fixture.Character(FVector(1800.f, -450.f, 0.f), 0);
	auto* Immune = Fixture.Character(FVector(1800.f, 450.f, 0.f));
	auto* Occluded = Fixture.Character(FVector(1800.f, 200.f, 0.f));
	auto* OutsideCone = Fixture.Character(FVector(1800.f, 1100.f, 0.f));
	auto* OutsideRange = Fixture.Character(FVector(5000.f, 0.f, 0.f));
	auto* Dead = Fixture.Character(FVector(1800.f, -250.f, 0.f));
	auto* Neutral = Fixture.Character(FVector(1800.f, -650.f, 0.f));
	if (!Source || !Hostile || !Friendly || !Immune || !Occluded || !OutsideCone || !OutsideRange || !Dead || !Neutral)
	{ AddError(TEXT("Target fixture failed to spawn")); return false; }
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	Neutral->bTestNeutral = true;
	Immune->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(Tags.Damage_Immunity_Disruption);
	Dead->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_IsDead);
	if (!TestNotNull(TEXT("Real visibility blocker"), Fixture.Wall(FVector(900.f, 100.f, 0.f)))) { return false; }
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	FSovAxiomNullPulseTestAccess::SetFullCharge(*Pulse);
	TestTrue(TEXT("Full charge release succeeds"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("Visible hostile hit"), Hostile->ResolvedHitCount, 1);
	const ASovAxiomRuntimeTestCharacter* Rejected[] = {Friendly, Immune, Occluded, OutsideCone, OutsideRange, Dead, Neutral};
	for (const auto* Actor : Rejected)
	{
		TestEqual(FString::Printf(TEXT("Rejected target %s gets no hit"), *Actor->GetName()), Actor->ResolvedHitCount, 0);
		TestEqual(TEXT("Rejected target retains Shield"), Value(Actor->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
		TestFalse(TEXT("Rejected target receives no suppression"), Actor->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_Shield_RechargeBlocked));
	}
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeDeviceAndDurationTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.DeviceEligibilityAndEffectOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeDeviceAndDurationTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Biological = Fixture.Character(FVector(1000.f, 200.f, 0.f));
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Drone = Fixture.World->SpawnActor<ASovDroneNPCBase>(ASovDroneNPCBase::StaticClass(),
		FVector(1000.f, -200.f, 0.f), FRotator::ZeroRotator, Spawn);
	if (!Source || !Biological || !Drone) { AddError(TEXT("Device fixtures failed to spawn")); return false; }
	UNarrativeAbilitySystemComponent* DroneASC = Drone->GetNarrativeAbilitySystemComponent();
	if (!TestNotNull(TEXT("Real drone owns ASC"), DroneASC)) { return false; }
	if (!TestNotNull(TEXT("Real drone owns native attributes"), Drone->GetAttributeSetBase())) { return false; }
	DroneASC->AddAttributeSetSubobject(Drone->GetAttributeSetBase());
	DroneASC->InitAbilityActorInfo(Drone, Drone);
	if (!TestNotNull(TEXT("Drone attributes registered without BeginPlay"), DroneASC->GetSet<UNarrativeAttributeSetBase>())) { return false; }
	DroneASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	DroneASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	TestTrue(TEXT("Actual drone is an eligible device"), FSovAxiomNullPulseTestAccess::IsDevice(*Pulse, Drone, DroneASC));
	TestFalse(TEXT("Generic biological target is not a device"), FSovAxiomNullPulseTestAccess::IsDevice(*Pulse, Biological, Biological->GetNarrativeAbilitySystemComponent()));
	DroneASC->AddLooseGameplayTag(Tags.Status_Immunity_Freeze);
	TestTrue(TEXT("Freeze immunity does not imply DeviceDisable immunity"), FSovAxiomNullPulseTestAccess::IsDevice(*Pulse, Drone, DroneASC));
	DroneASC->AddLooseGameplayTag(Tags.Status_Immunity_DeviceDisable);
	TestFalse(TEXT("Explicit DeviceDisable immunity is respected"), FSovAxiomNullPulseTestAccess::IsDevice(*Pulse, Drone, DroneASC));
	DroneASC->RemoveLooseGameplayTag(Tags.Status_Immunity_DeviceDisable);
	FSovAxiomNullPulseTestAccess::SetFullCharge(*Pulse);
	TestTrue(TEXT("Release with real drone target"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestTrue(TEXT("Freeze-immune drone receives DeviceDisabled"), DroneASC->HasMatchingGameplayTag(Tags.State_Status_DeviceDisabled));
	TestFalse(TEXT("Biological target remains independently active"), Biological->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(Tags.State_Status_DeviceDisabled));

	// Verify independent effect ownership by removing the short effect explicitly.
	// Timer progression/expiry remains a separate PIE validation requirement.
	UNarrativeAbilitySystemComponent* TargetASC = Biological->GetNarrativeAbilitySystemComponent();
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tags.State_Shield_RechargeBlocked));
	for (const FActiveGameplayEffectHandle Handle : TargetASC->GetActiveEffects(Query)) { TargetASC->RemoveActiveGameplayEffect(Handle); }
	TestTrue(TEXT("Long suppression applies through real GAS"), FSovAxiomNullPulseTestAccess::AddSuppression(*Pulse, TargetASC, 4.f));
	const TArray<FActiveGameplayEffectHandle> LongEffects = TargetASC->GetActiveEffects(Query);
	TestTrue(TEXT("Short suppression applies independently"), FSovAxiomNullPulseTestAccess::AddSuppression(*Pulse, TargetASC, 1.f));
	const TArray<FActiveGameplayEffectHandle> BothEffects = TargetASC->GetActiveEffects(Query);
	TestEqual(TEXT("Independent long and short effect instances"), BothEffects.Num(), 2);
	for (const FActiveGameplayEffectHandle Handle : BothEffects)
	{
		if (!LongEffects.Contains(Handle)) { TargetASC->RemoveActiveGameplayEffect(Handle); }
	}
	TestTrue(TEXT("Ending short suppression preserves longer tag grant"), TargetASC->HasMatchingGameplayTag(Tags.State_Shield_RechargeBlocked));
	for (const FActiveGameplayEffectHandle Handle : LongEffects) { TargetASC->RemoveActiveGameplayEffect(Handle); }
	TestFalse(TEXT("Ending final owned effect clears suppression"), TargetASC->HasMatchingGameplayTag(Tags.State_Shield_RechargeBlocked));
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeCancelAndLinkTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.CancellationWeaponAndLinkReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeCancelAndLinkTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Node = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	if (!Source || !Node) { AddError(TEXT("Command fixtures failed to spawn")); return false; }
	auto* Link = NewObject<USovAxiomRuntimeTestCommandLink>(Node);
	Node->AddInstanceComponent(Link);
	Link->RegisterComponent();
	TestTrue(TEXT("Existing link activates with authored identity"), Link->ActivateCommandLink(Node));
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	FSovCommandLinkSeverResult Result;
	TestTrue(TEXT("Direct link helper cannot bypass native selection"),
		Pulse->TrySeverAxiomCommandLink(Node, Result) == ESovCommandLinkSeverResolution::Invalid);
	TestTrue(TEXT("Native pulse performs legitimate Sever"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestTrue(TEXT("Link is severed"), Link->GetCommandLinkState() == ESovCommandLinkState::Severed);
	TestEqual(TEXT("One validated Sever refunds exactly 12 Echo"), Source->TestEcho->GetEcho(), 82.f);
	Pulse->FinishEchoAbility();
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	TestTrue(TEXT("Second cast can target already-severed node"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("Second cast does not repeat Sever reward"), Source->TestEcho->GetEcho(), 52.f);
	TestEqual(TEXT("Already unshielded target receives no manufactured damage packet"), Node->ResolvedHitCount, 1);
	TestEqual(TEXT("Unshielded target keeps all Health"), Value(Node->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	Pulse->FinishEchoAbility();

	Source->TestEcho->RestoreEchoFromCheckpoint(100.f);
	Node->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	Source->RemoveTestWeapon();
	TestFalse(TEXT("Removing actual wielded source cancels release"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestFalse(TEXT("Invalid weapon activation is ended"), Pulse->IsActive());
	TestEqual(TEXT("Cancelled release delivers no Shield payload"), Value(Node->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
	TestEqual(TEXT("Cancellation does not double-spend"), Source->TestEcho->GetEcho(), 70.f);
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	Source->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_IsDead);
	TestFalse(TEXT("Death cancels active charge synchronously"), Pulse->IsActive());
	TestFalse(TEXT("Late release after death is rejected"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("Death-cancelled pulse delivers no damage"), Value(Node->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeReentrantCancelTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.ReentrantCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeReentrantCancelTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	if (!Source || !Target) { AddError(TEXT("Reentrant fixtures failed to spawn")); return false; }
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	Target->ReentrantPulse = Pulse;
	Target->bCancelPulseOnDamage = true;
	TestTrue(TEXT("Committed Shield transaction can finish after callback cancellation"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestFalse(TEXT("Synchronous damage callback ended source activation"), Pulse->IsActive());
	TestEqual(TEXT("Already committed Shield loss remains"), Value(Target->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetShieldAttribute()), 0.f);
	TestFalse(TEXT("Cancelled activation applies no later suppression"), Target->GetNarrativeAbilitySystemComponent()->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Shield_RechargeBlocked));
	TestFalse(TEXT("Cancelled activation cannot release a second time"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("One committed transaction despite cancellation"), Target->ResolvedHitCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeCloseAimTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.CloseHorizontalAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeCloseAimTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(100.f, 0.f, 0.f));
	if (!Source || !Target) { AddError(TEXT("Close aim fixtures failed to spawn")); return false; }
	Source->TestEyeOffset = FVector(0.f, 0.f, 64.f);
	Target->TestEyeOffset = FVector(0.f, 0.f, 64.f);
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	TestTrue(TEXT("Horizontal early-release pulse accepts close target at eye height"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("Close visible target receives Shield payload"), Target->ResolvedHitCount, 1);
	Pulse->FinishEchoAbility();
	Target->GetNarrativeAbilitySystemComponent()->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	if (!TestNotNull(TEXT("Wall blocks both center and eye samples"), Fixture.Wall(FVector(50.f, 0.f, 0.f)))) { return false; }
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	TestTrue(TEXT("Occluded cast can release without hitting"), Pulse->ReleaseAxiomNullPulseFromAim());
	TestEqual(TEXT("Blocked eye sample cannot bypass wall"), Target->ResolvedHitCount, 1);
	TestEqual(TEXT("Occluded close target keeps Shield"), Value(Target->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetShieldAttribute()), 100.f);
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeActivationGateTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.ActivationGates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeActivationGateTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	if (!Source) { AddError(TEXT("Source fixture failed to spawn")); return false; }
	UNarrativeAbilitySystemComponent* ASC = Source->GetNarrativeAbilitySystemComponent();
	UWeaponItem* Weapon = Source->SetTestWeapon();
	FGameplayAbilitySpec Spec(USovAxiomRuntimeTestAbility::StaticClass(), 1, INDEX_NONE, Weapon);
	Spec.InputPressed = true;
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	ASC->RemoveLooseGameplayTag(Tags.Character_Player_Selene);
	ASC->AddLooseGameplayTag(Tags.Character_Player_Tarrik);
	TestFalse(TEXT("Tarrik cannot activate Selene's pulse"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("Rejected protagonist pays nothing"), Source->TestEcho->GetEcho(), 100.f);
	ASC->RemoveLooseGameplayTag(Tags.Character_Player_Tarrik);
	ASC->AddLooseGameplayTag(Tags.Character_Player_Selene);
	Source->TestEcho->RestoreEchoFromCheckpoint(20.f);
	TestFalse(TEXT("Insufficient Echo fails before activation"), ASC->TryActivateAbility(Handle));
	TestEqual(TEXT("Failed payment leaves Echo intact"), Source->TestEcho->GetEcho(), 20.f);
	TestFalse(TEXT("Rejected activation grants no busy state"), ASC->HasMatchingGameplayTag(Tags.State_EchoAbility_Active));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeMultipleLinkTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.SuccessiveInputsSeverIndependentLinks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeMultipleLinkTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Node = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	if (!Source || !Node) { AddError(TEXT("Command fixtures failed to spawn")); return false; }
	// Deliberately create B first: selection uses authored identity, not component order.
	auto* LinkB = AddPulseLink(*this, Node, Source, TEXT("Aurelion.Weaver.AnchorB"));
	auto* LinkA = AddPulseLink(*this, Node, Source, TEXT("Aurelion.Weaver.AnchorA"));
	if (!TestTrue(TEXT("A activates through native link owner"), LinkA->ActivateCommandLink(Node))
		|| !TestTrue(TEXT("B activates through native link owner"), LinkB->ActivateCommandLink(Node))) { return false; }
	const FGuid InstanceA = LinkA->GetLinkInstanceId(), InstanceB = LinkB->GetLinkInstanceId();
	TestTrue(TEXT("Independent active links have distinct instance identities"), InstanceA.IsValid() && InstanceB.IsValid() && InstanceA != InstanceB);
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse || !ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestEqual(TEXT("First input severs authored A"), LinkA->GetCommandLinkState(), ESovCommandLinkState::Severed);
	TestTrue(TEXT("B remains active after the first input"), LinkB->IsCommandLinkActive());
	TestEqual(TEXT("First input emits one actual native receipt"), Source->RecordedSevers.Num(), 1);
	TestEqual(TEXT("First native sever returns exactly 12 Echo after the 30 cost"), Source->TestEcho->GetEcho(), 82.f);
	Pulse->FinishEchoAbility();
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse || !ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestEqual(TEXT("Second ordinary input reaches B on the same actor"), LinkB->GetCommandLinkState(), ESovCommandLinkState::Severed);
	TestEqual(TEXT("Two ordinary inputs emit two actual receipts"), Source->RecordedSevers.Num(), 2);
	TestEqual(TEXT("Each independent native sever earns its own 12 Echo"), Source->TestEcho->GetEcho(), 64.f);
	if (Source->RecordedSevers.Num() == 2)
	{
		const auto& A = Source->RecordedSevers[0];
		const auto& B = Source->RecordedSevers[1];
		TestTrue(TEXT("Transactions are valid and distinct"), A.TransactionId.IsValid() && B.TransactionId.IsValid() && A.TransactionId != B.TransactionId);
		TestEqual(TEXT("A receipt retains exact activation identity"), A.LinkInstanceId, InstanceA);
		TestEqual(TEXT("B receipt retains exact activation identity"), B.LinkInstanceId, InstanceB);
		TestTrue(TEXT("Both receipts retain actual source, node, severer and reward admission"),
			A.LinkOwner == Node && B.LinkOwner == Node && A.CommandSource == Node && B.CommandSource == Node
			&& A.SeveredBy == Source && B.SeveredBy == Source && A.bEligibleForEchoReward && B.bEligibleForEchoReward);
	}
	const FGuid TransactionA = LinkA->CaptureCommandLinkState().LastSeverTransactionId;
	const FGuid TransactionB = LinkB->CaptureCommandLinkState().LastSeverTransactionId;
	Pulse->FinishEchoAbility();
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse || !ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestEqual(TEXT("Third ordinary input cannot replay either receipt"), Source->RecordedSevers.Num(), 2);
	TestEqual(TEXT("A transaction remains unchanged on replay"), LinkA->CaptureCommandLinkState().LastSeverTransactionId, TransactionA);
	TestEqual(TEXT("B transaction remains unchanged on replay"), LinkB->CaptureCommandLinkState().LastSeverTransactionId, TransactionB);
	TestEqual(TEXT("No reward without a new sever"), Source->TestEcho->GetEcho(), 34.f);
	TestEqual(TEXT("Command pulses keep the actual node alive"), Value(Node->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeBroadphaseTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.BroadphaseResolvesEachActorOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeBroadphaseTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Node = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	if (!Source || !Node) { AddError(TEXT("Command fixtures failed to spawn")); return false; }

	// One target actor commonly overlaps the release sphere through several primitives.
	// Give the node extra query-only geometry so the broadphase returns it repeatedly.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		auto* Extra = NewObject<UBoxComponent>(Node);
		Node->AddInstanceComponent(Extra);
		Extra->SetupAttachment(Node->GetRootComponent());
		Extra->SetBoxExtent(FVector(40.f, 40.f, 40.f));
		Extra->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Extra->SetCollisionObjectType(ECC_Pawn);
		Extra->SetCollisionResponseToAllChannels(ECR_Overlap);
		Extra->RegisterComponent();
		Extra->SetRelativeLocation(FVector(0.f, 0.f, 20.f * Index));
	}
	// Static level geometry is off the firing line. It is not an Axiom target and, since the
	// broadphase no longer queries ECC_WorldStatic, it must not change targeting either way.
	Fixture.Wall(FVector(500.f, 900.f, 0.f));

	auto* Link = AddPulseLink(*this, Node, Source, TEXT("Aurelion.Weaver.AnchorA"));
	if (!TestTrue(TEXT("Link activates through native link owner"), Link->ActivateCommandLink(Node))) { return false; }
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse || !ReleasePulseInput(*this, Source, Pulse)) { return false; }

	TestEqual(TEXT("A multi-primitive target is still acquired and severed"),
		Link->GetCommandLinkState(), ESovCommandLinkState::Severed);
	TestEqual(TEXT("Repeated overlap primitives produce exactly one native receipt"),
		Source->RecordedSevers.Num(), 1);
	TestEqual(TEXT("Repeated overlap primitives award the sever exactly once"),
		Source->TestEcho->GetEcho(), 82.f);
	TestEqual(TEXT("Command pulses keep the actual node alive"),
		Value(Node->GetNarrativeAbilitySystemComponent(), UNarrativeAttributeSetBase::GetHealthAttribute()), 100.f);
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeCandidateAdmissionTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.RejectedAndRetiredLinksCannotBlockEligibleLink",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeCandidateAdmissionTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Node = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	auto* Friendly = Fixture.Character(FVector(0.f, 1000.f, 0.f), 0);
	if (!Source || !Node || !Friendly) { AddError(TEXT("Admission fixtures failed to spawn")); return false; }
	auto* Inactive = AddPulseLink(*this, Node, Source, TEXT("00.Inactive"));
	auto* Retired = AddPulseLink(*this, Node, Source, TEXT("01.Unregistered"));
	auto* Immune = AddPulseLink(*this, Node, Source, TEXT("02.NonSeverable"));
	auto* NonHostile = AddPulseLink(*this, Node, Source, TEXT("03.NonHostile"));
	auto* Accepted = AddPulseLink(*this, Node, Source, TEXT("04.Accepted"));
	auto* Deferred = AddPulseLink(*this, Node, Source, TEXT("05.NextPulse"));
	Immune->SetTestSeverable(false);
	NonHostile->SetTestIncludeOwner(false);
	TestTrue(TEXT("Nonhostile candidate has an actual friendly participant"), NonHostile->RegisterLinkedActor(Friendly));
	for (auto* Link : {Retired, Immune, NonHostile, Accepted, Deferred})
	{
		if (!TestTrue(TEXT("Candidate activates normally"), Link->ActivateCommandLink(Node))) { return false; }
	}
	Retired->UnregisterComponent();
	TestFalse(TEXT("Retired candidate is no longer registered"), Retired->IsRegistered());
	TestTrue(TEXT("Retired candidate still carries a logical instance requiring rejection"), Retired->IsCommandLinkActive());
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse || !ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestEqual(TEXT("Native rejection of earlier candidates does not hide eligible link"), Accepted->GetCommandLinkState(), ESovCommandLinkState::Severed);
	TestTrue(TEXT("Next eligible link is reserved for another pulse"), Deferred->IsCommandLinkActive());
	TestEqual(TEXT("Only actual accepted link emits a receipt"), Source->RecordedSevers.Num(), 1);
	TestEqual(TEXT("Rejected candidates create no additional reward"), Source->TestEcho->GetEcho(), 82.f);
	TestEqual(TEXT("Inactive candidate remains inactive"), Inactive->GetCommandLinkState(), ESovCommandLinkState::Inactive);
	for (auto* Link : {Inactive, Retired, Immune, NonHostile, Deferred})
	{
		TestFalse(TEXT("Unselected or rejected candidate has no sever transaction"), Link->CaptureCommandLinkState().LastSeverTransactionId.IsValid());
	}
	TestTrue(TEXT("Nonseverable candidate retains native active state"), Immune->IsCommandLinkActive());
	TestTrue(TEXT("Nonhostile candidate retains native active state"), NonHostile->IsCommandLinkActive());
	if (Source->RecordedSevers.Num() == 1)
	{
		TestEqual(TEXT("Receipt names only the accepted native link"), Source->RecordedSevers[0].LinkId, Accepted->GetLinkId());
	}
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeOneLinkPerNodeTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.PreservesOneSeverPerHitCommandNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeOneLinkPerNodeTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Left = Fixture.Character(FVector(1000.f, -100.f, 0.f));
	auto* Right = Fixture.Character(FVector(1000.f, 100.f, 0.f));
	if (!Source || !Left || !Right) { AddError(TEXT("Multiple-node fixtures failed to spawn")); return false; }
	auto* LinkA = AddPulseLink(*this, Left, Source, TEXT("NodeA"));
	auto* LinkB = AddPulseLink(*this, Right, Source, TEXT("NodeB"));
	auto* NextA = AddPulseLink(*this, Left, Source, TEXT("NodeA2"));
	auto* NextB = AddPulseLink(*this, Right, Source, TEXT("NodeB2"));
	if (!TestTrue(TEXT("Left link activates"), LinkA->ActivateCommandLink(Left))
		|| !TestTrue(TEXT("Right link activates"), LinkB->ActivateCommandLink(Right))
		|| !TestTrue(TEXT("Second left link activates"), NextA->ActivateCommandLink(Left))
		|| !TestTrue(TEXT("Second right link activates"), NextB->ActivateCommandLink(Right))) { return false; }
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse || !ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestEqual(TEXT("Both visible command nodes receive the pulse's normal Shield payload"), Left->ResolvedHitCount + Right->ResolvedHitCount, 2);
	TestTrue(TEXT("Pulse preserves one successful sever on each hit node"),
		LinkA->GetCommandLinkState() == ESovCommandLinkState::Severed && LinkB->GetCommandLinkState() == ESovCommandLinkState::Severed);
	TestTrue(TEXT("Second independent link on each node waits for a later pulse"), NextA->IsCommandLinkActive() && NextB->IsCommandLinkActive());
	TestEqual(TEXT("Whole pulse preserves two native receipts across two hit nodes"), Source->RecordedSevers.Num(), 2);
	TestEqual(TEXT("Each successful node retains its existing sever reward"), Source->TestEcho->GetEcho(), 94.f);
	Pulse->FinishEchoAbility();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovAxiomRuntimeLinkCallbackRetirementTest,
	"ProjectVelkorran.Campaign.AxiomNullPulse.LinkCallbacksRetireRemainingCandidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSovAxiomRuntimeLinkCallbackRetirementTest::RunTest(const FString& Parameters)
{
	FAxiomRuntimeWorld Fixture;
	if (!Fixture.World) { AddError(TEXT("World creation failed")); return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Node = Fixture.Character(FVector(1000.f, 0.f, 0.f));
	if (!Source || !Node) { AddError(TEXT("Callback fixtures failed to spawn")); return false; }
	auto* LinkA = AddPulseLink(*this, Node, Source, TEXT("A"));
	auto* LinkB = AddPulseLink(*this, Node, Source, TEXT("B"));
	if (!LinkA->ActivateCommandLink(Node) || !LinkB->ActivateCommandLink(Node)) { AddError(TEXT("Link activation failed")); return false; }
	auto* Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	// A real routed Shield callback can retire the pulse before link admission.
	Node->ReentrantPulse = Pulse;
	Node->bCancelPulseOnDamage = true;
	if (!ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestFalse(TEXT("Damage callback ends the actual activation"), Pulse->IsActive());
	TestTrue(TEXT("Cancelled input leaves both links active"), LinkA->IsCommandLinkActive() && LinkB->IsCommandLinkActive());
	TestEqual(TEXT("Cancellation before admission emits no receipt"), Source->RecordedSevers.Num(), 0);
	TestEqual(TEXT("Cancellation before a sever earns no reward"), Source->TestEcho->GetEcho(), 70.f);
	Node->ReentrantPulse.Reset();
	Node->bCancelPulseOnDamage = false;
	Pulse = ActivatePulse(*this, Source);
	if (!Pulse) { return false; }
	Source->ReentrantPulse = Pulse;
	Source->bCancelPulseOnSever = true;
	if (!ReleasePulseInput(*this, Source, Pulse)) { return false; }
	TestFalse(TEXT("Actual native receipt listener ends the second activation"), Pulse->IsActive());
	TestEqual(TEXT("Already committed A remains severed"), LinkA->GetCommandLinkState(), ESovCommandLinkState::Severed);
	TestTrue(TEXT("Callback cancellation cannot continue into B"), LinkB->IsCommandLinkActive());
	TestEqual(TEXT("Committed receipt remains singular despite cancellation"), Source->RecordedSevers.Num(), 1);
	TestEqual(TEXT("Original living source retains only the earned sever refund"), Source->TestEcho->GetEcho(), 52.f);
	TestFalse(TEXT("Retired ability cannot release again"), Pulse->ReleaseAxiomNullPulseFromAim());
	return true;
}

#endif // WITH_AUTOMATION_TESTS
