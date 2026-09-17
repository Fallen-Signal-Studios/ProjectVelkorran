// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPlacedNPCDefinitionTestFixtures.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "Characters/SovDominionHandler.h"
#include "Characters/SovDroneNPCBase.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Targeting/SovTargetingComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/Script.h"

#if WITH_AUTOMATION_TESTS
namespace SovThreatEligibilityTests
{
	/** Minimal game world; NPC BeginPlay is dispatched explicitly, as the placed-NPC tests do. */
	struct FWorldScope
	{
#if WITH_EDITOR
		FEditorScriptExecutionGuard ScriptGuard;
#endif
		UWorld* World = nullptr;
		FWorldScope()
		{
			const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
				.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
		}
		~FWorldScope() { if (World) { World->DestroyWorld(false); } }
		ASovPlacedNPCDefinitionTestCharacter* Spawn() const
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World ? World->SpawnActor<ASovPlacedNPCDefinitionTestCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params) : nullptr;
		}
	};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSovThreatEligibilityTest,
	"ProjectVelkorran.Campaign.Targeting.CombatRolesPermitHardLock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSovThreatEligibilityTest::RunTest(const FString& Parameters)
{
	// Hard lock needs an actor tag no authored asset carried, so no enemy in the campaign could be
	// locked (audit PC2-07). Combat roles now declare it in code; support roles must not inherit it.
	const auto Permits = [](UClass* Class)
	{
		const auto* Default = Class ? Cast<ASovNPCCharacterBase>(Class->GetDefaultObject()) : nullptr;
		return Default && Default->bPermitsHardLock;
	};
	TestTrue(TEXT("Linkbound is a lockable threat"), Permits(ASovAurelionLinkbound::StaticClass()));
	TestTrue(TEXT("Wall runners inherit the Linkbound threat declaration"), Permits(ASovAurelionWallRunner::StaticClass()));
	TestTrue(TEXT("Elites inherit the Linkbound threat declaration"), Permits(ASovAurelionElite::StaticClass()));
	TestTrue(TEXT("Weaver is a lockable threat"), Permits(ASovAurelionWeaver::StaticClass()));
	TestTrue(TEXT("Every combat drone is a lockable threat"), Permits(ASovDroneNPCBase::StaticClass()));
	TestTrue(TEXT("Security drones inherit the drone threat declaration"), Permits(ASovAurelionSecurityDrone::StaticClass()));
	TestTrue(TEXT("Dominion handlers are lockable threats"), Permits(ASovDominionHandler::StaticClass()));
	TestFalse(TEXT("The shared NPC base grants no threat focus by itself"), Permits(ASovNPCCharacterBase::StaticClass()));
	TestFalse(TEXT("A protagonist's companion is never a lock target"), Permits(ASovProtagonistCompanionCharacter::StaticClass()));

	// The declaration reaches the world as the actor tag the targeting component actually reads.
	using namespace SovThreatEligibilityTests;
	FWorldScope Scope;
	auto* Threat = Scope.Spawn();
	auto* Bystander = Scope.Spawn();
	if (!TestNotNull(TEXT("Threat fixture"), Threat) || !TestNotNull(TEXT("Bystander fixture"), Bystander)) { return false; }
	TestFalse(TEXT("An undeclared NPC carries no hard-lock permission"),
		Bystander->ActorHasTag(USovTargetingComponent::HardLockPermissionTag()));
	Threat->bPermitsHardLock = true;
	// An author may already have tagged the same actor; publication must stay idempotent.
	Threat->Tags.Add(USovTargetingComponent::HardLockPermissionTag());
	Threat->DispatchBeginPlay();
	TestTrue(TEXT("A declared threat publishes the hard-lock permission"),
		Threat->ActorHasTag(USovTargetingComponent::HardLockPermissionTag()));
	int32 Published = 0;
	for (const FName& Tag : Threat->Tags) { Published += Tag == USovTargetingComponent::HardLockPermissionTag() ? 1 : 0; }
	TestEqual(TEXT("Publication never duplicates an authored tag"), Published, 1);
	return true;
}
#endif
