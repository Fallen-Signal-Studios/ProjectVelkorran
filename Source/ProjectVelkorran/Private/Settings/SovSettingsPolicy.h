// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstdint>
namespace SovSettingsPolicy
{
constexpr std::uint8_t Schema = 2;
inline bool InRange(float Value, float Min, float Max) { return std::isfinite(Value) && Value >= Min && Value <= Max; }
inline bool ValidGameplay(std::uint8_t Preset, float Damage, float Recovery, bool Unlocked)
{
	return Preset <= 4 && (Preset != 3 || Unlocked) && InRange(Damage, .1f, 2.f) && InRange(Recovery, .5f, 2.f);
}
inline bool ValidAssists(float Defense, float Exertion, float Buffer, float MeleeAim, float RangedAim)
{
	return InRange(Defense, 1.f, 2.f) && InRange(Exertion, .1f, 1.f) && InRange(Buffer, 0.f, .2f)
		&& InRange(MeleeAim, 0.f, 1.f) && InRange(RangedAim, 0.f, 1.f);
}
}
