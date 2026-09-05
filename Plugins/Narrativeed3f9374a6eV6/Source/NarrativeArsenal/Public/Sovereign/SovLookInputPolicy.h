// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>
namespace SovLookInputPolicy
{
inline double AccelerationIntegral(double Seconds, double Ramp)
{
	if (Ramp <= 0.0) { return Seconds; }
	const double T = std::clamp(Seconds, 0.0, Ramp);
	return .25 * T + .375 * T * T / Ramp + std::max(0.0, Seconds - Ramp);
}
inline float AverageAcceleration(float Before, float Delta, float Ramp)
{
	if (Ramp <= 0.f || Delta <= 0.f) { return 1.f; }
	return static_cast<float>((AccelerationIntegral(Before + Delta, Ramp) - AccelerationIntegral(Before, Ramp)) / Delta);
}
}
