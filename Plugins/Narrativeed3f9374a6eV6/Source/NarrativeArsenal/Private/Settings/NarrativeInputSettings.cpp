// Copyright Narrative Tools 2025.


#include "Settings/NarrativeInputSettings.h"

UNarrativeInputSettings::UNarrativeInputSettings()
{
	AimSensitivity = 1.f; 
}

void UNarrativeInputSettings::SetAimSensitivity(const float NewAimSensitivity)
{
	AimSensitivity = FMath::IsFinite(NewAimSensitivity) ? FMath::Clamp(NewAimSensitivity, 0.05f, 10.f) : 1.f;
	SaveSettings();
}

float UNarrativeInputSettings::GetAimSensitivity() const
{
	return FMath::IsFinite(AimSensitivity) ? FMath::Clamp(AimSensitivity, .05f, 10.f) : 1.f;
}

void UNarrativeInputSettings::SetInvertVertical(const bool NewInvertVertical)
{
	bInvertVertical = NewInvertVertical;
	SaveSettings();
}

bool UNarrativeInputSettings::GetInvertVertical() const
{
	return bInvertVertical;
}

void UNarrativeInputSettings::SetInvertHorizontal(const bool NewInvertHorizontal)
{
	bInvertHorizontal = NewInvertHorizontal;
	SaveSettings();
}

bool UNarrativeInputSettings::GetInvertHorizontal() const
{
	return bInvertHorizontal;
}

void UNarrativeInputSettings::SetCameraSensitivity(float Value)
{ CameraSensitivity = FMath::IsFinite(Value) ? FMath::Clamp(Value, .05f, 10.f) : 1.f; SaveSettings(); }
float UNarrativeInputSettings::GetCameraSensitivity() const
{ return FMath::IsFinite(CameraSensitivity) ? FMath::Clamp(CameraSensitivity, .05f, 10.f) : 1.f; }
void UNarrativeInputSettings::SetGamepadDeadZone(float Value)
{ GamepadDeadZone = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, .9f) : 0.f; SaveSettings(); }
float UNarrativeInputSettings::GetGamepadDeadZone() const
{ return FMath::IsFinite(GamepadDeadZone) ? FMath::Clamp(GamepadDeadZone, 0.f, .9f) : 0.f; }
void UNarrativeInputSettings::SetGamepadAccelerationSeconds(float Value)
{ GamepadAccelerationSeconds = FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.f, 2.f) : 0.f; SaveSettings(); }
float UNarrativeInputSettings::GetGamepadAccelerationSeconds() const
{ return FMath::IsFinite(GamepadAccelerationSeconds) ? FMath::Clamp(GamepadAccelerationSeconds, 0.f, 2.f) : 0.f; }
