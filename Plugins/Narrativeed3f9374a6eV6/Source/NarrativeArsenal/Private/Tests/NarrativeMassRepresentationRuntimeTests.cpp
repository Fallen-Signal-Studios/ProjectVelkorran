// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformProperties.h"
#include "GameFramework/Actor.h"
#include "MassAgentComponent.h"
#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
struct FNarrativeMassRepresentationTestAccess
{
	static void Set(const UMassNarrativePedRepresentationActorManagement* Management,
		const EMassActorEnabledType Type, AActor& Actor, FMassCommandBuffer& Buffer)
	{ Management->SetActorEnabled(Type, Actor, 0, Buffer); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeMassDeferredRepresentationTest,
	"ProjectVelkorran.Campaign.Mass.DeferredRepresentationOrderingAndLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNarrativeMassDeferredRepresentationTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues WorldInitialization = UWorld::InitializationValues().AllowAudioPlayback(false)
		.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
		ERHIFeatureLevel::Num, &WorldInitialization, true);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	World->InitWorld(WorldInitialization);
	World->UpdateWorldComponents(!FPlatformProperties::RequiresCookedData(), false);
	UMassEntitySubsystem* Mass = World->GetSubsystem<UMassEntitySubsystem>();
	if (TestNotNull(TEXT("Real Mass entity subsystem"), Mass))
	{
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		const auto Buffer = MakeShared<FMassCommandBuffer>();
		const auto* Management = GetDefault<UMassNarrativePedRepresentationActorManagement>();
		AActor* Actor = World->SpawnActor<AActor>();
		if (TestNotNull(TEXT("Representation actor"), Actor))
		{
			Actor->PrimaryActorTick.bCanEverTick = true;
			Actor->SetActorTickEnabled(true);
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
			FNarrativeMassRepresentationTestAccess::Set(Management, EMassActorEnabledType::Disabled, *Actor, *Buffer);
			FNarrativeMassRepresentationTestAccess::Set(Management, EMassActorEnabledType::HighRes, *Actor, *Buffer);
			Manager.FlushCommands(Buffer);
			TestTrue(TEXT("Disable then enable before flush leaves collision enabled"), Actor->GetActorEnableCollision());
			TestFalse(TEXT("Disable then enable before flush leaves actor visible"), Actor->IsHidden());
			TestTrue(TEXT("Disable then enable before flush leaves actor tick enabled"), Actor->IsActorTickEnabled());
			Actor->SetActorHiddenInGame(true); // Collision already matches, visibility does not.
			FNarrativeMassRepresentationTestAccess::Set(Management, EMassActorEnabledType::HighRes, *Actor, *Buffer);
			Manager.FlushCommands(Buffer);
			TestFalse(TEXT("Visibility is reconciled independently of collision"), Actor->IsHidden());
			FNarrativeMassRepresentationTestAccess::Set(Management, EMassActorEnabledType::Disabled, *Actor, *Buffer);
			auto* NewAssociation = NewObject<UMassAgentComponent>(Actor);
			Actor->AddInstanceComponent(NewAssociation);
			Manager.FlushCommands(Buffer);
			TestTrue(TEXT("Changed Mass component association rejects a stale command"), Actor->GetActorEnableCollision());
			FNarrativeMassRepresentationTestAccess::Set(Management, EMassActorEnabledType::Disabled, *Actor, *Buffer);
			Actor->Destroy();
			Manager.FlushCommands(Buffer); // Must not dereference the destroyed actor.
			TestTrue(TEXT("Destroyed actor stays destroyed after pending representation flush"), Actor->IsActorBeingDestroyed());
		}
	}
	World->DestroyWorld(false);
	if (GEngine) { GEngine->DestroyWorldContext(World); }
	return true;
}
#endif
