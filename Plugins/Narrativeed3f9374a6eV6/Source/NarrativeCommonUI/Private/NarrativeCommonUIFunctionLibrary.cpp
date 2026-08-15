// Copyright Narrative Tools 2022. 


#include "NarrativeCommonUIFunctionLibrary.h"
#include "NarrativeCommonUISubsystem.h"
#include "NarrativeCommonHUD.h"
#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <Kismet/GameplayStatics.h>
#include <CommonVideoPlayer.h>


void UNarrativeCommonUIFunctionLibrary::PlayCommonVideoPlayer(class UCommonVideoPlayer* VideoPlayer)
{
	if (VideoPlayer)
	{
		VideoPlayer->PlayFromStart();
		VideoPlayer->SetLooping(true);
	}
}

void UNarrativeCommonUIFunctionLibrary::SetCommonVideoPlayerSource(class UCommonVideoPlayer* VideoPlayer, class UMediaSource* NewVideo)
{
	if (VideoPlayer)
	{
		VideoPlayer->SetVideo(NewVideo);
	}
}
