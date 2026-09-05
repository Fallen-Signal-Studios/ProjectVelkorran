// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovConsoleUIPolicy
{
	// Slate analog events arrive while menus are paused. Bound the elapsed time so a resume
	// or stalled frame cannot skip an entire record, and reject nonfinite device input.
	inline float ScrollOffset(float Current, float Maximum, float Axis, float DeltaSeconds)
	{
		if (!std::isfinite(Maximum) || Maximum <= 0.f) { return 0.f; }
		Current = std::isfinite(Current) ? std::clamp(Current, 0.f, Maximum) : 0.f;
		if (!std::isfinite(Axis) || !std::isfinite(DeltaSeconds) || DeltaSeconds <= 0.f || std::abs(Axis) <= .2f) { return Current; }
		const float Speed = (std::clamp(std::abs(Axis), .2f, 1.f) - .2f) / .8f * 720.f;
		return std::clamp(Current + (Axis > 0.f ? -Speed : Speed) * std::min(DeltaSeconds, .05f), 0.f, Maximum);
	}
}
