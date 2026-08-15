// Copyright Narrative Tools 2025.

#include "TimeOfDay/TimeOfDay.h"
#include "Settings/NarrativeTimeOfDaySettings.h"

FTimeOfDay::FTimeOfDay()
{
	const UNarrativeTimeOfDaySettings* TimeOfDaySettings = GetDefault<UNarrativeTimeOfDaySettings>();
	Time = TimeOfDaySettings? TimeOfDaySettings->DefaultTimeOfDay.Time : 0.0f;
}
