// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequencePlayer.h"
#include "NarrativeLevelSequencePlayer.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeLevelSequencePlayer : public ULevelSequencePlayer
{
	GENERATED_BODY()
	
protected:

	UNarrativeLevelSequencePlayer(const FObjectInitializer& ObjectInitializer);

	virtual void OnStartedPlaying() override;
	virtual void OnStopped() override;

};
