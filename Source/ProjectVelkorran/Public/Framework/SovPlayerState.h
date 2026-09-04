// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "SovPlayerState.generated.h"

/** Project ownership seam for the persistent campaign ASC and player state. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovPlayerState : public ANarrativePlayerState
{
	GENERATED_BODY()

public:
	ASovPlayerState(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	class USovCorruptionAttributeSet* GetCorruptionAttributeSet() const
	{
		return CorruptionAttributeSet;
	}

protected:
	/** Persistent player-owned Eclipse exposure attributes. */
	UPROPERTY(Transient)
	TObjectPtr<class USovCorruptionAttributeSet> CorruptionAttributeSet;
};
