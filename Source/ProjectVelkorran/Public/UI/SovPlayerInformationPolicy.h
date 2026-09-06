// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cstddef>

/** Shared, engine-independent admission policy used by the native presentation. */
namespace SovPlayerInformationPolicy
{
	// Equal-priority events wait their turn. Critical pages always finish their readable interval.
	inline bool CanPreempt(int ActivePriority, int IncomingPriority, bool HasActive)
	{ return !HasActive || (ActivePriority < 2 && IncomingPriority > ActivePriority); }

	// Advance even for irrelevant actors, so a dense early spawn cohort cannot hide later weak points.
	inline std::size_t TakeNext(std::size_t Count, std::size_t& Cursor)
	{
		if (!Count) { Cursor = 0; return 0; }
		const std::size_t Result = Cursor % Count;
		Cursor = (Result + 1) % Count;
		return Result;
	}
}
