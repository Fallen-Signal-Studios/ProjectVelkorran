// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovTarrikPayloadTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/SovEchoComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Projectiles/SovCinderRequiemLine.h"
#include "Projectiles/SovCinderStickyGrenadeProjectile.h"
#include "Projectiles/SovVelkorransHungerProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"

#if WITH_AUTOMATION_TESTS
namespace SovTarrikPayloadTests
{
struct FTarrikTestWorld
{
	UWorld* World = nullptr;
	uint64 TimerFrame = GFrameCounter;
	FTarrikTestWorld()
	{
		const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false)
			.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
			.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
			ERHIFeatureLevel::Num, &WorldInitialization);
		if (World)
		{
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->GetTimerManager().Tick(0.f); // activate newly registered timers in this frame
		}
	}
	void AdvanceTimers(float Seconds)
	{
		TGuardValue<uint64> Frame(GFrameCounter, ++TimerFrame);
		World->GetTimerManager().Tick(Seconds);
	}
	~FTarrikTestWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
	ASovAxiomRuntimeTestCharacter* Character(FVector Location, int32 Team = 1)
	{
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>(
			ASovAxiomRuntimeTestCharacter::StaticClass(), Location, FRotator::ZeroRotator, Spawn) : nullptr;
		if (Actor)
		{
			Actor->InitializeTestCombat(Team);
			Actor->GetCapsuleComponent()->SetCollisionResponseToChannel(UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel, ECR_Block);
			if (Team == 0)
			{
				auto* ASC = Actor->GetNarrativeAbilitySystemComponent();
				ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
				ASC->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
			}
		}
		return Actor;
	}
	void Wall(FVector Location, FVector Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		auto* Box = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Box); Actor->SetRootComponent(Box);
		Box->SetBoxExtent(Extent); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Box->SetCollisionObjectType(ECC_WorldStatic); Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->RegisterComponent(); Actor->SetActorLocation(Location);
	}
	template<class T> int32 Count() const
	{
		int32 Count = 0;
		for (TActorIterator<T> It(World); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Count; } }
		return Count;
	}
};
template<class T> T* Activate(FAutomationTestBase& Test, ASovAxiomRuntimeTestCharacter* Source)
{
	if (!Source) { Test.AddError(TEXT("Tarrik fixture missing")); return nullptr; }
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	UWeaponItem* Weapon = Source->SetTestWeapon();
	FGameplayAbilitySpec Spec(T::StaticClass(), 1, INDEX_NONE, Weapon);
	Spec.InputPressed = true;
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	if (!Test.TestTrue(TEXT("Real GAS activation succeeds with native payload and source weapon"), ASC->TryActivateAbility(Handle))) { return nullptr; }
	FGameplayAbilitySpec* Granted = ASC->FindAbilitySpecFromHandle(Handle);
	return Granted ? Cast<T>(Granted->GetPrimaryInstance()) : nullptr;
}
float Shield(ASovAxiomRuntimeTestCharacter* Actor)
{
	return Actor->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTarrikSlamRuntimeTest,
	"ProjectVelkorran.Campaign.Tarrik.CinderSlam.DamageWardAndFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTarrikSlamRuntimeTest::RunTest(const FString& Parameters)
{
	SovTarrikPayloadTests::FTarrikTestWorld Fixture;
	if (!TestNotNull(TEXT("Physics world"), Fixture.World)) { return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(200.f, 0.f, 0.f));
	auto* Friendly = Fixture.Character(FVector(0.f, 200.f, 0.f), 0);
	auto* Hidden = Fixture.Character(FVector(-300.f, 0.f, 0.f));
	auto* Immune = Fixture.Character(FVector(0.f, -200.f, 0.f));
	if (!Source || !Target || !Friendly || !Hidden || !Immune) { AddError(TEXT("Spawn failed")); return false; }
	Fixture.Wall(FVector(-150.f, 0.f, 0.f), FVector(20.f, 80.f, 160.f));
	Immune->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FSovGameplayTags::Get().Damage_Immunity_All);
	auto* Ability = SovTarrikPayloadTests::Activate<USovTarrikSlamTestAbility>(*this, Source);
	if (!TestNotNull(TEXT("Slam instance"), Ability)) { return false; }
	TestEqual(TEXT("One 90 Echo commit"), Source->TestEcho->GetEcho(), 10.f);
	TestTrue(TEXT("Manual release succeeds"), Ability->ReleaseCinderSlam());
	TestFalse(TEXT("Second release rejected"), Ability->ReleaseCinderSlam());
	TestEqual(TEXT("One resolved radial hit"), Target->ResolvedHitCount, 1);
	TestTrue(TEXT("Hostile Shield reduced"), SovTarrikPayloadTests::Shield(Target) < 100.f);
	TestEqual(TEXT("Friendly unchanged"), SovTarrikPayloadTests::Shield(Friendly), 100.f);
	TestEqual(TEXT("Occluded target unchanged"), SovTarrikPayloadTests::Shield(Hidden), 100.f);
	TestEqual(TEXT("Immune target unchanged"), SovTarrikPayloadTests::Shield(Immune), 100.f);
	TestEqual(TEXT("Ward adds resistance"), Source->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetDamageResistanceAttribute()), 50.f);
	TestEqual(TEXT("Ward does not grant Shield"), SovTarrikPayloadTests::Shield(Source), 100.f);
	Ability->FinishEchoAbility();
	TestFalse(TEXT("Late notify rejected"), Ability->ReleaseCinderSlam());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTarrikRequiemRuntimeTest,
	"ProjectVelkorran.Campaign.Tarrik.CinderlineRequiem.PenetrationCoverAndPersistentChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTarrikRequiemRuntimeTest::RunTest(const FString& Parameters)
{
	SovTarrikPayloadTests::FTarrikTestWorld Fixture;
	if (!Fixture.World) { return false; }
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Friendly = Fixture.Character(FVector(150.f, 0.f, 0.f), 0);
	auto* First = Fixture.Character(FVector(500.f, 0.f, 0.f));
	auto* Second = Fixture.Character(FVector(800.f, 0.f, 0.f));
	auto* Side = Fixture.Character(FVector(600.f, 150.f, 0.f));
	auto* Hidden = Fixture.Character(FVector(1200.f, 0.f, 0.f));
	if (!Source || !Friendly || !First || !Second || !Side || !Hidden) { return false; }
	Fixture.Wall(FVector(1100.f, 0.f, 0.f), FVector(25.f, 400.f, 160.f));
	auto* Ability = SovTarrikPayloadTests::Activate<USovTarrikRequiemTestAbility>(*this, Source);
	if (!Ability) { return false; }
	TestTrue(TEXT("Requiem releases"), Ability->ReleaseCinderlineRequiemFromAim());
	TestFalse(TEXT("Requiem duplicate rejected"), Ability->ReleaseCinderlineRequiemFromAim());
	TestEqual(TEXT("First enemy penetrated once"), First->ResolvedHitCount, 1);
	TestEqual(TEXT("Second enemy penetrated once"), Second->ResolvedHitCount, 1);
	TestEqual(TEXT("Friendly does not absorb or receive penetration"), SovTarrikPayloadTests::Shield(Friendly), 100.f);
	TestEqual(TEXT("World cover stops penetration"), SovTarrikPayloadTests::Shield(Hidden), 100.f);
	TestEqual(TEXT("One persistent lane actor"), Fixture.Count<ASovCinderRequiemLine>(), 1);
	Ability->FinishEchoAbility();
	Source->RemoveTestWeapon();
	// Advance the actual world's native timer manager after the ability has ended.
	Fixture.AdvanceTimers(0.5f);
	TestEqual(TEXT("First enemy gets exactly one line packet"), First->ResolvedHitCount, 2);
	TestEqual(TEXT("Second enemy gets exactly one line packet"), Second->ResolvedHitCount, 2);
	TestEqual(TEXT("Off-axis hostile receives line only"), Side->ResolvedHitCount, 1);
	TestEqual(TEXT("World cover also stops chained blasts"), SovTarrikPayloadTests::Shield(Hidden), 100.f);
	TestEqual(TEXT("Only one Echo spend"), Source->TestEcho->GetEcho(), 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovTarrikProjectileLifecycleRuntimeTest,
	"ProjectVelkorran.Campaign.Tarrik.ProjectileRelease.NativeTimerAndWeaponRevalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovTarrikProjectileLifecycleRuntimeTest::RunTest(const FString& Parameters)
{
	{
		SovTarrikPayloadTests::FTarrikTestWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		auto* Hunger = SovTarrikPayloadTests::Activate<USovTarrikHungerTestAbility>(*this, Source);
		if (!Hunger) { return false; }
		Fixture.AdvanceTimers(0.4f);
		TestEqual(TEXT("Empty Blueprint gameplay releases one Hunger projectile"), Fixture.Count<ASovVelkorransHungerProjectile>(), 1);
		TestNull(TEXT("Late notify cannot spawn another projectile"), Hunger->ReleaseVelkorransHungerFromAim());
		Hunger->FinishEchoAbility();
		TestEqual(TEXT("Released projectile survives ability end"), Fixture.Count<ASovVelkorransHungerProjectile>(), 1);
	}
	{
		SovTarrikPayloadTests::FTarrikTestWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		auto* Hunger = SovTarrikPayloadTests::Activate<USovTarrikHungerTestAbility>(*this, Source);
		if (!Hunger) { return false; }
		Source->RemoveTestWeapon();
		Fixture.AdvanceTimers(0.4f);
		TestEqual(TEXT("Weapon removal rejects queued release"), Fixture.Count<ASovVelkorransHungerProjectile>(), 0);
		TestFalse(TEXT("Invalid source cancels recovery lane"), Hunger->IsActive());
	}
	{
		SovTarrikPayloadTests::FTarrikTestWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		auto* Grenade = SovTarrikPayloadTests::Activate<USovTarrikGrenadeTestAbility>(*this, Source);
		if (!Grenade) { return false; }
		Fixture.AdvanceTimers(0.4f);
		TestEqual(TEXT("Universal grenade releases without Blueprint gameplay"), Fixture.Count<ASovCinderStickyGrenadeProjectile>(), 1);
		TestNull(TEXT("Grenade duplicate rejected"), Grenade->ReleaseCinderStickyGrenadeFromAim());
		TestEqual(TEXT("Grenade spends once"), Source->TestEcho->GetEcho(), 65.f);
		Grenade->FinishEchoAbility();
	}
	{
		SovTarrikPayloadTests::FTarrikTestWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		if (!Source) { return false; }
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
		FGameplayAbilitySpec Spec(USovTarrikSlamTestAbility::StaticClass(), 1, INDEX_NONE, Source->SetTestWeapon());
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		TestFalse(TEXT("Untagged legacy character fails before payment"), ASC->TryActivateAbility(Handle));
		TestEqual(TEXT("Rejected identity spends no Echo"), Source->TestEcho->GetEcho(), 100.f);
	}
	{
		SovTarrikPayloadTests::FTarrikTestWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		auto* Hunger = SovTarrikPayloadTests::Activate<USovTarrikHungerTestAbility>(*this, Source);
		if (!Hunger) { return false; }
		Hunger->FinishEchoAbility(true);
		Fixture.AdvanceTimers(0.4f);
		TestEqual(TEXT("Cancellation clears the queued release"), Fixture.Count<ASovVelkorransHungerProjectile>(), 0);
	}
	return true;
}
#endif
