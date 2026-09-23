// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCombatInterruptionTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "AIController.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "Presentation/SovCinderJudgementPresentation.h"
#include "Projectiles/SovReformationDroneRocketProjectile.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS
namespace SovCombatInterruptionTests
{
struct FWorld
{
	UWorld* World = nullptr;
	FWorld()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false)
			.ShouldSimulatePhysics(false).SetTransactional(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (World)
		{
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			World->GetTimerManager().Tick(0.f);
		}
	}
	~FWorld()
	{
		if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
	}
	ASovAxiomRuntimeTestCharacter* Character(FVector Location, int32 Team)
	{
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Actor = World ? World->SpawnActor<ASovAxiomRuntimeTestCharacter>(Location, FRotator::ZeroRotator, Spawn) : nullptr;
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
		int32 Count = 0; for (TActorIterator<T> It(World); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Count; } } return Count;
	}
	ASovCinderJudgementPresentation* Judgement() const
	{
		for (TActorIterator<ASovCinderJudgementPresentation> It(World); It; ++It) { return *It; } return nullptr;
	}
};
template<class T> T* Activate(FAutomationTestBase& Test, ASovAxiomRuntimeTestCharacter* Source, bool bWeapon = false)
{
	if (!Source) { Test.AddError(TEXT("Missing source fixture")); return nullptr; }
	auto* ASC = Source->GetNarrativeAbilitySystemComponent();
	FGameplayAbilitySpec Spec(T::StaticClass(), 1, INDEX_NONE, bWeapon ? Source->SetTestWeapon() : nullptr);
	const auto Handle = ASC->GiveAbility(Spec);
	if (!Test.TestTrue(TEXT("Native GAS activation succeeds"), ASC->TryActivateAbility(Handle))) { return nullptr; }
	const auto* Granted = ASC->FindAbilitySpecFromHandle(Handle);
	return Granted ? Cast<T>(Granted->GetPrimaryInstance()) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDroneAttackTargetSurvivesMovementFocusTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Drone.SelectedTargetSurvivesMovementFocus",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDroneAttackTargetSurvivesMovementFocusTest::RunTest(const FString& Parameters)
{
	SovCombatInterruptionTests::FWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 1);
	auto* Target = Fixture.Character(FVector(500.f, 0.f, 0.f), 0);
	auto* Controller = Fixture.World->SpawnActor<AAIController>();
	if (!Source || !Target || !Controller) { return false; }
	Controller->Possess(Source);
	Controller->SetFocus(Target);
	if (!TestEqual(TEXT("Selector focus precedes activation"), Controller->GetFocusActor(), static_cast<AActor*>(Target)))
	{
		return false;
	}
	auto* Gun = SovCombatInterruptionTests::Activate<USovCombatDroneGunTestAbility>(*this, Source);
	if (!Gun) { return false; }
	// BT travel can replace focus during the windup. The chosen attack should
	// still fire toward its live hostile target rather than this movement point.
	Controller->ClearFocus(EAIFocusPriority::Gameplay);
	Controller->SetFocalPoint(FVector(0.f, -500.f, 0.f));
	TestNull(TEXT("Movement has removed actor focus"), Controller->GetFocusActor());
	Gun->FireGunBurstFromAim();
	TestEqual(TEXT("Committed attack still hits the selected player"), Target->ResolvedHitCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionDroneReleaseDisableTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Drone.ReleaseCallbackDisable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionDroneReleaseDisableTest::RunTest(const FString& Parameters)
{
	SovCombatInterruptionTests::FWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 1);
	auto* Ability = SovCombatInterruptionTests::Activate<USovCombatDroneRocketTestAbility>(*this, Source);
	if (!Ability) { return false; }
	Ability->bDisableDuringRelease = true;
	TestNull(TEXT("Transient disable in release callback rejects rocket"), Ability->LaunchRocketFromAim());
	TestEqual(TEXT("No projectile spawned past the canceled release"), Fixture.Count<ASovReformationDroneRocketProjectile>(), 0);
	TestFalse(TEXT("Disable ends the old firing lane even after tag removal"), Ability->IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionDroneReleaseRestartTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Drone.ReleaseCallbackRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionDroneReleaseRestartTest::RunTest(const FString& Parameters)
{
	SovCombatInterruptionTests::FWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 1);
	auto* Ability = SovCombatInterruptionTests::Activate<USovCombatDroneRocketTestAbility>(*this, Source);
	if (!Ability) { return false; }
	Ability->bRestartDuringRelease = true;
	TestNull(TEXT("Old release does not fire from restarted activation"), Ability->LaunchRocketFromAim());
	TestTrue(TEXT("Deliberate restart succeeds"), Ability->bRestartAccepted);
	TestTrue(TEXT("Old cancellation does not end restarted activation"), Ability->IsActive());
	TestEqual(TEXT("Restart remains in its own windup"), Fixture.Count<ASovReformationDroneRocketProjectile>(), 0);
	TestNotNull(TEXT("New activation can explicitly release its own rocket"), Ability->LaunchRocketFromAim());
	TestEqual(TEXT("Exactly one new projectile"), Fixture.Count<ASovReformationDroneRocketProjectile>(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionDroneBurstDisableTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Drone.BurstAndAvatarRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionDroneBurstDisableTest::RunTest(const FString& Parameters)
{
	{
		SovCombatInterruptionTests::FWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 1);
		auto* Target = Fixture.Character(FVector(500.f, 0.f, 0.f), 0);
		auto* Ability = SovCombatInterruptionTests::Activate<USovCombatDroneGunTestAbility>(*this, Source);
		if (!Ability || !Target) { return false; }
		Ability->FireGunBurstFromAim();
		TestEqual(TEXT("First shot is accepted"), Target->ResolvedHitCount, 1);
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		ASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
		ASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Status_DeviceDisabled);
		Fixture.World->GetTimerManager().Tick(0.5f);
		TestEqual(TEXT("No queued burst resumes after transient disable"), Target->ResolvedHitCount, 1);
	}
	{
		SovCombatInterruptionTests::FWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 1);
		auto* Replacement = Fixture.Character(FVector(0.f, 1000.f, 0.f), 1);
		auto* Ability = SovCombatInterruptionTests::Activate<USovCombatDroneRocketTestAbility>(*this, Source);
		if (!Ability || !Replacement) { return false; }
		auto* ASC = Source->GetNarrativeAbilitySystemComponent();
		ASC->InitAbilityActorInfo(Source, Replacement);
		ASC->InitAbilityActorInfo(Source, Source);
		TestNull(TEXT("Switch away/back does not revive old release ownership"), Ability->LaunchRocketFromAim());
		TestEqual(TEXT("Retired windup cannot create a projectile"), Fixture.Count<ASovReformationDroneRocketProjectile>(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionJudgementThinCoverTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Judgement.ThinCoverAndForwardTrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionJudgementThinCoverTest::RunTest(const FString& Parameters)
{
	SovCombatInterruptionTests::FWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(300.f, 0.f, 0.f), 1);
	if (!Source || !Target) { return false; }
	Fixture.Wall(FVector(50.f, 0.f, 0.f), FVector(1.f, 300.f, 300.f));
	auto* Ability = SovCombatInterruptionTests::Activate<USovCombatJudgementTestAbility>(*this, Source, true);
	if (!Ability) { return false; }
	TestTrue(TEXT("Paid shot resolves against nearby cover"), Ability->ReleaseCinderJudgementFromAim());
	auto* Packet = Fixture.Judgement();
	if (!TestNotNull(TEXT("Immutable presentation packet"), Packet)) { return false; }
	TestTrue(TEXT("Obstructed muzzle returns to authoritative eye"), Packet->GetTraceStart().Equals(FVector::ZeroVector, 1.f));
	TestTrue(TEXT("Shot progresses forward to near wall"), Packet->GetTraceEnd().X > 0.f && Packet->GetTraceEnd().X < 51.f);
	TestEqual(TEXT("Thin wall shields target from direct and radial damage"), Target->ResolvedHitCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionJudgementDamageReceiptTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Judgement.ReceiptSurvivesImmediateHealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionJudgementDamageReceiptTest::RunTest(const FString& Parameters)
{
	SovCombatInterruptionTests::FWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 0);
	auto* Target = Fixture.Character(FVector(500.f, 0.f, 0.f), 1);
	if (!Source || !Target) { return false; }
	TStrongObjectPtr<USovCombatInterruptionDamageProbe> Probe(NewObject<USovCombatInterruptionDamageProbe>());
	Probe->SourceASC = Source->GetNarrativeAbilitySystemComponent(); Probe->TargetASC = Target->GetNarrativeAbilitySystemComponent();
	Probe->bHealTarget = true;
	Probe->SourceASC->OnDamageResolvedAsSource.AddDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
	auto* Ability = SovCombatInterruptionTests::Activate<USovCombatJudgementTestAbility>(*this, Source, true);
	if (!Ability) { return false; }
	TestTrue(TEXT("Judgement releases"), Ability->ReleaseCinderJudgementFromAim());
	auto* Packet = Fixture.Judgement();
	if (!TestNotNull(TEXT("Judgement packet"), Packet)) { return false; }
	TestTrue(TEXT("Canonical direct receipt survives callback restoring all changed attributes"), Packet->DirectDamageResolved());
	TestEqual(TEXT("Direct plus radial applications each execute once"), Target->ResolvedHitCount, 2);
	Probe->SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionJudgementSourceRebindTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Judgement.SourceRebindCannotContinueBlast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionJudgementSourceRebindTest::RunTest(const FString& Parameters)
{
	for (const bool bReturnToOriginal : {false, true})
	{
		SovCombatInterruptionTests::FWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		auto* Target = Fixture.Character(FVector(500.f, 0.f, 0.f), 1);
		auto* Nearby = Fixture.Character(FVector(500.f, 180.f, 0.f), 1);
		auto* Replacement = Fixture.Character(FVector(0.f, 1000.f, 0.f), 0);
		if (!Source || !Target || !Nearby || !Replacement) { return false; }
		TStrongObjectPtr<USovCombatInterruptionDamageProbe> Probe(NewObject<USovCombatInterruptionDamageProbe>());
		Probe->SourceASC = Source->GetNarrativeAbilitySystemComponent(); Probe->OriginalAvatar = Source; Probe->ReplacementAvatar = Replacement;
		Probe->bRebindSource = true; Probe->bRestoreOriginalAvatar = bReturnToOriginal;
		Probe->SourceASC->OnDamageResolvedAsSource.AddDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
		auto* Ability = SovCombatInterruptionTests::Activate<USovCombatJudgementTestAbility>(*this, Source, true);
		if (!Ability) { return false; }
		TestTrue(TEXT("Original direct release was committed"), Ability->ReleaseCinderJudgementFromAim());
		TestEqual(TEXT("Original direct hit is not replayed"), Target->ResolvedHitCount, 1);
		TestEqual(TEXT("Remaining blast cannot use replacement or restored actor info"), Nearby->ResolvedHitCount, 0);
		Probe->SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionJudgementTargetRebindTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Judgement.TargetCandidateRetainsOriginalGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionJudgementTargetRebindTest::RunTest(const FString& Parameters)
{
	for (int32 Mutation = 0; Mutation < 3; ++Mutation)
	{
		SovCombatInterruptionTests::FWorld Fixture;
		auto* Source = Fixture.Character(FVector::ZeroVector, 0);
		auto* Direct = Fixture.Character(FVector(500.f, 0.f, 0.f), 1);
		auto* Nearby = Fixture.Character(FVector(500.f, 180.f, 0.f), 1);
		auto* Replacement = Fixture.Character(FVector(0.f, 1000.f, 0.f), 1);
		if (!Source || !Direct || !Nearby || !Replacement) { return false; }
		TStrongObjectPtr<USovCombatInterruptionDamageProbe> Probe(NewObject<USovCombatInterruptionDamageProbe>());
		Probe->SourceASC = Source->GetNarrativeAbilitySystemComponent(); Probe->TargetASC = Nearby->GetNarrativeAbilitySystemComponent();
		Probe->OriginalAvatar = Nearby; Probe->ReplacementAvatar = Replacement;
		Probe->bRebindTarget = Mutation < 2; Probe->bRestoreOriginalAvatar = Mutation == 1; Probe->bReviveTarget = Mutation == 2;
		Probe->SourceASC->OnDamageResolvedAsSource.AddDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
		auto* Ability = SovCombatInterruptionTests::Activate<USovCombatJudgementTestAbility>(*this, Source, true);
		if (!Ability) { return false; }
		TestTrue(TEXT("Judgement commits"), Ability->ReleaseCinderJudgementFromAim());
		TestEqual(TEXT("Unchanged direct candidate receives direct and radial damage"), Direct->ResolvedHitCount, 2);
		TestEqual(TEXT("Retired radial candidate receives no old blast"), Nearby->ResolvedHitCount, 0);
		TestEqual(TEXT("Replacement is not inherited as an old radial candidate"), Replacement->ResolvedHitCount, 0);
		Probe->SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovInterruptionDroneCommittedSuicideTest,
	"ProjectVelkorran.Campaign.CombatInterruption.Drone.CommittedSuicideSurvivesCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovInterruptionDroneCommittedSuicideTest::RunTest(const FString& Parameters)
{
	SovCombatInterruptionTests::FWorld Fixture;
	auto* Source = Fixture.Character(FVector::ZeroVector, 1);
	auto* First = Fixture.Character(FVector(150.f, 0.f, 0.f), 0);
	auto* Second = Fixture.Character(FVector(0.f, 150.f, 0.f), 0);
	if (!Source || !First || !Second) { return false; }
	TStrongObjectPtr<USovCombatInterruptionDamageProbe> Probe(NewObject<USovCombatInterruptionDamageProbe>());
	Probe->SourceASC = Source->GetNarrativeAbilitySystemComponent(); Probe->bCancelSource = true;
	Probe->SourceASC->OnDamageResolvedAsSource.AddDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
	auto* Ability = SovCombatInterruptionTests::Activate<USovCombatDroneSuicideTestAbility>(*this, Source);
	if (!Ability) { return false; }
	Ability->StartSelfDestructRun();
	// UE ticks a timer manager once per engine frame; the fixture already primed this frame.
	{ TGuardValue<uint64> Frame(GFrameCounter, GFrameCounter + 1); Fixture.World->GetTimerManager().Tick(0.1f); }
	TestEqual(TEXT("First outward target receives one committed blast"), First->ResolvedHitCount, 1);
	TestEqual(TEXT("Second outward target still receives committed blast after cancellation"), Second->ResolvedHitCount, 1);
	TestFalse(TEXT("Old ability stays ended"), Ability->IsActive());
	Probe->SourceASC->OnDamageResolvedAsSource.RemoveDynamic(Probe.Get(), &USovCombatInterruptionDamageProbe::ReceiveDamage);
	return true;
}
#endif
