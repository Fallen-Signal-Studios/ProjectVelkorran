// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

/**
 * What the player is allowed to know about nearby hostiles.
 *
 * This is gameplay, not decoration: a radar that sees through walls forever is a different game
 * from one that remembers where something was for a moment. Those rules live here, engine-free and
 * covered by portable tests, so they can be tuned in one place and argued about honestly.
 *
 * The rules, in order of importance:
 *
 * 1. A hostile that has never been seen is never shown. Detection requires line of sight at least
 *    once; there is no wallhack, and nothing appears merely for being close.
 * 2. A hostile that breaks line of sight fades from memory over a few seconds rather than vanishing
 *    instantly or persisting forever. What is displayed after that is a memory, not a sighting.
 * 3. A brief glimpse does not create a contact. A hostile must be visible for a short dwell before
 *    it registers, so the display does not flicker with every doorway and corner.
 * 4. Leaving the radius is treated exactly like losing sight: the contact fades, it does not blink out.
 */
namespace SovProximityDetectionPolicy
{
/** How far the sweep reaches, in centimetres. */
inline constexpr float DetectionRadius = 3500.f;
/** How long a contact survives after its last sighting. */
inline constexpr float MemorySeconds = 4.f;
/** Continuous visibility required before a contact registers at all. */
inline constexpr float AcquireSeconds = .2f;

inline constexpr bool IsFinite(float Value) { return Value == Value && Value <= 3.4e38f && Value >= -3.4e38f; }

inline constexpr float Clamp01(float Value) { return Value < 0.f ? 0.f : (Value > 1.f ? 1.f : Value); }

/** A new contact needs both line of sight and range. Neither alone is enough. */
inline bool AdmitsNewContact(bool bHasLineOfSight, float Distance)
{
	if (!bHasLineOfSight || !IsFinite(Distance) || Distance < 0.f) { return false; }
	return Distance <= DetectionRadius;
}

/** A glimpse is not a contact: visibility has to hold for the acquire dwell. */
inline bool HasAcquired(float ContinuouslyVisibleSeconds)
{
	if (!IsFinite(ContinuouslyVisibleSeconds) || ContinuouslyVisibleSeconds < 0.f) { return false; }
	return ContinuouslyVisibleSeconds >= AcquireSeconds;
}

/**
 * Opacity for a contact, by how long since it was last actually seen. Full while it is in sight,
 * then a linear fade to nothing across the memory window.
 */
inline float ContactAlpha(float SecondsSinceSeen)
{
	if (!IsFinite(SecondsSinceSeen)) { return 0.f; }
	if (SecondsSinceSeen <= 0.f) { return 1.f; }
	if (MemorySeconds <= 0.f) { return 0.f; }
	return Clamp01(1.f - SecondsSinceSeen / MemorySeconds);
}

/** True while a contact is still worth drawing at all. */
inline bool ShouldRetain(float SecondsSinceSeen)
{
	return IsFinite(SecondsSinceSeen) && SecondsSinceSeen < MemorySeconds;
}

/** True only while the contact is a live sighting rather than a memory. */
inline bool IsLiveSighting(float SecondsSinceSeen)
{
	return IsFinite(SecondsSinceSeen) && SecondsSinceSeen <= 0.f;
}

/** Signed bearing from the player's facing to a target, in degrees, normalised to (-180, 180]. */
inline float RelativeBearingDegrees(float FacingYawDegrees, float TargetYawDegrees)
{
	if (!IsFinite(FacingYawDegrees) || !IsFinite(TargetYawDegrees)) { return 0.f; }
	float Delta = TargetYawDegrees - FacingYawDegrees;
	// Plain arithmetic rather than a modulo, so the result stays exact for ordinary angles.
	while (Delta > 180.f) { Delta -= 360.f; }
	while (Delta <= -180.f) { Delta += 360.f; }
	return Delta;
}

/** Distance as a 0..1 fraction of the sweep, for placing a blip on the display. */
inline float NormalisedRange(float Distance)
{
	if (!IsFinite(Distance) || Distance <= 0.f) { return 0.f; }
	if (DetectionRadius <= 0.f) { return 0.f; }
	return Clamp01(Distance / DetectionRadius);
}
}
