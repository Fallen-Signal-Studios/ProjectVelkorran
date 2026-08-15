// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include <MediaSource.h>
#include "NarrativeCommonUIFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVECOMMONUI_API UNarrativeCommonUIFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public: 

	//Tell a common video player to play, as the play function isn't exposed to blueprints.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Narrative Common UI")
	static void PlayCommonVideoPlayer(class UCommonVideoPlayer* VideoPlayer);

	//Tell a common video player to set its source, as the play function isn't exposed to blueprints.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Narrative Common UI")
	static void SetCommonVideoPlayerSource(class UCommonVideoPlayer* VideoPlayer, class UMediaSource* NewVideo);
};
