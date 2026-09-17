// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "SovNPCCharacterBase.generated.h"

/** Project-owned NPC base with authoritative combat extensions available by default. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovNPCCharacterBase : public ANarrativeNPCCharacter
{
	GENERATED_BODY()
#if WITH_EDITOR
	friend class USovAurelionNPCIdentityLibrary;
#endif

public:
	ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	virtual bool ShouldRespawn_Implementation() const override;

	/** Optional definition for a directly placed encounter NPC. Spawner/restore definitions take priority.
	 * Use the definition's existing matching role class; this does not replace its AI, abilities or factions. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Sovereign|Placed NPC")
	TObjectPtr<class UNPCDefinition> AuthoredPlacedDefinition;
	UFUNCTION(BlueprintPure, Category="Sovereign|Placed NPC")
	class UNPCDefinition* GetAuthoredPlacedDefinition() const { return AuthoredPlacedDefinition; }

	/** Combat roles declare themselves lockable threats; story and support NPCs stay unlockable.
	 * BeginPlay publishes the authored actor tag USovTargetingComponent reads, so an author can
	 * also grant it per Blueprint or per placed instance without touching code. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sovereign|Targeting")
	bool bPermitsHardLock = false;

	/** Called on a deferred replacement before setting its NPC definition. */
	void PrepareForEncounterRestore(const FNPCSpawnInfo& SavedSpawnInfo, const FGuid& SavedGUID);
	const FNPCSpawnInfo& GetEncounterSpawnInfo() const { return SpawnInfo; }
	bool IsEncounterSnapshotReady() const { return bEncounterSnapshotReady; }
	void SetEncounterOwned() { bEncounterOwned = true; }
	void EnsureEncounterController();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovDismembermentComponent* GetDismembermentComponent() const
	{
		return DismembermentComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovCombatSustainDropComponent* GetCombatSustainDropComponent() const
	{
		return CombatSustainDropComponent;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Components")
	class USovStatusComponent* GetStatusComponent() const { return StatusComponent; }

protected:
	bool InitializeAuthoredPlacedDefinition(FString& Error);
	virtual void OnCharacterVisualInitialized() override;
	UPROPERTY(SaveGame)
	FGuid NativeSaveGuid;
	UPROPERTY(SaveGame)
	bool bEncounterOwned = false;
	bool bEncounterRestoreInitialization = false;
	bool bEncounterSnapshotReady = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovDismembermentComponent> DismembermentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovCombatSustainDropComponent> CombatSustainDropComponent;

	/** Authoritative transient status owner inherited by all campaign NPCs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<class USovStatusComponent> StatusComponent;
};
