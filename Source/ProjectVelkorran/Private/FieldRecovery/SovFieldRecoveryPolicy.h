// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>
namespace SovFieldRecoveryPolicy
{
	inline bool ValidCharges(int Current, int Capacity) { return Capacity >= 1 && Capacity <= 10 && Current >= 0 && Current <= Capacity; }
	inline double HealAmount(double Health, double Maximum, double Fraction)
	{
		if (!std::isfinite(Health) || !std::isfinite(Maximum) || !std::isfinite(Fraction)
			|| Health <= 0. || Maximum <= Health || Fraction <= 0. || Fraction > 1.) { return 0.; }
		return std::min(Maximum - Health, Maximum * Fraction);
	}
	inline bool CanBegin(int Current, int Capacity, double Health, double Maximum, double Fraction)
	{ return ValidCharges(Current, Capacity) && Current > 0 && HealAmount(Health, Maximum, Fraction) > 0.; }
	inline bool Completed(double Now, double Started, double Duration)
	{
		return std::isfinite(Now) && std::isfinite(Started) && std::isfinite(Duration) && Duration >= .1 && Duration <= 5.
			&& Now >= Started && Now - Started >= Duration;
	}
}
