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

public:
	ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer);
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	virtual bool ShouldRespawn_Implementation() const override;

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

protected:
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
};
