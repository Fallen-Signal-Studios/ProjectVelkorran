// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <limits>

namespace SovCombatTransaction
{
	/** One synchronous callback tree gets both a stack-depth and a total-work cap.
	 * Separate top-level hits start fresh; nested admission and resolution share it. */
	struct FCallbackBudget
	{
		unsigned Depth = 0;
		unsigned Remaining = 0;
	};

	inline FCallbackBudget& ThreadCallbackBudget()
	{
		static thread_local FCallbackBudget Budget;
		return Budget;
	}

	class FScopedCallbackBudget
	{
	public:
		explicit FScopedCallbackBudget(FCallbackBudget& InBudget) : Budget(InBudget)
		{
			if (Budget.Depth == 0) { Budget.Remaining = 128; }
			if (Budget.Depth < 32 && Budget.Remaining > 0)
			{
				++Budget.Depth;
				--Budget.Remaining;
				bAdmitted = true;
			}
		}
		~FScopedCallbackBudget() { if (bAdmitted) { --Budget.Depth; } }
		FScopedCallbackBudget(const FScopedCallbackBudget&) = delete;
		FScopedCallbackBudget& operator=(const FScopedCallbackBudget&) = delete;
		bool IsAdmitted() const { return bAdmitted; }
	private:
		FCallbackBudget& Budget;
		bool bAdmitted = false;
	};

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
