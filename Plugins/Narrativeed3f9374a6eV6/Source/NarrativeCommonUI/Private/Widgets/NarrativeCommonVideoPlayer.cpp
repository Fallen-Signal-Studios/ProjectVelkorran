// Copyright Narrative Tools 2024. 


#include "Widgets/NarrativeCommonVideoPlayer.h"

void UNarrativeCommonVideoPlayer::PostInitProperties()
{
	//Common video player has a crash that we're fixing here. 
	if (IsInGameThread() || IsInParallelGameThread())
	{
		Super::PostInitProperties();
	}
	else
	{
		UWidget::PostInitProperties();
	}
}

void UNarrativeCommonVideoPlayer::BPSetVideo(UMediaSource* NewVideo)
{
	SetVideo(NewVideo);
}

void UNarrativeCommonVideoPlayer::BPPlayFromStart()
{
	PlayFromStart();
}

void UNarrativeCommonVideoPlayer::BPPlay()
{
	Play();
}

void UNarrativeCommonVideoPlayer::BPClose()
{
	Close();
}
