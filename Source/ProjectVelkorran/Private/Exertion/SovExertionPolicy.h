// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovExertionPolicy
{
	inline bool ValidCost(double Value) { return std::isfinite(Value) && Value >= 0.; }
	inline bool CanPay(double Current, double Cost)
	{
		return ValidCost(Current) && ValidCost(Cost) && Current >= Cost;
	}
	inline double Spend(double Current, double Cost) { return CanPay(Current, Cost) ? Current - Cost : Current; }
	inline double RegenSeconds(double Delta, double RemainingDelay)
	{
		return ValidCost(Delta) && ValidCost(RemainingDelay) ? std::max(0., Delta - RemainingDelay) : 0.;
	}
	inline double SprintDrain(double Current, double Rate, double Delta, bool Combat, bool Sprinting)
	{
		return Combat && Sprinting && ValidCost(Current) && ValidCost(Rate) && ValidCost(Delta)
			? std::min(Current, Rate * Delta) : 0.;
	}
	inline double FrameRegen(double Current, double Maximum, double Rate, double Delta, double Delay,
		bool Exerting, double ActiveScale)
	{
		if (!ValidCost(Current) || !ValidCost(Maximum) || !ValidCost(Rate) || !ValidCost(ActiveScale)) { return Current; }
		const double Scale = Exerting ? std::clamp(ActiveScale, 0., 1.) : 1.;
		return std::min(Maximum, Current + Rate * Scale * RegenSeconds(Delta, Delay));
	}
}
