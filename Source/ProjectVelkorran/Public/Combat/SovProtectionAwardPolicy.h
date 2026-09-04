// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovProtectionAwardPolicy
{
	/** Rewinding a clock cannot bypass a source's already committed cooldown. */
	inline bool HasCooldownElapsed(double Now, double PreviousAward, double Cooldown)
	{
		return std::isfinite(Now) && std::isfinite(PreviousAward) && std::isfinite(Cooldown)
			&& Now >= PreviousAward && Now - PreviousAward >= std::max(Cooldown, 0.1);
	}
}
