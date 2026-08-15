// Copyright Narrative Tools 2022. 

#include "Tales/NarrativeDialogueSettings.h"

UNarrativeDialogueSettings::UNarrativeDialogueSettings()
{
	LettersPerSecondLineDuration = 25.f;
	MinDialogueTextDisplayTime = 2.f;
	DialogueLineAudioSilence = 0.5f;
	bAutoSelectSingleResponse = false;
	bEnableVerticalWiring = true;

	SpeakerColors.Add(FLinearColor(0.036161f, 0.115986f, 0.265625f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.008496f, 0.112847f, 0.025310f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.194444f, 0.021931f, 0.075750f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.010000f, 0.010000f, 0.010000f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.010000f, 0.010000f, 0.010000f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.623529f, 0.509607f, 0.062539f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.100204f, 0.363285f, 0.581597f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.171875f, 0.040824f, 0.006940f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.300000f, 0.300000f, 0.300000f, 1.000000f));
	SpeakerColors.Add(FLinearColor(0.744792f, 0.339469f, 0.673176f, 1.000000f));
}
