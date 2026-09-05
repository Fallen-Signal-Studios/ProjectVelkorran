// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Campaign/SovEncounterTypes.h"
#include "NarrativeSavableActor.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "SovPlayerState.generated.h"

/** Project ownership seam for the persistent campaign ASC and player state. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovPlayerState : public ANarrativePlayerState, public INarrativeSavableActor
{
	GENERATED_BODY()

public:
	ASovPlayerState(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	class USovCorruptionAttributeSet* GetCorruptionAttributeSet() const
	{
		return CorruptionAttributeSet;
	}

	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override { PlayerStateSaveGuid = SavedGUID; }
	virtual bool ShouldRespawn_Implementation() const override { return false; }
	virtual void PrepareForSave_Implementation() override;
	/** Replace the active protagonist faction set, including a deliberately empty set. */
	void SetCampaignFactions(const FGameplayTagContainer& Defaults);

	/** Capture does not replace the stored checkpoint until Store succeeds. */
	bool CaptureProtagonistSnapshot(class ASovPlayerCharacterBase* Pawn, FSovProtagonistSnapshot& OutSnapshot, FString& OutError);
	bool StoreProtagonistSnapshot(const FSovProtagonistSnapshot& Snapshot);
	bool FindProtagonistSnapshot(FGameplayTag Protagonist, FSovProtagonistSnapshot& OutSnapshot) const;
	/** Call only after the matching pawn/definition and grants have initialized. */
	bool RestoreProtagonistSnapshot(class ASovPlayerCharacterBase* Pawn, const FSovProtagonistSnapshot& Snapshot, bool bRestoreTransform, FString& OutError);

protected:
	/** Compatibility attributes used only by an explicitly attached legacy corruption component. */
	UPROPERTY(Transient)
	TObjectPtr<class USovCorruptionAttributeSet> CorruptionAttributeSet;
	virtual void BeginPlay() override;
	void EnsureCampaignResourceSaveSelection();
	UPROPERTY(SaveGame)
	TMap<FGameplayTag, FSovProtagonistSnapshot> ProtagonistSnapshots;
	UPROPERTY(SaveGame)
	FGuid PlayerStateSaveGuid;
	bool bRestoringProtagonist = false;
};
