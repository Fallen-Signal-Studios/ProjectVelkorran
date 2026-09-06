// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovThreatAttackTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "AI/NarrativeNPCController.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Effects/SovGameplayEffect_ReformationDroneWeapons.h"
#include "TimerManager.h"
#include "Sovereign/SovGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovThreatAttackTestAccess
{
	static void Start(ASovReformationDroneRocketProjectile& Rocket) { Rocket.StartProjectileMovement(); }
	static void ImpactNearby(ASovReformationDroneRocketProjectile& Rocket)
	{
		FHitResult Hit; Hit.bBlockingHit = true; Hit.ImpactPoint = Rocket.GetActorLocation(); Hit.ImpactNormal = FVector::UpVector;
		Rocket.ResolveImpact(Hit);
	}
};
namespace
{
struct FThreatAttackWorld
{
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = nullptr;
	ASovBotTestCharacter* Source = nullptr;
	ASovBotTestCharacter* Target = nullptr;
	ANarrativeNPCController* Controller = nullptr;
	UAIPerceptionComponent* Perception = nullptr;
	uint64 TimerFrame = GFrameCounter;
	FThreatAttackWorld()
	{
		const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(true).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
			ERHIFeatureLevel::Num, &WorldInitialization);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		World->GetTimerManager().Tick(0.f);
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Source = World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		Target = World->SpawnActor<ASovBotTestCharacter>(ASovBotTestCharacter::StaticClass(), FVector(250., 0., 0.), FRotator::ZeroRotator, Spawn);
		Controller = World->SpawnActor<ANarrativeNPCController>();
		if (!Source || !Target || !Controller) { return; }
		Source->InitializeTestCombat(0); Target->InitializeTestCombat(1);
		Target->GetCapsuleComponent()->SetCollisionResponseToChannel(UArsenalStatics::GetNarrativeProSettings()->WeaponTraceChannel, ECR_Block);
		Controller->Possess(Source); Controller->bShareThreatsWithFaction = false;
		Perception = NewObject<UAIPerceptionComponent>(Controller); Controller->AddInstanceComponent(Perception);
		auto* Sight = NewObject<UAISenseConfig_Sight>(Perception);
		Sight->DetectionByAffiliation.bDetectEnemies = true;
		Perception->ConfigureSense(*Sight); Controller->SetPerceptionComponent(*Perception); Perception->RegisterComponent();
		// Match authored stock perception: registration enables event-driven sight without Activate().
		Controller->RefreshThreatMemory();
	}
	~FThreatAttackWorld() { if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	bool Valid() const { return Source && Target && Controller && Perception && Perception->IsRegistered()
		&& Perception->IsSenseEnabled(UAISense_Sight::StaticClass()); }
	void Sight(bool Seen)
	{
		FAIStimulus Stimulus(*GetDefault<UAISense_Sight>(), 1.f, Target->GetActorLocation(), Source->GetActorLocation());
		if (!Seen) { Stimulus.MarkNoLongerSensed(); }
		Perception->RegisterStimulus(Target, Stimulus); Perception->ProcessStimuli();
	}
	float Health() const { return Target->GetNarrativeAbilitySystemComponent()->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()); }
	void AdvanceTimers(float Seconds)
	{
		TGuardValue<uint64> Frame(GFrameCounter, ++TimerFrame);
		World->GetTimerManager().Tick(Seconds);
	}
	template<class T> T* Activate()
	{
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(T::StaticClass(), 1));
		if (!ASC->TryActivateAbility(Handle)) { return nullptr; }
		const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		return Spec ? Cast<T>(Spec->GetPrimaryInstance()) : nullptr;
	}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatNativeAcquisitionTest, "ProjectVelkorran.Campaign.Threat.NativeAbilityAcquisitionAndWindupLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovThreatNativeAcquisitionTest::RunTest(const FString& Parameters)
{
	FThreatAttackWorld F;
	if (!TestTrue(TEXT("Real NPC, perception and GAS fixture"), F.Valid())) { return false; }
	TestNull(TEXT("Direct Hound activation cannot reacquire an unseen nearest hostile"), F.Activate<USovThreatHoundBite>());
	F.Sight(true); F.Controller->SetFocus(F.Target);
	TestTrue(TEXT("Real sight stimulus authorizes direct targeting"), F.Controller->CanDirectlyTargetThreat(F.Target));
	auto* Bite = F.Activate<USovThreatHoundBite>();
	if (!TestNotNull(TEXT("Direct Hound GAS activation succeeds with current sight"), Bite)) { return false; }
	F.Sight(false); // Do not tick the BT or refresh controller memory before the actual native impact timer.
	F.AdvanceTimers(.5f);
	TestFalse(TEXT("Windup cannot snapshot a target after sight was lost between BT ticks"), Bite->IsActive());
	TestEqual(TEXT("Lost target receives no deliberate bite"), F.Health(), 100.f);
	F.Controller->ClearFocus(EAIFocusPriority::Gameplay);
	auto* Exploder = F.Activate<USovThreatDroneExploder>();
	if (!TestNotNull(TEXT("Native self-destruct action reaches release admission"), Exploder)) { return false; }
	Exploder->StartSelfDestructRun();
	TestFalse(TEXT("Explosive drone's world scan cannot reacquire an unobserved nearest target"), Exploder->IsActive());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatNativeBurstTest, "ProjectVelkorran.Campaign.Threat.NativeBurstLossAndCollateral",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovThreatNativeBurstTest::RunTest(const FString& Parameters)
{
	FThreatAttackWorld F;
	if (!F.Valid()) { return false; }
	F.Sight(true); F.Controller->SetFocus(F.Target);
	auto* Gun = F.Activate<USovThreatDroneGun>();
	if (!TestNotNull(TEXT("Actual drone burst activates with current sight"), Gun)) { return false; }
	Gun->FireGunBurstFromAim();
	const float AfterFirst = F.Health();
	TestTrue(TEXT("First native shot reaches the physical target"), AfterFirst < 100.f);
	F.Target->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
	F.Target->SetActorLocation(FVector(250., 100., 0.));
	F.AdvanceTimers(.3f); // Actor focus intentionally remains stale; no BT tick.
	TestFalse(TEXT("Running burst stops before aiming at concealed live focus"), Gun->IsActive());
	TestEqual(TEXT("Cloak does not receive another tracked shot"), F.Health(), AfterFirst);
	F.Controller->ClearFocus(EAIFocusPriority::Gameplay);
	F.Controller->SetFocalPoint(F.Target->GetActorLocation()); // Explicit fixed-position suppression, not actor tracking.
	auto* Suppression = F.Activate<USovThreatDroneGun>();
	if (!TestNotNull(TEXT("Fixed-position fire remains possible"), Suppression)) { return false; }
	Suppression->FireGunBurstFromAim();
	TestTrue(TEXT("A concealed actor physically inside a shot is still damageable"), F.Health() < AfterFirst);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatRocketTrackingTest, "ProjectVelkorran.Campaign.Threat.ReleasedRocketRetiresHoming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovThreatRocketTrackingTest::RunTest(const FString& Parameters)
{
	FThreatAttackWorld F;
	if (!F.Valid()) { return false; }
	F.Sight(true);
	const FTransform Transform(FRotator::ZeroRotator, FVector(100., 0., 0.));
	auto* Rocket = F.World->SpawnActorDeferred<ASovReformationDroneRocketProjectile>(ASovReformationDroneRocketProjectile::StaticClass(),
		Transform, F.Source, F.Source, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Rocket) { return false; }
	const auto& Tags = FSovGameplayTags::Get();
	Rocket->InitializeRocket(F.Source->GetNarrativeAbilitySystemComponent(), F.Source, F.Source,
		USovGameplayEffect_ReformationDroneDamage::StaticClass(), Tags.Ability_NPC_ReformationDrone_RocketLauncher,
		FGameplayTagContainer(Tags.Damage_Channel_Kinetic), FGameplayTagContainer(Tags.Damage_GuardClass_Heavy), 1.f,
		FVector(1000., 0., 0.), 0.f, 10.f, 3.f, 350.f, 30.f, 10.f, .5f, true, F.Target, 2000.f);
	Rocket->FinishSpawning(Transform); FSovThreatAttackTestAccess::Start(*Rocket);
	auto* Movement = Rocket->FindComponentByClass<UProjectileMovementComponent>();
	if (!TestNotNull(TEXT("Existing projectile movement"), Movement)) { return false; }
	TestTrue(TEXT("Current sight permits actual homing"), Movement->bIsHomingProjectile);
	const FVector Velocity = Movement->Velocity;
	F.Sight(false); Rocket->Tick(.016f);
	TestFalse(TEXT("Released rocket stops tracking before its next movement tick"), Movement->bIsHomingProjectile);
	TestTrue(TEXT("Retirement preserves physical velocity and collision"), Movement->Velocity == Velocity && Movement->UpdatedComponent != nullptr);
	F.Sight(true); Rocket->Tick(.016f);
	TestFalse(TEXT("A spent homing lease cannot reacquire later"), Movement->bIsHomingProjectile);
	F.Target->GetNarrativeAbilitySystemComponent()->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
	FSovThreatAttackTestAccess::ImpactNearby(*Rocket);
	TestTrue(TEXT("Nearby rocket splash still damages a concealed actor"), F.Health() < 100.f);
	return true;
}
#endif
