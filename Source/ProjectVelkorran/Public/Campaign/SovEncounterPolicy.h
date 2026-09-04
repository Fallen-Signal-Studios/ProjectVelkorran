// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>

/** Small policy shared by native gameplay and portable regression tests. */
namespace SovEncounterPolicy
{
	inline bool ValidResource(float Current, float Maximum)
	{
		return std::isfinite(Current) && std::isfinite(Maximum)
			&& Current >= 0.f && Maximum >= 0.f && Current <= Maximum + 0.01f;
	}
	inline float ClampRestoredResource(float SavedCurrent, float CurrentMaximum)
	{
		if (!std::isfinite(SavedCurrent) || !std::isfinite(CurrentMaximum) || CurrentMaximum <= 0.f) { return 0.f; }
		return SavedCurrent < 0.f ? 0.f : (SavedCurrent > CurrentMaximum ? CurrentMaximum : SavedCurrent);
	}
	// Mirrors ESovEncounterState without coupling portable tests to Unreal headers.
	constexpr bool CanBegin(unsigned State) { return State == 0u; }
	constexpr bool CanResolve(unsigned State) { return State == 1u; }
	constexpr bool CanRetry(unsigned State, bool HasSnapshot) { return HasSnapshot && (State == 1u || State == 3u); }
	constexpr bool CanClaimCompletionReward(unsigned State, bool ValidId, bool AlreadyClaimed)
	{
		return State == 2u && ValidId && !AlreadyClaimed;
	}
	constexpr unsigned StateAfterLoad(unsigned State, bool HasCheckpoint = false)
	{
		return State == 1u || State >= 4u || (State == 0u && HasCheckpoint) ? 3u : State;
	}
}
