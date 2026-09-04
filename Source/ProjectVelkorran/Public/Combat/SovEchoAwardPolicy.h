// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

namespace SovEchoAwardPolicy
{
	inline bool IsMeterReady(float Current, float Requirement)
	{
		return std::isfinite(Current) && std::isfinite(Requirement)
			&& Requirement >= 0.f && Current + 0.0001f >= Requirement;
	}

	/** An inactivity-bounded distinct-target chain. Repeated targets do not pay extra links. */
	template <typename TargetKey> struct TPrecisionChain
	{
		std::vector<TargetKey> Targets;
		double LastHit = -1.0e30;
		int BonusLinks = 0;
		void Reset() { Targets.clear(); LastHit = -1.0e30; BonusLinks = 0; }
		bool Advance(const TargetKey& Target, double Now, double Window, int MaximumBonusLinks)
		{
			if (!std::isfinite(Now) || !std::isfinite(Window) || Window <= 0.0) { Reset(); return false; }
			if (Now < LastHit || Now - LastHit > Window) Reset();
			const bool Distinct = std::find(Targets.begin(), Targets.end(), Target) == Targets.end();
			const bool Bonus = Distinct && !Targets.empty() && BonusLinks < std::max(MaximumBonusLinks, 0);
			// Once capped, stop adding identities; expiry/reset is required before any further payout.
			if (Distinct && (Targets.empty() || BonusLinks < std::max(MaximumBonusLinks, 0))) Targets.push_back(Target);
			LastHit = Now;
			if (Bonus) ++BonusLinks;
			return Bonus;
		}
	};
}
