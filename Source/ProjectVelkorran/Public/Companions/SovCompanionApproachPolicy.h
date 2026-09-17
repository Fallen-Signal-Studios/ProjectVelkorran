// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

/** Pure approach geometry shared with the command tick and its tests.
 *
 * A commanded companion used to approach a focus only when that focus was already within ten metres
 * of the leader and already in the companion's line of sight, which meant an ordered target behind a
 * crate fifteen metres ahead was accepted and then never walked to. Line of sight is what walking
 * earns, so it is not a precondition here; the leash is applied to the approach point instead, which
 * is what actually decides how far the companion may be drawn from the leader. */
namespace SovCompanionApproachPolicy
{
/** Distance from the leader an ordered focus may pull the companion, matching command admission. */
inline constexpr float OrderedLeash = 2500.f;
/** An unordered focus the companion picked up itself keeps the existing defense area. */
inline constexpr float UnorderedLeash = 1000.f;

/** Stand-off distance for an attack whose ranges are known, biased toward the preferred range. */
inline float ApproachRange(float MinimumRange, float PreferredRange, float MaximumRange)
{
	if (!FMath::IsFinite(MinimumRange) || !FMath::IsFinite(MaximumRange) || MaximumRange <= 0.f) { return 0.f; }
	const float Low = FMath::Max(0.f, FMath::Min(MinimumRange, MaximumRange));
	const float High = FMath::Max(Low, MaximumRange);
	const float Wanted = FMath::IsFinite(PreferredRange) ? PreferredRange : Low;
	return FMath::Clamp(Wanted, Low, FMath::Lerp(Low, High, .75f));
}

/** True when the companion should move, with OutPoint the place to stand.
 * Refuses when the attack already reaches, when the geometry is not finite, or when standing there
 * would put the companion further from the leader than the order admits. */
inline bool SelectApproachPoint(const FVector& Companion, const FVector& Focus, const FVector& Leader,
	float MinimumRange, float PreferredRange, float MaximumRange, float Leash, FVector& OutPoint)
{
	OutPoint = FVector::ZeroVector;
	if (Companion.ContainsNaN() || Focus.ContainsNaN() || Leader.ContainsNaN()
		|| !FMath::IsFinite(MaximumRange) || MaximumRange <= 0.f || !FMath::IsFinite(Leash) || Leash <= 0.f) { return false; }
	if (FVector::Dist(Companion, Focus) <= MaximumRange) { return false; }
	FVector TowardCompanion = (Companion - Focus).GetSafeNormal2D();
	// Directly overhead or underneath: any horizontal stand-off is as good as another.
	if (TowardCompanion.IsNearlyZero()) { TowardCompanion = FVector::ForwardVector; }
	const FVector Point = Focus + TowardCompanion * ApproachRange(MinimumRange, PreferredRange, MaximumRange);
	if (FVector::DistSquared(Point, Leader) > FMath::Square(Leash)) { return false; }
	OutPoint = Point;
	return true;
}
}
