// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace SovAimAssistPolicy
{
constexpr double MaximumLeadSeconds = .6;
constexpr double MaximumCorrectionDegrees = 8.;
inline bool SolveIntercept(double DistanceSquared, double PositionVelocityDot, double VelocitySquared,
	double Speed, double& Time)
{
	Time = 0.;
	if (!std::isfinite(DistanceSquared) || !std::isfinite(PositionVelocityDot) || !std::isfinite(VelocitySquared)
		|| !std::isfinite(Speed) || DistanceSquared <= 0. || VelocitySquared < 0. || Speed <= 0.) { return false; }
	const double A = VelocitySquared - Speed * Speed;
	const double B = 2. * PositionVelocityDot;
	const double C = DistanceSquared;
	double Candidate = -1.;
	if (std::abs(A) <= 1.e-8 * Speed * Speed)
	{
		if (B >= -1.e-8) { return false; }
		Candidate = -C / B;
	}
	else
	{
		const double Discriminant = B * B - 4. * A * C;
		if (!std::isfinite(Discriminant) || Discriminant < 0.) { return false; }
		const double Root = std::sqrt(Discriminant);
		const double First = (-B - Root) / (2. * A);
		const double Second = (-B + Root) / (2. * A);
		if (First > 0. && Second > 0.) { Candidate = First < Second ? First : Second; }
		else { Candidate = First > 0. ? First : Second; }
	}
	if (!std::isfinite(Candidate) || Candidate <= 0. || Candidate > MaximumLeadSeconds) { return false; }
	Time = Candidate; return true;
}
}
