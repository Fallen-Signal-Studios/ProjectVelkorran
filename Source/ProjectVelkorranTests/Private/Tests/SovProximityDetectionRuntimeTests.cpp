// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovMeleeRuntimeTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
#include "AI/SovProximityDetectionComponent.h"
#include "AI/SovProximityDetectionPolicy.h"
#include "Character/PlayerDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Framework/SovPlayerState.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS

/** Drives observation directly, so each test controls exactly how much time passes. */
struct FSovProximityDetectionTestAccess
{
	static void Observe(USovProximityDetectionComponent& Component, float DeltaSeconds)
	{ Component.Observe(DeltaSeconds); }
};

namespace
{
namespace Policy = SovProximityDetectionPolicy;

struct FDetectionWorld
{
	FEditorScriptExecutionGuard ScriptGuard;
	UWorld* World = nullptr;
	ASovMeleeRuntimeTestPlayer* Player = nullptr;
	ASovAxiomRuntimeTestCharacter* Enemy = nullptr;
	ASovHandoffRuntimeTestController* Controller = nullptr;
	USovProximityDetectionComponent* Detection = nullptr;

	FDetectionWorld()
	{
		const auto Init = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
			.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		if (!World) { return; }
		if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Player = World->SpawnActor<ASovMeleeRuntimeTestPlayer>(ASovMeleeRuntimeTestPlayer::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
		Enemy = World->SpawnActor<ASovAxiomRuntimeTestCharacter>(ASovAxiomRuntimeTestCharacter::StaticClass(), FVector(900.f, 0.f, 0.f), FRotator::ZeroRotator, Spawn);
		Controller = World->SpawnActor<ASovHandoffRuntimeTestController>();
		auto* PlayerState = World->SpawnActor<ASovPlayerState>();
		if (!Player || !Enemy || !Controller || !PlayerState) { return; }
		Enemy->InitializeTestCombat(1);
		auto* Definition = NewObject<UPlayerDefinition>(Controller); Controller->KeepAlive.Add(Definition);
		Player->PrepareCampaignInitialization(Definition); Controller->SetTestPlayerState(PlayerState);
		// Deliberately not a local player controller: Narrative builds its gameplay HUD on BeginPlay
		// for one, against a controller that has no attached player here.
		World->AddController(Controller); Controller->Possess(Player);
		if (!Player->StageTestReadiness(PlayerState, true) || !Player->CompleteCampaignDataInitialization(false)) { return; }
		Detection = NewObject<USovProximityDetectionComponent>(Player);
		Player->AddInstanceComponent(Detection); Detection->RegisterComponent();
		Detection->SetComponentTickEnabled(false);
	}
	~FDetectionWorld()
	{ if (World) { World->DestroyWorld(false); if (GEngine) { GEngine->DestroyWorldContext(World); } } }
	bool Valid() const { return World && Player && Enemy && Controller && Detection; }

	/** A solid wall between the player and the enemy. */
	AActor* Wall()
	{
		auto* Blocker = World->SpawnActor<AActor>();
		auto* Shape = NewObject<UBoxComponent>(Blocker);
		Blocker->SetRootComponent(Shape); Blocker->AddInstanceComponent(Shape);
		Shape->SetBoxExtent(FVector(20.f, 600.f, 600.f));
		Shape->SetCollisionProfileName(TEXT("BlockAll"));
		Shape->RegisterComponent();
		Blocker->SetActorLocation(FVector(450.f, 0.f, 0.f));
		return Blocker;
	}
	/** Observes for a stretch of time in ordinary sweep-sized steps. */
	void ObserveFor(float Seconds)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += .1f)
		{ FSovProximityDetectionTestAccess::Observe(*Detection, .1f); }
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDetectionUnseenTest, "ProjectVelkorran.Campaign.Detection.AnUnseenHostileIsNeverReported",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDetectionUnseenTest::RunTest(const FString& Parameters)
{
	FDetectionWorld F;
	if (!TestTrue(TEXT("Ready detection fixture"), F.Valid())) { return false; }
	F.Wall();
	// Well inside the sweep, alive and hostile, but behind cover the whole time.
	F.ObserveFor(Policy::AcquireSeconds * 10.f);
	TestEqual(TEXT("Proximity alone never reveals a hostile"), F.Detection->GetContactCount(), 0);
	TestEqual(TEXT("Nothing is reported as a live sighting either"), F.Detection->GetLiveSightingCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDetectionAcquireTest, "ProjectVelkorran.Campaign.Detection.AGlimpseDoesNotRegisterButASightingDoes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDetectionAcquireTest::RunTest(const FString& Parameters)
{
	FDetectionWorld F;
	if (!TestTrue(TEXT("Ready detection fixture"), F.Valid())) { return false; }
	// A single sweep is shorter than the acquire dwell, so nothing registers yet.
	FSovProximityDetectionTestAccess::Observe(*F.Detection, .1f);
	TestEqual(TEXT("A glimpse does not create a contact"), F.Detection->GetContactCount(), 0);

	F.ObserveFor(Policy::AcquireSeconds * 4.f);
	if (!TestEqual(TEXT("A held sighting registers exactly one contact"), F.Detection->GetContactCount(), 1)) { return false; }
	const FSovProximityContact& Contact = F.Detection->GetContacts()[0];
	TestTrue(TEXT("The contact is the hostile itself"), Contact.Actor.Get() == F.Enemy);
	TestTrue(TEXT("An enemy in sight is a live sighting"), Contact.bLiveSighting);
	TestEqual(TEXT("A live sighting is drawn at full strength"), Contact.Alpha, 1.f);
	TestTrue(TEXT("Range is placed inside the sweep"), Contact.NormalisedRange > 0.f && Contact.NormalisedRange < 1.f);
	// The enemy stands straight ahead of an unrotated player.
	TestTrue(TEXT("Bearing reads dead ahead"), FMath::Abs(Contact.BearingDegrees) < 5.f);
	TestEqual(TEXT("The player never appears on their own display"), F.Detection->GetContactCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovDetectionMemoryTest, "ProjectVelkorran.Campaign.Detection.LostSightFadesToMemoryThenForgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSovDetectionMemoryTest::RunTest(const FString& Parameters)
{
	FDetectionWorld F;
	if (!TestTrue(TEXT("Ready detection fixture"), F.Valid())) { return false; }
	F.ObserveFor(Policy::AcquireSeconds * 4.f);
	if (!TestEqual(TEXT("Contact acquired before losing sight"), F.Detection->GetContactCount(), 1)) { return false; }
	const FVector SeenAt = F.Detection->GetContacts()[0].LastSeenLocation;

	// Sight is broken, and the enemy then moves somewhere the player cannot see.
	F.Wall();
	F.Enemy->SetActorLocation(FVector(900.f, 800.f, 0.f));
	F.ObserveFor(Policy::MemorySeconds * .5f);
	if (!TestEqual(TEXT("A lost hostile is remembered rather than erased"), F.Detection->GetContactCount(), 1)) { return false; }
	const FSovProximityContact& Remembered = F.Detection->GetContacts()[0];
	TestFalse(TEXT("A memory is not a live sighting"), Remembered.bLiveSighting);
	TestTrue(TEXT("A memory fades"), Remembered.Alpha > 0.f && Remembered.Alpha < 1.f);
	// The crucial property: the radar shows where it was seen, not where it actually is.
	TestTrue(TEXT("Memory keeps the last sighted position rather than tracking through the wall"),
		Remembered.LastSeenLocation.Equals(SeenAt, 1.f));

	F.ObserveFor(Policy::MemorySeconds);
	TestEqual(TEXT("Past its memory window the contact is forgotten entirely"), F.Detection->GetContactCount(), 0);
	return true;
}
#endif
