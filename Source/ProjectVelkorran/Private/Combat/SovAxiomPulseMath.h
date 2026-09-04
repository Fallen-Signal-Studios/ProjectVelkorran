#pragma once

// Shared by the authoritative pulse and the portable geometry regression tests.
// This file deliberately has no Unreal dependency; it does not resolve actors or apply effects.
#include <cmath>

namespace SovAxiomPulse
{
inline float ChargeAlpha(const float ElapsedSeconds, const float FullChargeSeconds)
{
	if (!std::isfinite(ElapsedSeconds) || !std::isfinite(FullChargeSeconds) || FullChargeSeconds <= 0.f)
	{
		return 0.f;
	}
	if (ElapsedSeconds <= 0.f) { return 0.f; }
	if (ElapsedSeconds >= FullChargeSeconds) { return 1.f; }
	return ElapsedSeconds / FullChargeSeconds;
}

inline float Lerp(const float Minimum, const float Maximum, const float Alpha)
{
	if (!std::isfinite(Minimum) || !std::isfinite(Maximum) || !std::isfinite(Alpha) || Maximum < Minimum)
	{
		return 0.f;
	}
	if (Alpha <= 0.f) { return Minimum; }
	if (Alpha >= 1.f) { return Maximum; }
	// Evaluate in double to avoid intermediate float overflow for valid endpoints.
	return static_cast<float>(static_cast<double>(Minimum) +
		(static_cast<double>(Maximum) - static_cast<double>(Minimum)) * Alpha);
}

inline bool ContainsPoint(const double X, const double Y, const double Z,
	const double ForwardX, const double ForwardY, const double ForwardZ,
	const double Range, const double HalfAngleDegrees)
{
	if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z) ||
		!std::isfinite(ForwardX) || !std::isfinite(ForwardY) || !std::isfinite(ForwardZ) ||
		!std::isfinite(Range) || !std::isfinite(HalfAngleDegrees) ||
		Range <= 0.0 || HalfAngleDegrees < 0.0 || HalfAngleDegrees > 180.0)
	{
		return false;
	}
	const double ForwardLength = std::hypot(std::hypot(ForwardX, ForwardY), ForwardZ);
	const double Distance = std::hypot(std::hypot(X, Y), Z);
	if (!std::isfinite(ForwardLength) || ForwardLength <= 0.0 || !std::isfinite(Distance) || Distance > Range)
	{
		return false;
	}
	if (Distance == 0.0) { return true; }
	const double Dot = (X / Distance) * (ForwardX / ForwardLength) +
		(Y / Distance) * (ForwardY / ForwardLength) + (Z / Distance) * (ForwardZ / ForwardLength);
	constexpr double DegreesToRadians = 0.017453292519943295769;
	// Include the exact cone edge despite rounding in the normalized dot product.
	return Dot + 1.e-12 >= std::cos(HalfAngleDegrees * DegreesToRadians);
}
}
