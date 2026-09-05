// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace NarrativeAudioSettingsPolicy
{
inline float Volume(float Value, float Fallback = 1.f)
{ return std::isfinite(Value) ? (Value < 0.f ? 0.f : Value > 1.f ? 1.f : Value) : Fallback; }
inline bool ValidRange(unsigned Value) { return Value <= 2; }
}
