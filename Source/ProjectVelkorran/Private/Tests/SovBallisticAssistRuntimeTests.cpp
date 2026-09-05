// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Targeting/SovAimAssist.h"
#include "Targeting/SovAimAssistPolicy.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovSettingsTestFixtures.h"
#include "Tests/SovTarrikPayloadTestFixtures.h"
#include "Tests/SovSelenePayloadTestFixtures.h"
#include "Projectiles/SovCinderStickyGrenadeProjectile.h"
#include "Projectiles/SovSeleneCombatProjectile.h"
#include "Components/SovEchoComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
struct FSovBallisticAssistTestAccess
{
	static FVector Velocity(const ASovCinderStickyGrenadeProjectile& Projectile) { return Projectile.ReplicatedInitialVelocity; }
	static float GravityScale(const ASovCinderStickyGrenadeProjectile& Projectile) { return Projectile.ReplicatedGravityScale; }
};
namespace
{
struct FBallisticWorld
{
	UWorld* World = nullptr;
	ASovAxiomRuntimeTestCharacter* Player = nullptr;
	ASovAxiomRuntimeTestCharacter* Enemy = nullptr;
	ANarrativePlayerController* Controller = nullptr;
	TStrongObjectPtr<USovSettingsTestSettings> Settings{NewObject<USovSettingsTestSettings>()};
	TStrongObjectPtr<UObject> PreviousSettings;
	FObjectProperty* SettingsProperty = nullptr;
	FBallisticWorld()
	{
		if (!GEngine) { return; }
		// Swap the real settings owner for a non-persisting test child, then restore it on every exit.
		SettingsProperty = FindFProperty<FObjectProperty>(GEngine->GetClass(), TEXT("GameUserSettings"));
		if (!SettingsProperty) { return; }
		PreviousSettings.Reset(SettingsProperty->GetObjectPropertyValue_InContainer(GEngine));
		SettingsProperty->SetObjectPropertyValue_InContainer(GEngine, Settings.Get());
		const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &IVS);
		if (!World) { return; }
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		Enemy = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(),
			FVector(500.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
		Controller = World->SpawnActor<ANarrativePlayerController>();
		if (!Player || !Enemy || !Controller) { return; }
		Player->InitializeTestCombat(0); Enemy->InitializeTestCombat(1);
		Controller->Possess(Player); Controller->SetViewTarget(Player);
		Player->GetNarrativeAbilitySystemComponent()->SetCharacterReadyEpoch(1);
		Enemy->GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
		Enemy->GetCharacterMovement()->Velocity = FVector(0., 160., 0.);
	}
	~FBallisticWorld()
	{
		if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
		if (SettingsProperty && GEngine) { SettingsProperty->SetObjectPropertyValue_InContainer(GEngine, PreviousSettings.Get()); }
	}
	bool Valid() const { return World && Player && Enemy && Controller && SettingsProperty; }
	bool SetEnabled(bool Enabled)
	{
		auto Snapshot = Settings->GetSettingsSnapshot(); Snapshot.bProjectileLead = Enabled;
		FString Error; return Settings->ApplySettingsSnapshot(Snapshot, Error);
	}
	FVector Point() const { return Enemy->GetActorLocation() + FVector(0., 0., Enemy->GetSimpleCollisionHalfHeight() * .3); }
	SovAimAssist::FProjectileLeadRequest Request() const
	{
		SovAimAssist::FProjectileLeadRequest Value;
		Value.AimDirection = Point().GetSafeNormal(); Value.Range = 1000.f; Value.CollisionRadius = 10.f;
		Value.Gravity = FVector(0., 0., -980.);
		SovAimAssistPolicy::Vector Launch; double Time;
		SovAimAssistPolicy::SolveBallisticIntercept({Point().X, Point().Y, Point().Z}, {}, {0., 0., -980.},
			1600., .6, false, Launch, Time);
		Value.InitialVelocity = FVector(Launch.X, Launch.Y, Launch.Z);
		return Value;
	}
	AActor* Box(FVector Location, FVector Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		auto* Shape = NewObject<UBoxComponent>(Actor); Actor->AddInstanceComponent(Shape); Actor->SetRootComponent(Shape);
		Shape->SetBoxExtent(Extent); Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Shape->SetCollisionObjectType(ECC_WorldStatic); Shape->SetCollisionResponseToAllChannels(ECR_Block);
		Shape->RegisterComponent(); Actor->SetActorLocation(Location); return Actor;
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBallisticAdmissionTest, "ProjectVelkorran.Campaign.Targeting.BallisticAdmissionAndGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovBallisticAdmissionTest::RunTest(const FString& Parameters)
{
	FBallisticWorld F;
	if (!TestTrue(TEXT("Actual physics, controller, settings and combat fixture"), F.Valid())) { return false; }
	if (!F.SetEnabled(false)) { return false; }
	auto Request = F.Request(); FVector Output;
	TestFalse(TEXT("Accessibility off preserves native launch"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	TestTrue(TEXT("Exact original velocity on rejection"), Output == Request.InitialVelocity);
	TestTrue(TEXT("Enable the actual accessibility preference"), F.SetEnabled(true));
	TestTrue(TEXT("Visible moving hostile receives a bounded gravity solution"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	TestTrue(TEXT("Real target velocity produces lateral lead"), Output.Y > 0.);
	TestTrue(TEXT("Fixed launch speed remains 1600"), FMath::IsNearlyEqual(Output.Size(), 1600., .01));
	const FVector Assisted = Output;
	Request.MaximumCorrectionDegrees = .1f;
	TestFalse(TEXT("Per-attack correction bound is authoritative"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	Request = F.Request(); Request.MaximumFlightSeconds = .1f;
	TestFalse(TEXT("Expiry/fuse bounds reject late impacts"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	Request = F.Request(); Request.MaximumFlightSpeed = 1000.f;
	TestFalse(TEXT("Movement speed clamp cannot invalidate the predicted parabola"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	Request = F.Request(); Request.bPreferHighArc = true;
	TestFalse(TEXT("High arc outside horizon does not silently become a low arc"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	Request = F.Request(); F.Enemy->TestTeam = 0;
	TestFalse(TEXT("Friendly target excluded"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	F.Enemy->TestTeam = 1;
	// The swept arc meets this small obstruction while both straight visibility rays clear it.
	const double T = F.Point().X / Assisted.X;
	const FVector Mid = Assisted * (T * .5) + Request.Gravity * (.5 * FMath::Square(T * .5));
	AActor* Cover = F.Box(Mid, FVector(6., 6., 1.));
	AActor* Target = nullptr; FVector Point;
	TestTrue(TEXT("Target remains visible below the arc obstruction"), SovAimAssist::FindVisibleTarget(F.Player, {}, Request.AimDirection, 1000.f, 8.f, Target, Point));
	TestFalse(TEXT("Physical projectile radius and curved path reject overhead cover"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	TestTrue(TEXT("Blocked assist preserves original launch"), Output == Request.InitialVelocity);
	Cover->Destroy();
	F.Box(FVector(250., 0., 0.), FVector(20., 200., 200.));
	TestFalse(TEXT("Opaque direct cover rejects target acquisition"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	F.Controller->UnPossess();
	TestFalse(TEXT("Lost possession cannot steer a new projectile"), SovAimAssist::GetBallisticProjectileLead(F.Player, {}, Request, Output));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovBallisticNativeLaunchTest, "ProjectVelkorran.Campaign.Targeting.NativeGrenadeLeadConsumers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovBallisticNativeLaunchTest::RunTest(const FString& Parameters)
{
	{
		FBallisticWorld F;
		if (!F.Valid() || !F.SetEnabled(true)) { return false; }
		auto* ASC = F.Player->GetNarrativeAbilitySystemComponent();
		ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Selene);
		ASC->AddLooseGameplayTag(FSovGameplayTags::Get().Character_Player_Tarrik);
		F.Player->TestEyeOffset = FVector(45., 20., 65.);
		F.Enemy->SetActorLocation(FVector(500., 20., 65.));
		F.Controller->SetControlRotation((F.Point() - F.Player->TestEyeOffset).Rotation());
		const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovTarrikGrenadeTestAbility::StaticClass(), 1, INDEX_NONE, F.Player->SetTestWeapon()));
		if (!TestTrue(TEXT("Real Cinder GAS action activates"), ASC->TryActivateAbility(Handle))) { return false; }
		auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		auto* Ability = Spec ? Cast<USovTarrikGrenadeTestAbility>(Spec->GetPrimaryInstance()) : nullptr;
		auto* Grenade = Ability ? Ability->ReleaseCinderStickyGrenadeFromAim() : nullptr;
		if (!TestNotNull(TEXT("Actual Cinder release creates its existing projectile"), Grenade)) { return false; }
		const FVector Velocity = FSovBallisticAssistTestAccess::Velocity(*Grenade);
		TestTrue(TEXT("Cinder launch consumes moving-target assistance"), Velocity.Y > 100.);
		TestTrue(TEXT("Cinder retains configured launch speed"), FMath::IsNearlyEqual(Velocity.Size(), 1600., .01));
		TestEqual(TEXT("Cinder retains actual gravity scale"), FSovBallisticAssistTestAccess::GravityScale(*Grenade), 1.f);
		TestEqual(TEXT("Assistance does not duplicate Echo payment"), F.Player->TestEcho->GetEcho(), 65.f);
		TestNull(TEXT("Duplicate release still rejected"), Ability->ReleaseCinderStickyGrenadeFromAim());
	}
	{
		FBallisticWorld F;
		if (!F.Valid() || !F.SetEnabled(true)) { return false; }
		F.World->GetWorldSettings()->bGlobalGravitySet = true;
		F.World->GetWorldSettings()->GlobalGravityZ = -450.f;
		F.Controller->SetControlRotation(F.Point().Rotation());
		auto* ASC = F.Player->GetNarrativeAbilitySystemComponent();
		const auto Handle = ASC->GiveAbility(FGameplayAbilitySpec(USovStillpointPayloadTestAbility::StaticClass(), 1, INDEX_NONE, F.Player->SetTestWeapon()));
		if (!TestTrue(TEXT("Real Stillpoint GAS action activates"), ASC->TryActivateAbility(Handle))) { return false; }
		ASovSeleneCombatProjectile* Grenade = nullptr;
		for (TActorIterator<ASovSeleneCombatProjectile> It(F.World); It; ++It) { Grenade = *It; break; }
		if (!TestNotNull(TEXT("Actual Stillpoint release creates its existing projectile"), Grenade)) { return false; }
		const FVector Start = Grenade->GetActorLocation(), Velocity = Grenade->GetVelocity();
		TestTrue(TEXT("Stillpoint launch consumes moving-target assistance"), Velocity.Y > 100.);
		TestTrue(TEXT("Stillpoint retains fixed launch speed"), FMath::IsNearlyEqual(Velocity.Size(), 1500., .01));
		TestEqual(TEXT("Nondefault world gravity is active"), F.World->GetGravityZ(), -450.f);
		Grenade->Tick(.2f);
		const FVector Expected = Start + Velocity * .2 + FVector(0., 0., .5 * F.World->GetGravityZ() * .2 * .2);
		TestTrue(TEXT("Real substepped flight matches gravity used at release"), Grenade->GetActorLocation().Equals(Expected, .05));
		TestEqual(TEXT("Stillpoint pays once"), F.Player->TestEcho->GetEcho(), 65.f);
	}
	return true;
}
#endif
