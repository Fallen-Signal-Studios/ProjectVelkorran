// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <limits>

namespace SovCombatTransaction
{
	/** Keep valid large tuning values from overflowing a float attribute/receipt. */
	inline float BoundedProduct(double Left, double Right)
	{
		if (!std::isfinite(Left) || !std::isfinite(Right) || Left <= 0. || Right <= 0.) { return 0.f; }
		const double Limit = std::numeric_limits<float>::max();
		if (Left >= Limit / Right) { return std::numeric_limits<float>::max(); }
		return static_cast<float>(Left * Right);
	}
	enum class EPacket { None, Body, Poise, Control };
	/** One and only one packet enters AttributeSet routing; malformed body input fails closed. */
	inline EPacket SelectPacket(const double Body, const double Poise, const bool HasStatus,
		const double StatusMagnitude, const double Epsilon)
	{
		if (!std::isfinite(Body)) { return EPacket::None; }
		if (Body > Epsilon) { return EPacket::Body; }
		if (std::isfinite(Poise) && Poise > Epsilon) { return EPacket::Poise; }
		return HasStatus && std::isfinite(StatusMagnitude) && StatusMagnitude > Epsilon
			? EPacket::Control : EPacket::None;
	}

	inline bool AcceptStatusRequest(const bool HasRequest, const bool PerfectDefense,
		const bool Guarded, const bool ControlOnly, const double AppliedDamage, const double Epsilon)
	{
		return HasRequest && !PerfectDefense
			&& (AppliedDamage > Epsilon || (ControlOnly && !Guarded));
	}
}
