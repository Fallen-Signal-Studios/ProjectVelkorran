// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include <GameplayTagContainer.h>
#include "SaveSystemStatics.h"
#include "SpawnerBase.generated.h"

/**
A big problem with world partition is that it doesn't work well on dynamically spawned actors. Spawners address this by existing in the world
and thus being managed by world partition. They save their spawned actors to a save record and so remember which of their actors to respawn.

ie if we had a Pickup Spawner, and the player took the pickups, the spawner wouldn't respawn the pickups as it would see which pickups were taken in its saved records.

Spawners replace settlements for spawning NPCs in Narrative 1.2. */
UCLASS(Placeable)
class NARRATIVEARSENAL_API ASpawnerBase : public AActor, public INarrativeSavableActor
{
	GENERATED_BODY()
	
public:	

	// Sets default values for this actor's properties
	ASpawnerBase();

	//Root component - we want the settlement to have a transform 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	class USceneComponent* SpawnerRoot;

	//The GUID that the save system uses to identify and save the settlement
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Spawner Configuration")
	FGuid SpawnerSaveGUID;

	#if WITH_EDITORONLY_DATA

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Display, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UBillboardComponent> SpriteComponent;

	#endif 

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	virtual void SpawnActors();

	UFUNCTION(BlueprintCallable, Category = "Spawner")
	virtual void RemoveActors();

	FGuid GetActorGUID_Implementation() const override;
	void SetActorGUID_Implementation(const FGuid& SavedGUID) override;

	//Whether the spawner is enabled or not 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, SaveGame, Category = "Spawner Configuration")
	bool bActivateOnBeginPlay;

};
