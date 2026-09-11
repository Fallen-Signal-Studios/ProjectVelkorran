// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovNPCVisualLifecycleTestFixtures.h"
#include "AI/NPCDefinition.h"
#include "Engine/Engine.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Misc/AutomationTest.h"
#include "NarrativeSave.h"
#include "NarrativeStableActor.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovNPCVisualLifecycleTests
{
	struct FWorld
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		FWorld()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			if (World && GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
		}
		~FWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine) { GEngine->DestroyWorldContext(World); }
			}
		}
		ASovNPCVisualLifecycleTestCharacter* Spawn(bool bEncounterRestore = false)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* NPC = World->SpawnActor<ASovNPCVisualLifecycleTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (!NPC) { return nullptr; }
			if (bEncounterRestore) { NPC->PrepareForEncounterRestore(FNPCSpawnInfo(), FGuid::NewGuid()); }
			auto* Definition = NewObject<UNPCDefinition>(NPC);
			Definition->NPCClassPath = ASovNPCVisualLifecycleTestCharacter::StaticClass();
			Definition->bAllowMultipleInstances = true;
			Definition->NPCID = FName(TEXT("VisualLifecycleFixture"));
			NPC->SetNPCDefinition(Definition);
			NPC->GetNarrativeAbilitySystemComponent()->InitAbilityActorInfo(NPC, NPC);
			NPC->CharacterVisualInitialized.AddDynamic(NPC, &ASovNPCVisualLifecycleTestCharacter::ObserveVisual);
			return NPC;
		}
		ASovNPCVisualLifecycleTestVisual* Visual(ASovNPCVisualLifecycleTestCharacter* NPC, bool bPublish = true)
		{
			FActorSpawnParameters Params;
			Params.Owner = NPC;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* Result = World->SpawnActor<ASovNPCVisualLifecycleTestVisual>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (Result)
			{
				Result->SetCharacterForTest(NPC);
				Result->AttachToActor(NPC, FAttachmentTransformRules::KeepWorldTransform);
				if (bPublish) { NPC->SetVisualForTest(Result); }
			}
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovSavedTombstoneVisualLifecycleTest,
	"ProjectVelkorran.Campaign.NPCVisualLifecycle.SavedTombstoneRetiresActualMeshCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovSavedTombstoneVisualLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCVisualLifecycleTests;
	FWorld Scope;
	if (!TestNotNull(TEXT("Native world"), Scope.World)) { return false; }
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Stable native NPC"), NPC)) { return false; }
	auto* Visual = Scope.Visual(NPC);
	auto* Saves = Scope.World->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!TestNotNull(TEXT("Actual world save subsystem"), Saves)
		|| !TestNotNull(TEXT("Separate owned visual"), Visual)) { return false; }
	TestTrue(TEXT("Save owner creates its own live in-memory save object"), Saves->UpdateSaveObject(true));
	if (!TestNotNull(TEXT("Actual live save object"), Saves->GetSaveObject())) { return false; }
	FNarrativeActorRecord Tombstone;
	if (!TestTrue(TEXT("Real actor and component record capture"), Saves->CreateActorRecord(NPC, Tombstone))) { return false; }
	TestEqual(TEXT("Stored stable identity is the actual NPC identity"), Tombstone.ActorGUID,
		INarrativeStableActor::Execute_GetActorGUID(NPC));
	// External stored state: this placed actor died before the checkpoint load.
	// The production LoadSingleActor must find and apply this exact tombstone.
	Tombstone.bNetStartup = true;
	Tombstone.bDestroyed = true;
	Saves->GetSaveObject()->RecordMap.Add(Tombstone.ActorGUID, Tombstone);
	TestFalse(TEXT("No early encounter readiness"), NPC->IsEncounterSnapshotReady());
	Visual->CompleteMeshesForTest();
	TestTrue(TEXT("Actual saved tombstone destroyed the NPC"), !IsValid(NPC) || NPC->IsActorBeingDestroyed());
	TestTrue(TEXT("Native destruction also retired its owned visual"), !IsValid(Visual) || Visual->IsActorBeingDestroyed());
	TestEqual(TEXT("A loaded tombstone cannot grant new-character inventory or activities"), NPC->NewCharacterCalls, 0);
	TestEqual(TEXT("No retired character-visual notification"), NPC->VisualNotifications, 0);
	TestFalse(TEXT("No post-destruction map marker"), NPC->HasMarkerForTest());
	TestFalse(TEXT("No post-destruction encounter-ready publication"), NPC->IsEncounterSnapshotReady());
	TestEqual(TEXT("No visual Blueprint completion after its owner was destroyed"), Visual->FinalAppearanceEvents, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovVisualReplacementContinuationTest,
	"ProjectVelkorran.Campaign.NPCVisualLifecycle.ReplacementNotificationRetiresOnlyOldVisual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovVisualReplacementContinuationTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCVisualLifecycleTests;
	for (const bool bEncounterRestore : {false, true})
	{
		FWorld Scope;
		if (!TestNotNull(TEXT("Native world"), Scope.World)) { return false; }
		auto* NPC = Scope.Spawn(bEncounterRestore);
		if (!TestNotNull(TEXT("Actual ordinary or encounter-restore NPC"), NPC)) { return false; }
		auto* Original = Scope.Visual(NPC);
		auto* Replacement = Scope.Visual(NPC, false);
		if (!TestNotNull(TEXT("Original visual"), Original) || !TestNotNull(TEXT("Replacement visual"), Replacement)) { return false; }
		NPC->ReplacementVisual = Replacement;
		Original->CompleteMeshesForTest();
		TestTrue(TEXT("Notification really replaced the current visual"), NPC->GetCharacterVisual() == Replacement);
		TestEqual(TEXT("Original notification occurred exactly once"), NPC->VisualNotifications, 1);
		TestFalse(TEXT("Old continuation did not create a marker"), NPC->HasMarkerForTest());
		TestFalse(TEXT("Old continuation did not ready a replacement body"), NPC->IsEncounterSnapshotReady());
		TestEqual(TEXT("Old producer did not run its final Blueprint callback"), Original->FinalAppearanceEvents, 0);
		Original->CompleteMeshesForTest();
		TestEqual(TEXT("A late callback from the old visual cannot notify for the new one"), NPC->VisualNotifications, 1);
		TestTrue(TEXT("Late old callback preserves exact current visual"), NPC->GetCharacterVisual() == Replacement);
		Replacement->CompleteMeshesForTest();
		TestTrue(TEXT("Current visual completes ordinary native readiness"), NPC->IsEncounterSnapshotReady());
		TestTrue(TEXT("Current completion registers its real map marker"), NPC->HasMarkerForTest());
		TestEqual(TEXT("Current visual emits its own final Blueprint callback once"), Replacement->FinalAppearanceEvents, 1);
		TestEqual(TEXT("Exactly the two legitimate visual notifications"), NPC->VisualNotifications, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovVisualCallbackDestructionTest,
	"ProjectVelkorran.Campaign.NPCVisualLifecycle.NativeInitializationAndNotificationCanRetireOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovVisualCallbackDestructionTest::RunTest(const FString& Parameters)
{
	using namespace SovNPCVisualLifecycleTests;
	for (const bool bEncounterRestore : {false, true})
	{
		for (const bool bDuringNewCharacter : {false, true})
		{
			FWorld Scope;
			if (!TestNotNull(TEXT("Native world"), Scope.World)) { return false; }
			auto* NPC = Scope.Spawn(bEncounterRestore);
			if (!TestNotNull(TEXT("Current NPC"), NPC)) { return false; }
			auto* Visual = Scope.Visual(NPC);
			if (!TestNotNull(TEXT("Current separate visual"), Visual)) { return false; }
			NPC->bDestroyDuringNewCharacter = bDuringNewCharacter;
			NPC->bDestroyDuringVisualNotification = !bDuringNewCharacter;
			Visual->CompleteMeshesForTest();
			TestTrue(TEXT("The outward callback actually retired its actor"), !IsValid(NPC) || NPC->IsActorBeingDestroyed());
			TestEqual(TEXT("Actual native new-character path was entered once"), NPC->NewCharacterCalls, 1);
			TestEqual(TEXT("Notification is reached only before notification-owned destruction"), NPC->VisualNotifications, bDuringNewCharacter ? 0 : 1);
			TestFalse(TEXT("No stale map-marker continuation"), NPC->HasMarkerForTest());
			TestFalse(TEXT("No stale encounter-ready publication"), NPC->IsEncounterSnapshotReady());
			TestEqual(TEXT("No visual Blueprint continuation after retirement"), Visual->FinalAppearanceEvents, 0);
		}
	}
	return true;
}
#endif
