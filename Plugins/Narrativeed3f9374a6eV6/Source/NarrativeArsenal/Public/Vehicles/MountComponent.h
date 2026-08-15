// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractableComponent.h"
#include "MountComponent.generated.h"

/**
 * Base interactable class for mounts such as horses, vehicles, etc. 
 */
UCLASS()
class NARRATIVEARSENAL_API UMountComponent : public UNarrativeInteractableComponent
{
	GENERATED_BODY()
	
public:

	virtual void BeginPlay() override; 

	/**Add a bunch of NPC occupants to the mount. Gameplay code will very often want to do this, such as mass traffic, 
	prerecorded cinematic vehicles and so on often won't have any occupants by default, and you need to quickly add
	a bunch. */
	UFUNCTION(BlueprintCallable, Category = "Occupants")
	virtual bool AddOccupants(TArray<class UNPCDefinition*> OccupantDefs, int32 OptionalSeed=-1);

	//Need to defer mounting the vehicle until the occupants appearance is ready. 
	UFUNCTION()
	virtual void SpawnedOccupantAppearanceReady(class ANarrativeCharacter* Character);

	UPROPERTY()
	mutable TMap<AActor*, int> ActorToSeatIndexMap;

	//Need this so sequencer can key these on if required. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Occupants")
	bool bAddOccupantsOnBeginPlay = false;

	//On beginplay we'll use this to populate the car with occupants for you. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Occupants")
	TArray<class UNPCDefinition*> AutoAddOccupants;

};
