// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

/**
 * Aim-down-sights feel: how fast the sight comes up, how far it zooms, and when the sight picture
 * takes over the screen.
 *
 * Engine-free on purpose. These are the numbers a player actually feels, so they are covered by the
 * portable policy tests rather than only by eye, and they live in one place instead of being spread
 * across a component, a camera manager and a widget.
 *
 * Why per-weapon values live here rather than on the weapon assets: WI_Staccato is not tracked by
 * version control on this project, so a value authored onto it exists on one machine only. Cinderline
 * is tracked, but splitting the pair across two sources would be worse than keeping both in code.
 */
namespace SovAdsPolicy
{
enum class ESight : unsigned char { None, IronSight, Scope };

struct FProfile
{
	/** Field of view at full aim, as a fraction of the player's own setting. Smaller zooms further. */
	float ZoomFraction = 1.f;
	float RaiseSeconds = .18f;
	float LowerSeconds = .12f;
	ESight Sight = ESight::None;
	/**
	 * Blend at which the sight picture reaches full strength. Above 1 means it never does: iron
	 * sights frame the shot without ever taking the screen, which a scope is supposed to do.
	 */
	float SightFullAt = 1.1f;
	/** Look sensitivity multiplier at full aim, so a zoomed sight is steady rather than twitchy. */
	float AimSensitivity = 1.f;
};

/** A rifle: quick to raise, modest magnification, sights that frame rather than obscure. */
inline constexpr FProfile Cinderline{ .62f, .16f, .11f, ESight::IronSight, 1.1f, .8f };
/** A marksman weapon: slower to settle, real magnification, and a scope that owns the screen. */
inline constexpr FProfile Staccato{ .30f, .26f, .15f, ESight::Scope, .85f, .45f };
/** Anything else that can aim still gets a readable, unmagnified sight rather than nothing. */
inline constexpr FProfile Default{ .8f, .16f, .11f, ESight::IronSight, 1.1f, .9f };

inline constexpr bool IsFinite(float Value) { return Value == Value && Value <= 3.4e38f && Value >= -3.4e38f; }

inline constexpr float Clamp01(float Value) { return Value < 0.f ? 0.f : (Value > 1.f ? 1.f : Value); }

/**
 * Advances the raise/lower blend. Returns the new blend in 0..1.
 *
 * A dropped or absurd frame time must not snap the sight: an unreadable delta holds the current
 * blend rather than completing it, which keeps a hitch from teleporting the player into a scope.
 */
inline float AdvanceBlend(float Current, bool bAiming, float DeltaSeconds, const FProfile& Profile)
{
	const float Start = IsFinite(Current) ? Clamp01(Current) : 0.f;
	if (!IsFinite(DeltaSeconds) || DeltaSeconds < 0.f) { return Start; }
	const float Duration = bAiming ? Profile.RaiseSeconds : Profile.LowerSeconds;
	// A non-positive duration is an authored instant transition, not a divide by zero.
	if (!IsFinite(Duration) || Duration <= 0.f) { return bAiming ? 1.f : 0.f; }
	const float Step = DeltaSeconds / Duration;
	return Clamp01(bAiming ? Start + Step : Start - Step);
}

/** The field of view for a blend, from the player's own base setting down to the weapon's zoom. */
inline float FieldOfView(float BaseFieldOfView, float Blend, const FProfile& Profile)
{
	if (!IsFinite(BaseFieldOfView) || BaseFieldOfView <= 0.f) { return BaseFieldOfView; }
	const float Alpha = IsFinite(Blend) ? Clamp01(Blend) : 0.f;
	const float Zoom = IsFinite(Profile.ZoomFraction) && Profile.ZoomFraction > 0.f ? Profile.ZoomFraction : 1.f;
	return BaseFieldOfView * (1.f - Alpha) + BaseFieldOfView * Zoom * Alpha;
}

/** Opacity of the sight overlay for a blend. Zero while hip firing, so nothing is drawn at rest. */
inline float SightAlpha(float Blend, const FProfile& Profile)
{
	const float Alpha = IsFinite(Blend) ? Clamp01(Blend) : 0.f;
	const float Full = IsFinite(Profile.SightFullAt) && Profile.SightFullAt > 0.f ? Profile.SightFullAt : 1.f;
	return Clamp01(Alpha / Full);
}

/** Look sensitivity for a blend, easing toward the profile's aimed value as the sight comes up. */
inline float Sensitivity(float Blend, const FProfile& Profile)
{
	const float Alpha = IsFinite(Blend) ? Clamp01(Blend) : 0.f;
	const float Aimed = IsFinite(Profile.AimSensitivity) && Profile.AimSensitivity > 0.f ? Profile.AimSensitivity : 1.f;
	return 1.f * (1.f - Alpha) + Aimed * Alpha;
}

/** True once the sight picture should replace the hip crosshair. */
inline bool ReplacesCrosshair(float Blend, const FProfile& Profile)
{
	return Profile.Sight != ESight::None && SightAlpha(Blend, Profile) > .5f;
}
}
