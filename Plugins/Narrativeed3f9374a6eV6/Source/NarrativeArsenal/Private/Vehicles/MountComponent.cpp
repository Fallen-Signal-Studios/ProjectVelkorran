// Copyright Narrative Tools 2025.


#include "Vehicles/MountComponent.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "Interaction/NPCInteractionComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UMountComponent::BeginPlay()
{
	Super::BeginPlay();

	//If we're a cinematic vehicle etc spawn some occupants onto the mount 
	if (bAddOccupantsOnBeginPlay && AutoAddOccupants.Num())
	{
		AddOccupants(AutoAddOccupants, -1);
	}
}

bool UMountComponent::AddOccupants(TArray<class UNPCDefinition*> OccupantDefs, int32 OptionalSeed)
{
	AActor* MountActor = GetOwner();

	if(MountActor)
	{
		//Next, we'll need to spawn some occupants. 
		if (UNarrativeCharacterSubsystem* CharSub = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
		{
			int32 Idx = 0;
			auto NumSlots = InteractionSlots.Num();

			for (auto& OccupantToSpawn : OccupantDefs)
			{

				// Trying to spawn an npc on an invalid vehicle seat
				if (Idx < NumSlots)
				{
					//Seed the occupant using the cars seed. 
					FNPCSpawnParams OccupantParams;
					OccupantParams.bOverride_CharacterRandomSeed = OptionalSeed > 0 ? OptionalSeed : -1;
					OccupantParams.CharacterRandomSeed = OptionalSeed + Idx + 1;

					FTransform SpawnT = MountActor->GetActorTransform();
					SpawnT.SetLocation(SpawnT.GetLocation() + FVector(0.f, 0.f, 10000.f));

					if (ANarrativeNPCCharacter* NPCChar = CharSub->SpawnNPC(OccupantToSpawn, FTransform(), OccupantParams))
					{
						ActorToSeatIndexMap.Add({ NPCChar, Idx });

						//Disable collision otherwise NPC collides with car 
						NPCChar->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						NPCChar->GetCharacterMovement()->SetMovementMode(MOVE_None);
						//NPCChar->GetCharacterMovement()->SetMovementMode(MOVE_None);
						NPCChar->AttachToActor(MountActor, FAttachmentTransformRules::SnapToTargetNotIncludingScale);

						//Now, we need to callback to when the occupant has actually loaded their appearance in before we attach them to the vehicle. 
						NPCChar->CharacterVisualInitialized.AddUniqueDynamic(this, &UMountComponent::SpawnedOccupantAppearanceReady);
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Attempted to spawn occupant at %d but mount only has %d seats"), Idx, NumSlots);
				}

				++Idx;
			}
		}
		return true;
	}

	return false; 
}

void UMountComponent::SpawnedOccupantAppearanceReady(class ANarrativeCharacter* Character)
{
	if (ANarrativeNPCCharacter* NPCChar = Cast<ANarrativeNPCCharacter>(Character))
	{
		//Char should be attached to the mount already, we're just activating the mount behavior. 
		if (ensure(Character->GetAttachParentActor()))
		{
			if (ANarrativeNPCController* NPCController = NPCChar->GetNPCController())
			{
				if (UNPCInteractionComponent* NPCInteraction = NPCController->GetInteractionComponent())
				{
					if (NPCInteraction->TargetInteractionSlot(this, ActorToSeatIndexMap[NPCChar], false))
					{
						const bool bGotInCar = NPCInteraction->RunInteractBehavior(false);
						ActorToSeatIndexMap.Remove(NPCChar);

						NPCChar->CharacterVisualInitialized.RemoveAll(this);

						return;

					}
				}
			}
		}
	}

	//Something went wrong! 
	check(false);
}

