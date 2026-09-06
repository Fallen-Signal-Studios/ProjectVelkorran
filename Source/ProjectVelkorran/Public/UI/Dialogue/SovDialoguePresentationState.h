// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UI/Dialogue/SovDialoguePressurePolicy.h"
struct PROJECTVELKORRAN_API FSovDialoguePresentationState
{
	SovDialoguePressure::State Pressure;
	TArray<FText> Choices;
	FText Speaker;
	FGuid SpeechRequest;
	int64 ReplyRevision = 0;
	uint64 SpeechGeneration = 0;
	int32 AnnouncedSelection = 0;
	bool bNarration = false;
	bool bAnnouncementPending = false;
	bool bSuspended = false;
	bool bFullAnnouncementComplete = false;
	bool bSelectionAnnouncement = false;
	bool bUnsupported = false;
	int32 SelectedIndex = 0;
};
