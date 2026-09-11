// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCompanionStagingTestFixtures.h"
#include "Tests/SovNPCActivityRestoreTestFixtures.h"
#include "AI/NPCDefinition.h"
#include "Character/NarrativeCharacterMovement.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"

ASovCompanionStagingTestCharacter::ASovCompanionStagingTestCharacter(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	AutoPossessAI = EAutoPossessAI::Disabled;
	PrimaryActorTick.bCanEverTick = false; // Exclude cosmetic actor work, never the real movement tick.
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
	GetCapsuleComponent()->SetCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
}
void ASovCompanionStagingTestCharacter::BeginPlay()
{
	// The actual companion/NPC/character/component startup executes. Only the
	// definition's appearance callback is delayed by the fixture override.
	Super::BeginPlay();
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
}
void ASovCompanionStagingTestCharacter::SetActorHiddenInGame(bool bNewHidden)
{
	Super::SetActorHiddenInGame(bNewHidden);
	const auto Callback = MoveTemp(DuringVisibilityChange);
	DuringVisibilityChange = nullptr;
	if (Callback) { Callback(bNewHidden); }
}

#if WITH_AUTOMATION_TESTS
namespace SovCompanionStagingTests
{
	struct FWorld
	{
		FEditorScriptExecutionGuard ScriptGuard;
		UWorld* World = nullptr;
		uint64 Frame = GFrameCounter;
		explicit FWorld(bool bStartPlaying = true)
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (!World) { return; }
			if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
			auto* FloorActor = World->SpawnActor<AActor>();
			auto* Floor = NewObject<UBoxComponent>(FloorActor);
			FloorActor->SetRootComponent(Floor); FloorActor->AddInstanceComponent(Floor);
			Floor->SetBoxExtent(FVector(600.f, 600.f, 25.f));
			Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Floor->SetCollisionObjectType(ECC_WorldStatic); Floor->SetCollisionResponseToAllChannels(ECR_Block);
			Floor->RegisterComponent(); FloorActor->SetActorLocation(FVector(0.f, 0.f, -25.f));
			if (bStartPlaying) { Begin(); }
		}
		void Begin()
		{
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			for (TActorIterator<AActor> It(World); It; ++It)
			{ if (!It->HasActorBegunPlay()) { It->DispatchBeginPlay(); } }
		}
		~FWorld()
		{
			if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } }
		}
		ASovCompanionStagingTestCharacter* Deferred(const FVector& Location)
		{
			auto* Result = World->SpawnActorDeferred<ASovCompanionStagingTestCharacter>(ASovCompanionStagingTestCharacter::StaticClass(),
				FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Result) { Result->SetNPCDefinition(NewObject<UNPCDefinition>(Result)); }
			return Result;
		}
		void Advance(int32 Frames)
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				TGuardValue<uint64> CurrentFrame(GFrameCounter, ++Frame);
				World->Tick(LEVELTICK_All, 1.f / 60.f);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionStagedFloorTest,
	"ProjectVelkorran.Campaign.Companion.Staging.DelayedDeferredSpawnKeepsRealFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionStagedFloorTest::RunTest(const FString& Parameters)
{
	using namespace SovCompanionStagingTests;
	FWorld F(false); if (!TestNotNull(TEXT("Physics world"), F.World)) { return false; }
	const FVector Entry(0.f, 0.f, 90.f);
	auto* Proxy = F.Deferred(Entry); if (!TestNotNull(TEXT("Actual deferred companion"), Proxy)) { return false; }
	auto* Movement = Proxy->GetNarrativeCharacterMovement();
	TestTrue(TEXT("Unstaged movement begins enabled"), Movement->IsComponentTickEnabled());
	TestTrue(TEXT("Unstaged registration begins enabled"), Movement->PrimaryComponentTick.bStartWithTickEnabled);
	TestFalse(TEXT("Deferred component is initially inactive before world initialization"), Movement->IsActive());
	Proxy->SetProxyStaged(true);
	Proxy->FinishSpawning(FTransform(Entry));
	F.Begin();
	TestTrue(TEXT("Actual actor and movement BeginPlay completed"), Proxy->HasActorBegunPlay() && Movement->HasBegunPlay());
	TestTrue(TEXT("Actual movement tick registered"), Movement->PrimaryComponentTick.IsTickFunctionRegistered());
	TestFalse(TEXT("Registration did not reactivate staged gravity"), Movement->IsComponentTickEnabled());
	TestTrue(TEXT("Native InitializeComponents activated movement normally"), Movement->IsActive());
	TestFalse(TEXT("Collision is actually disabled during staging"), Proxy->GetActorEnableCollision());

	// A control with the original collision-only behavior must really fall in the same
	// scheduler. Stop it before the native ragdoll threshold to avoid a missing-mesh fixture.
	const FVector ControlEntry(250.f, 0.f, 90.f);
	auto* Control = F.Deferred(ControlEntry); if (!Control) { return false; }
	Control->SetProxyStaged(false); Control->FinishSpawning(FTransform(ControlEntry)); Control->SetActorEnableCollision(false);
	Control->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	F.Advance(24);
	TestTrue(TEXT("Real unheld gravity crosses the floor's capsule center"), Control->GetActorLocation().Z < 40.f);
	Control->Destroy();
	const auto StagedMode = Movement->MovementMode;
	const FVector StagedVelocity = Movement->Velocity;
	Proxy->SetProxyStaged(true); // The repeated visual-ready callback's exact production entry.
	F.Advance(150);
	TestTrue(TEXT("Delayed startup keeps the exact authored entry"), Proxy->GetActorLocation().Equals(Entry, .01));
	TestEqual(TEXT("Staging never changed the movement mode"), Movement->MovementMode.GetValue(), StagedMode.GetValue());
	TestTrue(TEXT("Staging never advanced velocity"), Movement->Velocity.Equals(StagedVelocity, .01));
	TestFalse(TEXT("Delayed startup never enters native ragdoll"), Movement->IsRagdoll());
	TestTrue(TEXT("Companion remains alive"), Proxy->IsAlive());
	Proxy->SetProxyStaged(false);
	TestTrue(TEXT("Collision restored before movement is released"), Proxy->GetActorEnableCollision());
	TestTrue(TEXT("Original movement tick restored"), Movement->IsComponentTickEnabled());
	TestTrue(TEXT("Original registration default restored"), Movement->PrimaryComponentTick.bStartWithTickEnabled);
	TestTrue(TEXT("Original automatic tick registration restored"), Movement->bAutoUpdateTickRegistration);
	TestTrue(TEXT("Released movement retains normal native activation"), Movement->IsActive());
	F.Advance(30);
	TestTrue(TEXT("Released real movement finds the physical floor"), Movement->CurrentFloor.IsWalkableFloor());
	TestTrue(TEXT("Released capsule stays above its floor"), Proxy->GetActorLocation().Z >= 87.9);
	TestFalse(TEXT("Release does not clear or induce ragdoll"), Movement->IsRagdoll());
	for (int32 Index = 0; Index < 60; ++Index)
	{ Proxy->AddMovementInput(FVector::ForwardVector, 1.f, true); F.Advance(1); }
	TestTrue(TEXT("Released native movement accepts ordinary horizontal input"), Proxy->GetActorLocation().X > 25.f);
	TestTrue(TEXT("Moving companion remains supported by the real floor"), Movement->CurrentFloor.IsWalkableFloor());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionStagedOwnershipTest,
	"ProjectVelkorran.Campaign.Companion.Staging.PreservesModeTickAndActivityOwners",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionStagedOwnershipTest::RunTest(const FString& Parameters)
{
	using namespace SovCompanionStagingTests;
	FWorld F; if (!F.World) { return false; }
	auto* Proxy = F.Deferred(FVector(0.f, 0.f, 90.f)); if (!Proxy) { return false; }
	Proxy->FinishSpawning(FTransform(FVector(0.f, 0.f, 90.f)));
	auto* Controller = F.World->SpawnActor<ASovNPCActivityRestoreTestController>(); if (!Controller) { return false; }
	Controller->Possess(Proxy);
	auto* Activities = Controller->GetActivityComponent();
	Activities->Deactivate();
	auto* ExistingActivity = Activities->AddActivity(USovNPCActivityRestoreTestActivity::StaticClass(), false);
	if (!TestNotNull(TEXT("Native activity has real controller ownership"), ExistingActivity)) { return false; }
	auto* Movement = Proxy->GetCharacterMovement();
	Movement->PrimaryComponentTick.bStartWithTickEnabled = false;
	Movement->bAutoUpdateTickRegistration = false;
	Movement->SetComponentTickEnabled(false); Movement->SetMovementMode(MOVE_Flying);
	Movement->Velocity = FVector(17.f, 4.f, 3.f);
	const FVector Velocity = Movement->Velocity;
	Proxy->SetProxyStaged(true); Proxy->SetProxyStaged(true); Proxy->SetProxyStaged(false);
	TestFalse(TEXT("An originally disabled tick stays disabled"), Movement->IsComponentTickEnabled());
	TestFalse(TEXT("An originally disabled start default stays disabled"), Movement->PrimaryComponentTick.bStartWithTickEnabled);
	TestFalse(TEXT("An originally disabled auto tick policy stays disabled"), Movement->bAutoUpdateTickRegistration);
	TestEqual(TEXT("Original flying mode is untouched"), Movement->MovementMode.GetValue(), MOVE_Flying);
	TestTrue(TEXT("Original velocity is untouched"), Movement->Velocity.Equals(Velocity));
	TestFalse(TEXT("Staging does not activate AI activities"), Activities->IsActive());
	TestTrue(TEXT("Same native activity remains registered"), Activities->GetActivity(ExistingActivity->GetClass()) == ExistingActivity);
	TestTrue(TEXT("Same native controller retains possession"), Proxy->GetController() == Controller);

	Proxy->SetProxyStaged(true);
	Movement->SetMovementMode(MOVE_Falling); // A newer independent owner changes mode while staged.
	Movement->SetComponentTickEnabled(true); Movement->PrimaryComponentTick.bStartWithTickEnabled = true;
	Movement->bAutoUpdateTickRegistration = true;
	Proxy->SetProxyStaged(true); Proxy->SetProxyStaged(false);
	TestEqual(TEXT("Newer mode is not overwritten by a saved mode"), Movement->MovementMode.GetValue(), MOVE_Falling);
	TestTrue(TEXT("Explicit newer tick enable survives"), Movement->IsComponentTickEnabled());
	TestTrue(TEXT("Explicit newer registration default survives"), Movement->PrimaryComponentTick.bStartWithTickEnabled);
	TestTrue(TEXT("Explicit newer auto tick policy survives"), Movement->bAutoUpdateTickRegistration);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovCompanionStagedReentryTest,
	"ProjectVelkorran.Campaign.Companion.Staging.ReentrantVisibilityKeepsNewestTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovCompanionStagedReentryTest::RunTest(const FString& Parameters)
{
	using namespace SovCompanionStagingTests;
	FWorld F; if (!F.World) { return false; }
	auto* Proxy = F.Deferred(FVector(0.f, 0.f, 90.f)); if (!Proxy) { return false; }
	Proxy->SetProxyStaged(true); Proxy->FinishSpawning(FTransform(FVector(0.f, 0.f, 90.f)));
	auto* Movement = Proxy->GetCharacterMovement();
	Proxy->DuringVisibilityChange = [Proxy](bool bHidden)
	{ if (!bHidden) { Proxy->SetProxyStaged(true); } };
	Proxy->SetProxyStaged(false);
	TestTrue(TEXT("Newer nested staging keeps the proxy hidden"), Proxy->IsHidden());
	TestFalse(TEXT("Older reveal cannot restore collision"), Proxy->GetActorEnableCollision());
	TestFalse(TEXT("Older reveal cannot restore gravity"), Movement->IsComponentTickEnabled());
	Proxy->SetProxyStaged(false);
	TestTrue(TEXT("Current reveal restores original gravity exactly once"), Movement->IsComponentTickEnabled());
	Movement->SetComponentTickEnabled(false);
	Proxy->SetProxyStaged(false);
	TestFalse(TEXT("Late repeated visual callback cannot replay a retired lease"), Movement->IsComponentTickEnabled());

	Movement->SetComponentTickEnabled(true);
	Proxy->DuringVisibilityChange = [Proxy](bool bHidden)
	{ if (bHidden) { Proxy->SetProxyStaged(false); } };
	Proxy->SetProxyStaged(true);
	TestFalse(TEXT("Newer nested reveal remains visible"), Proxy->IsHidden());
	TestTrue(TEXT("Older staging cannot disable restored collision"), Proxy->GetActorEnableCollision());
	TestTrue(TEXT("Newer reveal keeps original gravity"), Movement->IsComponentTickEnabled());
	return true;
}
#endif
