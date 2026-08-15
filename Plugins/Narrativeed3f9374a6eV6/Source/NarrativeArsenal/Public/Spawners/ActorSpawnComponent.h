// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Spawners/SpawnComponent.h"
#include "ActorSpawnComponent.generated.h"

/**
 * Spawn component that spawns in an actor of the specified class. 
 */
UCLASS(ClassGroup=(Spawners), meta = (BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API UActorSpawnComponent : public USpawnComponent
{
	GENERATED_BODY()
	
public: 

	UActorSpawnComponent();

	virtual class AActor* SpawnActor_Implementation() override;
	virtual FString GetEditorLabel_Implementation() const;

	//The actor that we wish to spawn in
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Component")
	TSubclassOf<class AActor> ActorClass;

	//How we should handle any collisions with the spawn 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn Component")
	ESpawnActorCollisionHandlingMethod SpawnCollisionHandlingOverride;

};
