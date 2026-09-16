// Copyright Fallen Signal Studios. All Rights Reserved.
// Portable coverage for what the radar is allowed to show the player.
#include "AI/SovProximityDetectionPolicy.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{
namespace Policy = SovProximityDetectionPolicy;

void NothingIsShownWithoutHavingBeenSeen()
{
	// Proximity alone never creates a contact: there is no seeing through walls.
	assert(!Policy::AdmitsNewContact(false, 100.f));
	assert(!Policy::AdmitsNewContact(false, 0.f));
	// Sight alone is not enough either; it still has to be within the sweep.
	assert(!Policy::AdmitsNewContact(true, Policy::DetectionRadius + 1.f));
	assert(Policy::AdmitsNewContact(true, Policy::DetectionRadius));
	assert(Policy::AdmitsNewContact(true, 0.f));
	// Unreadable distances are refused rather than admitted by accident.
	assert(!Policy::AdmitsNewContact(true, std::numeric_limits<float>::quiet_NaN()));
	assert(!Policy::AdmitsNewContact(true, -50.f));
}

void AGlimpseIsNotAContact()
{
	assert(!Policy::HasAcquired(0.f));
	assert(!Policy::HasAcquired(Policy::AcquireSeconds * .5f));
	assert(Policy::HasAcquired(Policy::AcquireSeconds));
	assert(Policy::HasAcquired(Policy::AcquireSeconds * 4.f));
	assert(!Policy::HasAcquired(std::numeric_limits<float>::quiet_NaN()));
	assert(!Policy::HasAcquired(-1.f));
}

void SightingsFadeIntoMemoryRatherThanVanishing()
{
	// In sight: full strength.
	assert(Policy::ContactAlpha(0.f) == 1.f);
	assert(Policy::IsLiveSighting(0.f));
	// Out of sight: a decaying memory, not an instant disappearance and not a permanent mark.
	const float Half = Policy::ContactAlpha(Policy::MemorySeconds * .5f);
	assert(Half > .4f && Half < .6f);
	assert(!Policy::IsLiveSighting(Policy::MemorySeconds * .5f));
	assert(Policy::ShouldRetain(Policy::MemorySeconds * .5f));
	// Past the memory window it is gone entirely.
	assert(Policy::ContactAlpha(Policy::MemorySeconds) == 0.f);
	assert(!Policy::ShouldRetain(Policy::MemorySeconds));
	assert(!Policy::ShouldRetain(Policy::MemorySeconds * 2.f));
	// Memory strictly decays: later is never brighter than earlier.
	float Previous = 1.f;
	for (float Seconds = 0.f; Seconds <= Policy::MemorySeconds; Seconds += .25f)
	{
		const float Alpha = Policy::ContactAlpha(Seconds);
		assert(Alpha <= Previous + .0001f);
		Previous = Alpha;
	}
	// An unreadable age shows nothing rather than a stuck blip.
	assert(Policy::ContactAlpha(std::numeric_limits<float>::quiet_NaN()) == 0.f);
	assert(!Policy::ShouldRetain(std::numeric_limits<float>::quiet_NaN()));
}

void BearingIsSignedAndWrapsCorrectly()
{
	assert(std::fabs(Policy::RelativeBearingDegrees(0.f, 0.f)) < .001f);
	assert(std::fabs(Policy::RelativeBearingDegrees(0.f, 90.f) - 90.f) < .001f);
	assert(std::fabs(Policy::RelativeBearingDegrees(0.f, -90.f) + 90.f) < .001f);
	// Wrapping across the seam: a target just clockwise of due aft reads as nearly -180, not +180.
	assert(std::fabs(Policy::RelativeBearingDegrees(0.f, 350.f) + 10.f) < .001f);
	assert(std::fabs(Policy::RelativeBearingDegrees(350.f, 0.f) - 10.f) < .001f);
	// Facing rotates the whole frame: the same world angle reads differently as the player turns.
	assert(std::fabs(Policy::RelativeBearingDegrees(90.f, 90.f)) < .001f);
	assert(std::fabs(Policy::RelativeBearingDegrees(90.f, 180.f) - 90.f) < .001f);
	// Always inside the half-open range, for any input.
	for (float Facing = -720.f; Facing <= 720.f; Facing += 37.f)
	{
		for (float Target = -720.f; Target <= 720.f; Target += 53.f)
		{
			const float Bearing = Policy::RelativeBearingDegrees(Facing, Target);
			assert(Bearing > -180.f - .001f && Bearing <= 180.f + .001f);
		}
	}
	assert(Policy::RelativeBearingDegrees(std::numeric_limits<float>::quiet_NaN(), 10.f) == 0.f);
}

void RangePlacementStaysOnTheDisplay()
{
	assert(Policy::NormalisedRange(0.f) == 0.f);
	assert(std::fabs(Policy::NormalisedRange(Policy::DetectionRadius) - 1.f) < .001f);
	assert(std::fabs(Policy::NormalisedRange(Policy::DetectionRadius * .5f) - .5f) < .001f);
	// Anything beyond the sweep pins to the rim rather than drawing outside the display.
	assert(Policy::NormalisedRange(Policy::DetectionRadius * 10.f) == 1.f);
	assert(Policy::NormalisedRange(-100.f) == 0.f);
	assert(Policy::NormalisedRange(std::numeric_limits<float>::quiet_NaN()) == 0.f);
}

void TheSweepIsAReadableSize()
{
	// Sanity on the tuning itself: a radar with no reach, or one that remembers forever, is a bug.
	assert(Policy::DetectionRadius > 500.f);
	assert(Policy::MemorySeconds > 0.f && Policy::MemorySeconds < 30.f);
	assert(Policy::AcquireSeconds >= 0.f && Policy::AcquireSeconds < Policy::MemorySeconds);
}
}

int main()
{
	NothingIsShownWithoutHavingBeenSeen();
	AGlimpseIsNotAContact();
	SightingsFadeIntoMemoryRatherThanVanishing();
	BearingIsSignedAndWrapsCorrectly();
	RangePlacementStaysOnTheDisplay();
	TheSweepIsAReadableSize();
	return 0;
}
