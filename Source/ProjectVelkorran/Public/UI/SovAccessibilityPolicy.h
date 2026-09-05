// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstdint>

namespace SovAccessibilityPolicy
{
	inline bool Range(float Value, float Min, float Max) { return std::isfinite(Value) && Value >= Min && Value <= Max; }
	inline bool ValidLayout(float UI, float Subtitle, float Opacity, int Characters, int Lines, float Outline)
	{
		return Range(UI, 1.f, 2.f) && Range(Subtitle, 1.f, 2.5f) && Range(Opacity, 0.f, 1.f)
			&& Characters >= 20 && Characters <= 64 && Lines >= 1 && Lines <= 4 && Range(Outline, 1.f, 6.f);
	}
	inline bool ValidPressure(uint8_t Mode, float Minimum, float Extension)
	{ return Mode <= 2 && Range(Minimum, 2.f, 30.f) && Range(Extension, 1.f, 5.f); }
	inline bool ValidColor(float R, float G, float B, float A)
	{ return Range(R, 0.f, 1.f) && Range(G, 0.f, 1.f) && Range(B, 0.f, 1.f) && A == 1.f; }
	inline float Pulse(float Seconds, bool Enabled)
	{ return Enabled && std::isfinite(Seconds) ? .85f + .15f * std::sin(Seconds * 3.14159265f) : 1.f; }
}
