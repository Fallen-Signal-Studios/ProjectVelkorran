// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/NarrativeMassParticipantBridgeTestFixtures.h"

#include "AI/Mass/Peds/MassNarrativePedRepresentationActorManagement.h"
#include "AI/Mass/Peds/MassPedSpawnerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MassAgentComponent.h"
#include "MassCommandBuffer.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/StructView.h"

#if WITH_AUTOMATION_TESTS
struct FNarrativeMassParticipantBridgeTestAccess
{
	static ESpawnRequestStatus Spawn(const UMassPedSpawnerSubsystem* Spawner, const FMassActorSpawnRequest& Request,
		TObjectPtr<AActor>& Actor, FActorSpawnParameters& Parameters)
	{
		return Spawner->SpawnActor(FConstStructView::Make(Request), Actor, Parameters);
	}

	static void Set(const EMassActorEnabledType Type, AActor& Actor, const FMassEntityHandle Entity,
		FMassCommandBuffer& Buffer)
	{
		GetDefault<UMassNarrativePedRepresentationActorManagement>()->SetActorEnabled(Type, Actor, Entity.Index, Buffer);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeMassParticipantReceiptTest,
	"ProjectVelkorran.Campaign.Mass.PlainActorReceiptRejectsStaleEntityEpoch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNarrativeMassParticipantReceiptTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("World"), World)) { return false; }
	if (GEngine) { GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World); }
	World->InitializeNewWorld(UWorld::InitializationValues().AllowAudioPlayback(false)
		.RequiresHitProxies(false).CreatePhysicsScene(true).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false));
	UMassEntitySubsystem* Mass = World->GetSubsystem<UMassEntitySubsystem>();
	if (TestNotNull(TEXT("Real Mass entity subsystem"), Mass))
	{
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		const TArray<const UScriptStruct*> Fragments = { FNarrativeMassParticipantFragment::StaticStruct() };
		const FMassArchetypeHandle Archetype = Manager.CreateArchetype(Fragments);
		const FMassEntityHandle FirstEntity = Manager.CreateEntity(Archetype);
		const FMassEntityHandle SecondEntity = Manager.CreateEntity(Archetype);
		auto* Owner = World->SpawnActor<ANarrativeMassParticipantTestOwner>();
		AActor* Actor = World->SpawnActor<AActor>();
		if (TestNotNull(TEXT("Owner"), Owner) && TestNotNull(TEXT("Plain representation actor"), Actor))
		{
			auto& First = Manager.GetFragmentDataChecked<FNarrativeMassParticipantFragment>(FirstEntity);
			First.Owner = Owner;
			First.ActorIdentity = FGuid::NewGuid();
			First.ParticipantId = TEXT("Participant");
			First.EncounterId = TEXT("Encounter");
			First.Generation = 1;
			auto& Second = Manager.GetFragmentDataChecked<FNarrativeMassParticipantFragment>(SecondEntity);
			Second = First;
			Second.Generation = 2;
			UMassPedSpawnerSubsystem* Spawner = World->GetSubsystem<UMassPedSpawnerSubsystem>();
			if (TestNotNull(TEXT("Narrative Mass actor spawner"), Spawner))
			{
				FMassActorSpawnRequest Request;
				Request.MassAgent = FirstEntity;
				Request.Template = AActor::StaticClass();
				Request.Transform = FTransform::Identity;
				Request.Guid = First.ActorIdentity;
				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Name = Actor->GetFName(); // Must not alias an existing actor's name.
				TObjectPtr<AActor> Spawned = nullptr;
				TestEqual(TEXT("Participant without ped fragments spawns a plain template actor"),
					FNarrativeMassParticipantBridgeTestAccess::Spawn(Spawner, Request, Spawned, SpawnParameters), ESpawnRequestStatus::Succeeded);
				if (TestNotNull(TEXT("Plain actor result"), Spawned.Get()))
				{
					TestTrue(TEXT("Campaign spawn does not reuse a same-name actor"), Spawned != Actor);
					TestTrue(TEXT("Campaign spawn uses precisely the configured non-NPC class"), Spawned->GetClass() == AActor::StaticClass());
					Spawned->Destroy();
				}
			}
			auto* Receipt = NewObject<UNarrativeMassParticipantReceiptComponent>(Actor);
			Actor->AddInstanceComponent(Receipt);
			Receipt->RegisterComponent();
			TestNull(TEXT("Receipt actor deliberately has no MassAgent"), Actor->FindComponentByClass<UMassAgentComponent>());
			TestTrue(TEXT("Owner accepts the first exact entity receipt"), Receipt->Bind(Manager.AsShared(), FirstEntity));
			const uint64 FirstEpoch = Receipt->GetBindingEpoch();
			Actor->PrimaryActorTick.bCanEverTick = true;
			Actor->SetActorHiddenInGame(false);
			const auto Buffer = MakeShared<FMassCommandBuffer>();
			FNarrativeMassParticipantBridgeTestAccess::Set(EMassActorEnabledType::Disabled, *Actor, FirstEntity, *Buffer);
			TestTrue(TEXT("The same actor can acquire a newer entity receipt"), Receipt->Bind(Manager.AsShared(), SecondEntity));
			TestTrue(TEXT("Actor-local receipt epoch advances on reuse"), Receipt->GetBindingEpoch() > FirstEpoch);
			Manager.FlushCommands(Buffer);
			TestFalse(TEXT("Old entity disable cannot hide the reused actor without a MassAgent"), Actor->IsHidden());

			FNarrativeMassParticipantBridgeTestAccess::Set(EMassActorEnabledType::Disabled, *Actor, SecondEntity, *Buffer);
			FNarrativeMassParticipantBridgeTestAccess::Set(EMassActorEnabledType::HighRes, *Actor, SecondEntity, *Buffer);
			Actor->SetActorEnableCollision(true);
			Actor->SetActorTickEnabled(true);
			Manager.FlushCommands(Buffer);
			TestFalse(TEXT("Current disable then enable preserves ordering"), Actor->IsHidden());
			TestFalse(TEXT("Presentation-only enable never restores combat collision"), Actor->GetActorEnableCollision());
			TestFalse(TEXT("Presentation-only enable never restores actor ticking"), Actor->IsActorTickEnabled());

			FNarrativeMassParticipantBridgeTestAccess::Set(EMassActorEnabledType::Disabled, *Actor, SecondEntity, *Buffer);
			++Second.Generation;
			Manager.FlushCommands(Buffer);
			TestFalse(TEXT("Fragment generation mutation rejects queued work"), Actor->IsHidden());
			TestFalse(TEXT("Old generation receipt is no longer current"), Receipt->IsCurrent(Manager));
			TestTrue(TEXT("Owner accepts updated generation"), Receipt->Bind(Manager.AsShared(), SecondEntity));

			FNarrativeMassParticipantBridgeTestAccess::Set(EMassActorEnabledType::Disabled, *Actor, SecondEntity, *Buffer);
			Receipt->Reset();
			TestTrue(TEXT("Identical entity/generation can be rebound after release"), Receipt->Bind(Manager.AsShared(), SecondEntity));
			Manager.FlushCommands(Buffer);
			TestFalse(TEXT("Local receipt epoch rejects work even when entity and generation repeat"), Actor->IsHidden());
			TestTrue(TEXT("Rebinding sends release notifications"), Owner->ReleaseCalls >= 3);

			FNarrativeMassParticipantBridgeTestAccess::Set(EMassActorEnabledType::Disabled, *Actor, SecondEntity, *Buffer);
			Owner->Destroy();
			Manager.FlushCommands(Buffer);
			TestFalse(TEXT("Destroyed owner cannot authorize queued representation work"), Actor->IsHidden());
			TestFalse(TEXT("Destroyed owner cannot acquire a new receipt"), Receipt->Bind(Manager.AsShared(), SecondEntity));
			Actor->Destroy();
		}
		Manager.DestroyEntity(FirstEntity);
		Manager.DestroyEntity(SecondEntity);
	}
	World->DestroyWorld(false);
	if (GEngine) { GEngine->DestroyWorldContext(World); }
	return true;
}
#endif
