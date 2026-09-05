// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>

namespace SovCombatInputPolicy
{
	inline bool ValidWindow(double OpensAfter, double ClosesAfter)
	{
		return std::isfinite(OpensAfter) && std::isfinite(ClosesAfter)
			&& OpensAfter >= 0. && ClosesAfter > OpensAfter && ClosesAfter <= 10.;
	}
	inline bool CanConsume(double Now, double PressedAt, double OpensAt, double ClosesAt, double Lifetime)
	{
		return std::isfinite(Now) && std::isfinite(PressedAt) && std::isfinite(OpensAt)
			&& std::isfinite(ClosesAt) && std::isfinite(Lifetime) && Lifetime >= 0.
			&& Now >= OpensAt && Now <= ClosesAt && PressedAt <= Now && Now - PressedAt <= Lifetime;
	}
}
