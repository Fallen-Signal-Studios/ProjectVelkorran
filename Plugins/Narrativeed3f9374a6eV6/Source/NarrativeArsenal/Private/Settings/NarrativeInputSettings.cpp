// Copyright Narrative Tools 2025.


#include "Settings/NarrativeInputSettings.h"

UNarrativeInputSettings::UNarrativeInputSettings()
{
	AimSensitivity = 1.f; 
}

void UNarrativeInputSettings::SetAimSensitivity(const float NewAimSensitivity)
{
	AimSensitivity = NewAimSensitivity;
}

float UNarrativeInputSettings::GetAimSensitivity() const
{
	return AimSensitivity;
}

void UNarrativeInputSettings::SetInvertVertical(const bool NewInvertVertical)
{
	bInvertVertical = NewInvertVertical;
}

bool UNarrativeInputSettings::GetInvertVertical() const
{
	return bInvertVertical;
}

void UNarrativeInputSettings::SetInvertHorizontal(const bool NewInvertHorizontal)
{
	bInvertHorizontal = NewInvertHorizontal;
}

bool UNarrativeInputSettings::GetInvertHorizontal() const
{
	return bInvertHorizontal;
}
