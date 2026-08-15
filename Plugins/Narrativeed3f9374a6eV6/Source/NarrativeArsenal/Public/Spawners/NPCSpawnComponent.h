// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Spawners/SpawnComponent.h"
#include "AI/Activities/NPCGoalItem.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "NPCSpawnComponent.generated.h"

/**
 * Spawns an NPC when asked by our spawner. 
 */
UCLASS(ClassGroup=(Spawners), meta = (BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API UNPCSpawnComponent : public USpawnComponent
{
	GENERATED_BODY()
	
public: 

	UNPCSpawnComponent();

	virtual bool ShouldSpawnActor();
	virtual bool ShouldDespawnActor();

	virtual class AActor* SpawnActor_Implementation() override;
	virtual bool TryDespawnActor();
	virtual bool RemoveActor();

	virtual FString GetEditorLabel_Implementation() const;

	//Try grab the currently spawned NPC if one exists. 
	UFUNCTION(BlueprintPure, BlueprintCallable, Category = "NPC Spawn Component")
	ANarrativeNPCCharacter* GetSpawnedNPC() const;

	//NPCs within this range of the player wont get despawned; we'll instead tether them to the player 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC Spawn Component")
	float UntetherDistance;

	//If true, we wont spawn the NPC in any more if it was previously killed. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "NPC Spawn Component")
	bool bDontSpawnIfPreviouslyKilled;

	//The NPC we want the spawn to spawn in 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "NPC Spawn Component")
	TObjectPtr<class UNPCDefinition> NPCToSpawn;

	//Optional spawn params we can use in the spawning process
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "NPC Spawn Component")
	FNPCSpawnParams SpawnParams;

	//Optional goal to assign to the NPC when it spawns in 
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category = "NPC Spawn Component")
	TObjectPtr<UNPCGoalItem> OptionalGoal; 

	/**The save GUID to assign to the NPC, so that its stats, items, etc are saved. You only need to set this if the NPC is non-unique
	and you actually want it to save. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "NPC Spawn Component")
	FGuid NPCSaveGUID;

protected:

	//Whether the NPC this spawn created has been killed in the past. If they were we shouldnt spawn them 
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "NPC Spawn Component")
	bool bWasKilled;

	//Whether the NPC was tethered to the player
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "NPC Spawn Component")
	bool bTetheredToPlayer;

	UFUNCTION()
	virtual void SpawnedNPCDied(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);

#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif


};
