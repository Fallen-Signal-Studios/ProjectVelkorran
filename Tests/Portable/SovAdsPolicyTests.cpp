// Copyright Fallen Signal Studios. All Rights Reserved.
// Portable coverage for aim-down-sights feel: raise/lower blending, zoom, sight and sensitivity.
#include "Weapons/SovAdsPolicy.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{
using SovAdsPolicy::ESight;
using SovAdsPolicy::FProfile;

void TheTwoWeaponsFeelDifferent()
{
	// A marksman weapon magnifies further and takes longer to settle than a rifle.
	assert(SovAdsPolicy::Staccato.ZoomFraction < SovAdsPolicy::Cinderline.ZoomFraction);
	assert(SovAdsPolicy::Staccato.RaiseSeconds > SovAdsPolicy::Cinderline.RaiseSeconds);
	// Only the marksman weapon's sight picture takes over the screen.
	assert(SovAdsPolicy::Staccato.Sight == ESight::Scope);
	assert(SovAdsPolicy::Cinderline.Sight == ESight::IronSight);
	assert(SovAdsPolicy::Staccato.SightFullAt <= 1.f);
	assert(SovAdsPolicy::Cinderline.SightFullAt > 1.f);
	// A steadier weapon is less twitchy while aimed.
	assert(SovAdsPolicy::Staccato.AimSensitivity < SovAdsPolicy::Cinderline.AimSensitivity);
	// Lowering is never slower than raising: dropping the sight must not trap the player.
	for (const FProfile& Profile : {SovAdsPolicy::Cinderline, SovAdsPolicy::Staccato, SovAdsPolicy::Default})
	{
		assert(Profile.LowerSeconds <= Profile.RaiseSeconds);
		assert(Profile.ZoomFraction > 0.f && Profile.ZoomFraction <= 1.f);
	}
}

void TheSightRaisesAndLowersInItsOwnTime()
{
	const FProfile& Rifle = SovAdsPolicy::Cinderline;
	float Blend = 0.f;
	Blend = SovAdsPolicy::AdvanceBlend(Blend, true, Rifle.RaiseSeconds * .5f, Rifle);
	assert(Blend > .4f && Blend < .6f);
	Blend = SovAdsPolicy::AdvanceBlend(Blend, true, Rifle.RaiseSeconds, Rifle);
	assert(Blend == 1.f); // Clamped, never overshooting.
	Blend = SovAdsPolicy::AdvanceBlend(Blend, false, Rifle.LowerSeconds * .5f, Rifle);
	assert(Blend > .4f && Blend < .6f);
	Blend = SovAdsPolicy::AdvanceBlend(Blend, false, Rifle.LowerSeconds, Rifle);
	assert(Blend == 0.f);
}

void AHitchNeverSnapsTheSight()
{
	const FProfile& Scope = SovAdsPolicy::Staccato;
	const float NaNValue = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	assert(SovAdsPolicy::AdvanceBlend(.5f, true, NaNValue, Scope) == .5f);
	assert(SovAdsPolicy::AdvanceBlend(.5f, true, Infinity, Scope) == .5f);
	assert(SovAdsPolicy::AdvanceBlend(.5f, true, -1.f, Scope) == .5f);
	// An unreadable current blend restarts from the hip rather than from nonsense.
	assert(SovAdsPolicy::AdvanceBlend(NaNValue, false, .016f, Scope) == 0.f);
	// A long frame still completes rather than overshooting into an invalid blend.
	const float Long = SovAdsPolicy::AdvanceBlend(0.f, true, 10.f, Scope);
	assert(Long == 1.f);
}

void ZoomFollowsTheBlendFromThePlayersOwnSetting()
{
	const FProfile& Scope = SovAdsPolicy::Staccato;
	// At rest the player's chosen field of view is untouched: aiming is the only thing that zooms.
	assert(std::fabs(SovAdsPolicy::FieldOfView(90.f, 0.f, Scope) - 90.f) < .001f);
	assert(std::fabs(SovAdsPolicy::FieldOfView(90.f, 1.f, Scope) - 90.f * Scope.ZoomFraction) < .001f);
	const float Half = SovAdsPolicy::FieldOfView(90.f, .5f, Scope);
	assert(Half < 90.f && Half > 90.f * Scope.ZoomFraction);
	// A different base setting still zooms by the same proportion, so the setting is respected.
	assert(std::fabs(SovAdsPolicy::FieldOfView(70.f, 1.f, Scope) - 70.f * Scope.ZoomFraction) < .001f);
	// Nonsense in, same value out: never a zero or negative field of view.
	assert(SovAdsPolicy::FieldOfView(-5.f, 1.f, Scope) == -5.f);
}

void TheSightPictureOnlyAppearsWhileAiming()
{
	assert(SovAdsPolicy::SightAlpha(0.f, SovAdsPolicy::Staccato) == 0.f);
	assert(SovAdsPolicy::SightAlpha(0.f, SovAdsPolicy::Cinderline) == 0.f);
	// The scope reaches full strength before the blend completes, so it is settled when the shot is.
	assert(SovAdsPolicy::SightAlpha(SovAdsPolicy::Staccato.SightFullAt, SovAdsPolicy::Staccato) == 1.f);
	assert(SovAdsPolicy::SightAlpha(1.f, SovAdsPolicy::Staccato) == 1.f);
	// Iron sights never fully take the screen.
	assert(SovAdsPolicy::SightAlpha(1.f, SovAdsPolicy::Cinderline) < 1.f);
	// The hip crosshair is replaced only once a real sight picture is up.
	assert(!SovAdsPolicy::ReplacesCrosshair(0.f, SovAdsPolicy::Staccato));
	assert(SovAdsPolicy::ReplacesCrosshair(1.f, SovAdsPolicy::Staccato));
	assert(SovAdsPolicy::ReplacesCrosshair(1.f, SovAdsPolicy::Default));
}

void SensitivityEasesRatherThanSnapping()
{
	const FProfile& Scope = SovAdsPolicy::Staccato;
	assert(SovAdsPolicy::Sensitivity(0.f, Scope) == 1.f);
	assert(std::fabs(SovAdsPolicy::Sensitivity(1.f, Scope) - Scope.AimSensitivity) < .001f);
	const float Half = SovAdsPolicy::Sensitivity(.5f, Scope);
	assert(Half < 1.f && Half > Scope.AimSensitivity);
	// Never zero: a zoomed sight is slower to turn, never immovable.
	for (float Blend = 0.f; Blend <= 1.f; Blend += .1f)
	{
		assert(SovAdsPolicy::Sensitivity(Blend, Scope) > 0.f);
		assert(SovAdsPolicy::Sensitivity(Blend, SovAdsPolicy::Cinderline) > 0.f);
	}
}
}

int main()
{
	TheTwoWeaponsFeelDifferent();
	TheSightRaisesAndLowersInItsOwnTime();
	AHitchNeverSnapsTheSight();
	ZoomFollowsTheBlendFromThePlayersOwnSetting();
	TheSightPictureOnlyAppearsWhileAiming();
	SensitivityEasesRatherThanSnapping();
	return 0;
}
