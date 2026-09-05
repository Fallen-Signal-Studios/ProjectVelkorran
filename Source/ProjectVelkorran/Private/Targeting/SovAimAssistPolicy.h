// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <array>
#include <algorithm>
namespace SovAimAssistPolicy
{
constexpr double MaximumLeadSeconds = .6;
constexpr double MaximumCorrectionDegrees = 8.;
inline bool SolveIntercept(double DistanceSquared, double PositionVelocityDot, double VelocitySquared,
	double Speed, double& Time);
struct Vector
{
	double X = 0., Y = 0., Z = 0.;
	Vector operator+(Vector Other) const { return {X + Other.X, Y + Other.Y, Z + Other.Z}; }
	Vector operator-(Vector Other) const { return {X - Other.X, Y - Other.Y, Z - Other.Z}; }
	Vector operator*(double Scale) const { return {X * Scale, Y * Scale, Z * Scale}; }
	double Dot(Vector Other) const { return X * Other.X + Y * Other.Y + Z * Other.Z; }
	bool Finite() const { return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Z); }
};
inline Vector PositionAtTime(Vector Origin, Vector Velocity, Vector Gravity, double Time)
{
	return Origin + Velocity * Time + Gravity * (.5 * Time * Time);
}
namespace Detail
{
using Polynomial = std::array<double, 5>;
inline double Evaluate(const Polynomial& Coefficients, int Degree, double X)
{
	double Value = Coefficients[Degree];
	for (int I = Degree - 1; I >= 0; --I) { Value = Value * X + Coefficients[I]; }
	return Value;
}
/** Derivative roots partition a quartic into monotone intervals, including tangent roots. */
inline int RootsInUnitInterval(Polynomial Coefficients, int Degree, std::array<double, 4>& Roots)
{
	while (Degree > 0 && Coefficients[Degree] == 0.) { --Degree; }
	if (Degree == 0) { return 0; }
	double Scale = 0.;
	for (int I = 0; I <= Degree; ++I) { Scale = std::max(Scale, std::abs(Coefficients[I])); }
	if (!std::isfinite(Scale) || Scale == 0.) { return 0; }
	for (int I = 0; I <= Degree; ++I) { Coefficients[I] /= Scale; }
	if (Degree == 1)
	{
		const double Root = -Coefficients[0] / Coefficients[1];
		if (Root >= 0. && Root <= 1.) { Roots[0] = Root; return 1; }
		return 0;
	}
	Polynomial Derivative{};
	for (int I = 1; I <= Degree; ++I) { Derivative[I - 1] = I * Coefficients[I]; }
	std::array<double, 4> Critical{};
	const int CriticalCount = RootsInUnitInterval(Derivative, Degree - 1, Critical);
	std::array<double, 6> Boundaries{};
	int BoundaryCount = 1;
	for (int I = 0; I < CriticalCount; ++I)
	{
		if (Critical[I] > 0. && Critical[I] < 1.) { Boundaries[BoundaryCount++] = Critical[I]; }
	}
	Boundaries[BoundaryCount++] = 1.;
	int Count = 0;
	const auto Add = [&](double Root)
	{
		if (Count < Degree && (Count == 0 || Root - Roots[Count - 1] > 1.e-10)) { Roots[Count++] = Root; }
	};
	for (int I = 0; I < BoundaryCount; ++I)
	{
		double Left = Boundaries[I];
		double LeftValue = Evaluate(Coefficients, Degree, Left);
		if (std::abs(LeftValue) <= 1.e-12) { Add(Left); }
		if (I + 1 == BoundaryCount) { break; }
		double Right = Boundaries[I + 1];
		const double RightValue = Evaluate(Coefficients, Degree, Right);
		if (LeftValue == 0. || RightValue == 0. || (LeftValue < 0.) == (RightValue < 0.)) { continue; }
		for (int Iteration = 0; Iteration < 64; ++Iteration)
		{
			const double Mid = .5 * (Left + Right);
			const double MidValue = Evaluate(Coefficients, Degree, Mid);
			if ((MidValue < 0.) == (LeftValue < 0.)) { Left = Mid; LeftValue = MidValue; }
			else { Right = Mid; }
		}
		Add(.5 * (Left + Right));
	}
	return Count;
}
}
/** Fixed launch speed, constant target velocity and real gravity. Never lengthens the assistance horizon. */
inline bool SolveBallisticIntercept(Vector Relative, Vector TargetVelocity, Vector Gravity, double Speed,
	double Horizon, bool PreferHighArc, Vector& LaunchVelocity, double& Time)
{
	Time = 0.; LaunchVelocity = {};
	if (!Relative.Finite() || !TargetVelocity.Finite() || !Gravity.Finite() || !std::isfinite(Speed)
		|| !std::isfinite(Horizon) || Speed <= 0. || Horizon <= 0. || Horizon > MaximumLeadSeconds
		|| Relative.Dot(Relative) <= 1.e-12) { return false; }
	if (Gravity.Dot(Gravity) <= 1.e-12)
	{
		double Candidate = 0.;
		if (!SolveIntercept(Relative.Dot(Relative), Relative.Dot(TargetVelocity), TargetVelocity.Dot(TargetVelocity), Speed, Candidate)
			|| Candidate > Horizon) { return false; }
		const Vector Velocity = (Relative + TargetVelocity * Candidate) * (1. / Candidate);
		if (!Velocity.Finite()) { return false; }
		LaunchVelocity = Velocity; Time = Candidate; return true;
	}
	// Solve in normalized time [0,1] so finite very short horizons are well conditioned.
	const Vector V = TargetVelocity * Horizon;
	const Vector G = Gravity * (.5 * Horizon * Horizon);
	const double SH = Speed * Horizon;
	Detail::Polynomial P = {Relative.Dot(Relative), 2. * Relative.Dot(V),
		V.Dot(V) - 2. * Relative.Dot(G) - SH * SH, -2. * V.Dot(G), G.Dot(G)};
	for (double C : P) { if (!std::isfinite(C)) { return false; } }
	std::array<double, 4> Roots{};
	const int Count = Detail::RootsInUnitInterval(P, 4, Roots);
	for (int I = 0; I < Count; ++I)
	{
		const int Index = PreferHighArc ? Count - 1 - I : I;
		const double Candidate = Roots[Index] * Horizon;
		if (Candidate <= 1.e-8 || Candidate > Horizon) { continue; }
		// The late (high) arc crosses back to positive residual. If it is outside
		// the horizon, do not silently substitute the short arc below it.
		const double X = Roots[Index];
		const double Slope = P[1] + X * (2. * P[2] + X * (3. * P[3] + X * 4. * P[4]));
		const double SlopeTolerance = 1.e-8 * std::max(1., SH * SH);
		if (Gravity.Dot(Gravity) > 1.e-12 && (PreferHighArc ? Slope < -SlopeTolerance : Slope > SlopeTolerance)) { continue; }
		const Vector Velocity = (Relative + TargetVelocity * Candidate - Gravity * (.5 * Candidate * Candidate)) * (1. / Candidate);
		const double ActualSpeedSquared = Velocity.Dot(Velocity);
		if (!Velocity.Finite() || !std::isfinite(ActualSpeedSquared)
			|| std::abs(ActualSpeedSquared - Speed * Speed) > 1.e-7 * Speed * Speed) { continue; }
		LaunchVelocity = Velocity; Time = Candidate; return true;
	}
	return false;
}
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
