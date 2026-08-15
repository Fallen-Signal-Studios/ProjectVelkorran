// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleSpawnerSubsystem.h"

#include "ChaosVehicleMovementComponent.h"
#include "MassEntityUtils.h"
#include "MassEntityView.h"
#include "MassMovementFragments.h"
#include "Vehicles/NarrativeVehicleBase.h"
#include "Vehicles/Mass/VehicleFragments.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "Interaction/NPCInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Vehicles/MountComponent.h"

ESpawnRequestStatus UVehicleSpawnerSubsystem::SpawnActor(FConstStructView SpawnRequestView,
                                                         TObjectPtr<AActor>& OutSpawnedActor, FActorSpawnParameters& InOutSpawnParameters) const
{
	auto Result = Super::SpawnActor(SpawnRequestView, OutSpawnedActor, InOutSpawnParameters);

	auto VehicleComponent = OutSpawnedActor->FindComponentByClass<UChaosVehicleMovementComponent>();
	check(VehicleComponent);

	auto PrimitiveRoot = Cast<UPrimitiveComponent>(OutSpawnedActor->GetRootComponent());
	check(PrimitiveRoot);

	const FMassActorSpawnRequest& SpawnRequest = SpawnRequestView.Get<const FMassActorSpawnRequest>();
	FMassEntityView EntityView = FMassEntityView(UE::Mass::Utils::GetEntityManagerChecked(*GetWorld()), SpawnRequest.MassAgent);

	FMassVelocityFragment& VelocityFragment = EntityView.GetFragmentData<FMassVelocityFragment>();
	FVehicleLocomotionFragment& VehicleLocomotion = EntityView.GetFragmentData<FVehicleLocomotionFragment>();
	FSeedFragment& SeedFragment = EntityView.GetFragmentData<FSeedFragment>();
	const FPassengerSettingsFragment& OccupantFragment = EntityView.GetConstSharedFragmentData<
		FPassengerSettingsFragment>();
	FVehiclePassengersFragment& PassengerFragment = EntityView.GetFragmentData<FVehiclePassengersFragment>();

	// Init using simple velocity 
	FBaseSnapshotData BaseSnapshotData;
	BaseSnapshotData.Transform = OutSpawnedActor->GetTransform();
	BaseSnapshotData.LinearVelocity = VelocityFragment.Value;
	BaseSnapshotData.AngularVelocity = FVector::ZeroVector;
	VehicleComponent->SetBaseSnapshot(BaseSnapshotData);
	
	if (auto VehicleBase = Cast<ANarrativeVehicleBase>(OutSpawnedActor))
	{
		// Set vehicle color, and spawn some occupants in the vehicle. 
		VehicleBase->SetRandomSeed(SeedFragment.Seed);
		VehicleBase->SetManagedByMass(true);

		if (auto MountComp = VehicleBase->FindComponentByClass<UMountComponent>())
		{
			//todo gareth does this need to be a TPair? Convert.
			TArray<class UNPCDefinition*> PassengerDefs;

			for (auto& PDef : PassengerFragment.Passengers)
			{
				PassengerDefs.Add(PDef.Value);
			}

			MountComp->AddOccupants(PassengerDefs, SeedFragment.Seed);
		}
	}

		//Removed as we've moved this into MountComp. 
		//Next, we'll need to spawn some occupants. 
		//if (UNarrativeCharacterSubsystem* CharSub = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
		//{
		//	auto& OccupantDefs = PassengerFragment.Passengers;

		//	int32 Idx = 0;

		//	for (auto& OccupantToSpawn : OccupantDefs)
		//	{
		//		auto InteractableComp = VehicleBase->FindComponentByClass<UNarrativeInteractableComponent>();
		//		auto NumSlots = InteractableComp ? InteractableComp->InteractionSlots.Num() : 0;

		//		// Trying to spawn an npc on an invalid vehicle seat
		//		if (OccupantToSpawn.Key >= NumSlots)
		//		{
		//			continue;
		//		}
		//		
		//		//Seed the occupant using the cars seed. 
		//		FNPCSpawnParams OccupantParams;
		//		OccupantParams.bOverride_CharacterRandomSeed = true;
		//		OccupantParams.CharacterRandomSeed = SeedFragment.Seed + Idx + 1;

		//		FTransform SpawnT = VehicleBase->GetActorTransform();
		//		SpawnT.SetLocation(SpawnT.GetLocation() + FVector(0.f, 0.f, 10000.f));

		//		if (ANarrativeNPCCharacter* NPCChar = CharSub->SpawnNPC(OccupantToSpawn.Value, FTransform(), OccupantParams))
		//		{
		//			ActorToSeatIndexMap.Add({NPCChar, OccupantToSpawn.Key});

		//			//Disable collision otherwise NPC collides with car 
		//			NPCChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		//			NPCChar->GetCharacterMovement()->SetMovementMode(MOVE_None);
		//			//NPCChar->GetCharacterMovement()->SetMovementMode(MOVE_None);
		//			NPCChar->AttachToActor(VehicleBase, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

		//			//Now, we need to callback to when the occupant has actually loaded their appearance in before we attach them to the vehicle. 
		//			NPCChar->CharacterVisualInitialized.AddUniqueDynamic(this, &UVehicleSpawnerSubsystem::SpawnedOccupantAppearanceReady);
		//		}

		//		++Idx;
		//	}

	
	return Result;	
}

//void UVehicleSpawnerSubsystem::SpawnedOccupantAppearanceReady(class ANarrativeCharacter* Character)
//{
//	if (ANarrativeNPCCharacter* NPCChar = Cast<ANarrativeNPCCharacter>(Character))
//	{
//		if (ANarrativeVehicleBase* VehicleBase = CastChecked<ANarrativeVehicleBase>(Character->GetAttachParentActor()))
//		{
//			//TODO interactable not added in cpp yet, use findcomponent. 
//			UNarrativeInteractableComponent* Interactable = VehicleBase->GetComponentByClass<UNarrativeInteractableComponent>();
//			check(Interactable);
//
//			if (ANarrativeNPCController* NPCController = NPCChar->GetNPCController())
//			{
//				if (UNPCInteractionComponent* NPCInteraction = NPCController->GetInteractionComponent())
//				{
//					if (NPCInteraction->TargetInteractionSlot(Interactable, ActorToSeatIndexMap[NPCChar], false))
//					{
//						const bool bGotInCar = NPCInteraction->RunInteractBehavior(false);
//						ActorToSeatIndexMap.Remove(NPCChar);
//
//						NPCChar->CharacterVisualInitialized.RemoveAll(this);
//
//						return;
//
//					}
//				}
//			}
//		}
//	}
//
//	//Something went wrong! 
//	check(false);
//}

