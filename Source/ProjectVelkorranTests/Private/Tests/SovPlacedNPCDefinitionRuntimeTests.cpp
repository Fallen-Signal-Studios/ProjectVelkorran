// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlacedNPCDefinitionTestFixtures.h"
#include "AI/NPCDefinition.h"
#include "Characters/SovDroneNPCBase.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovPlacedNPCDefinitionTests
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
		}
		~FWorld() { if (World) { World->DestroyWorld(false); } }
		ASovPlacedNPCDefinitionTestCharacter* Spawn()
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World ? World->SpawnActor<ASovPlacedNPCDefinitionTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params) : nullptr;
		}
	};
	UNPCDefinition* Definition(bool bUnique = false)
	{
		auto* Result = NewObject<UNPCDefinition>();
		Result->NPCClassPath = ASovNPCCharacterBase::StaticClass();
		Result->bAllowMultipleInstances = !bUnique;
		Result->NPCID = FName(TEXT("PlacedDefinitionTest"));
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCDefinitionFallbackTest,
	"ProjectVelkorran.Campaign.PlacedNPC.DefinitionFallbackAndExistingOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCDefinitionFallbackTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Placed NPC fixture"), NPC)) { return false; }
	FString Error;
	TestFalse(TEXT("No authored definition cannot initialize"), NPC->InitializePlaced(Error));
	TestEqual(TEXT("No definition dispatch"), NPC->DefinitionDispatches, 0);
	auto* Authored = Definition();
	NPC->AuthoredPlacedDefinition = Authored;
	TestTrue(TEXT("Matching authored role enters normal definition dispatch"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Actual definition assigned"), NPC->GetNPCDefinition() == Authored);
	TestEqual(TEXT("One native dispatch"), NPC->DefinitionDispatches, 1);
	NPC->AuthoredPlacedDefinition = Definition();
	TestTrue(TEXT("Subsequent calls preserve existing runtime definition"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Existing definition wins over changed authored fallback"), NPC->GetNPCDefinition() == Authored);
	TestEqual(TEXT("No duplicate definition grants"), NPC->DefinitionDispatches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCDefinitionRestoreTest,
	"ProjectVelkorran.Campaign.PlacedNPC.RestoreAndRoleRejection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCDefinitionRestoreTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* NPC = Scope.Spawn();
	if (!TestNotNull(TEXT("Placed NPC fixture"), NPC)) { return false; }
	FString Error;
	auto* WrongRole = Definition();
	WrongRole->NPCClassPath = ASovDroneNPCBase::StaticClass();
	NPC->AuthoredPlacedDefinition = WrongRole;
	TestFalse(TEXT("Different role cannot be applied to the placed actor"), NPC->InitializePlaced(Error));
	TestEqual(TEXT("Rejected role has no content callback"), NPC->DefinitionDispatches, 0);
	NPC->AuthoredPlacedDefinition = Definition();
	NPC->PrepareForEncounterRestore(FNPCSpawnInfo(), FGuid::NewGuid());
	TestFalse(TEXT("Restore cannot invent a missing definition from placed data"), NPC->InitializePlaced(Error));
	auto* Restored = Definition();
	NPC->SetNPCDefinition(Restored);
	TestTrue(TEXT("Supplied restore definition is retained"), NPC->InitializePlaced(Error));
	TestTrue(TEXT("Restore owns actual definition"), NPC->GetNPCDefinition() == Restored);
	TestEqual(TEXT("Restore definition dispatched once"), NPC->DefinitionDispatches, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovPlacedNPCDefinitionUniqueTest,
	"ProjectVelkorran.Campaign.PlacedNPC.UniqueDefinitionAdmission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovPlacedNPCDefinitionUniqueTest::RunTest(const FString& Parameters)
{
	using namespace SovPlacedNPCDefinitionTests;
	FWorld Scope;
	auto* First = Scope.Spawn(); auto* Second = Scope.Spawn();
	if (!TestNotNull(TEXT("First placed actor"), First) || !TestNotNull(TEXT("Second placed actor"), Second)) { return false; }
	auto* Unique = Definition(true);
	First->AuthoredPlacedDefinition = Unique; Second->AuthoredPlacedDefinition = Unique;
	FString Error;
	TestFalse(TEXT("Duplicate unique assignment fails before dispatch"), First->InitializePlaced(Error));
	TestFalse(TEXT("Reversed initialization order also rejects the duplicate"), Second->InitializePlaced(Error));
	TestEqual(TEXT("No definition dispatch on either actor"), First->DefinitionDispatches + Second->DefinitionDispatches, 0);
	Unique->bAllowMultipleInstances = true;
	TestTrue(TEXT("Explicit repeatable definition admits first actor"), First->InitializePlaced(Error));
	TestTrue(TEXT("Explicit repeatable definition admits second actor"), Second->InitializePlaced(Error));
	return true;
}
#endif
