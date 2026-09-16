// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

/**
 * Aurelion Elite boss scaling and phase thresholds.
 *
 * Engine-free on purpose: these are the numbers that decide whether the fight reads as a late-game
 * boss, so they are covered by the portable policy tests rather than only by a play session.
 *
 * Measured baseline, 15 September 2026: the Elite entered its own boss encounter with 53.2 health,
 * the same pool as the Weaver and WallRunner standing beside it, and died 16-36 seconds into E4B
 * across four retained route runs. Scaling is expressed against that measurement, not invented.
 *
 * The same actor fights both phases: the link director freezes it and transfers it into the thermal
 * encounter. A single static pool therefore cannot be both values, so the boss re-arms at the phase
 * boundary. That is a deliberate design beat - phase A is the link set piece, phase B is the real
 * fight - and it is recorded here rather than buried in a constructor.
 */
namespace SovAurelionElitePolicy
{
/** Observed pre-pass durability. */
inline constexpr float BaselineHealth = 53.2f;
inline constexpr float LinkPhaseMultiplier = 4.f;
inline constexpr float CruciblePhaseMultiplier = 10.f;
inline constexpr float LinkPhaseHealth = BaselineHealth * LinkPhaseMultiplier;
inline constexpr float CruciblePhaseHealth = BaselineHealth * CruciblePhaseMultiplier;
/** Additive armour, alongside the Weaver ward's existing +15 while its links survive. */
inline constexpr float LinkPhaseArmor = 25.f;
inline constexpr float CruciblePhaseArmor = 40.f;

enum class EPhase : unsigned char { First = 0, Second = 1, Third = 2, Enrage = 3 };

/** Remaining-health fractions that open each later phase of the crucible fight. */
inline constexpr float SecondPhaseFraction = .66f;
inline constexpr float ThirdPhaseFraction = .33f;
inline constexpr float EnrageFraction = .15f;

inline constexpr bool IsFinite(float Value) { return Value == Value && Value <= 3.4e38f && Value >= -3.4e38f; }

/**
 * Phases never walk backwards. Healing, a restored checkpoint reporting a stale fraction, or a
 * transient attribute read must not return an enraged boss to its opening behaviour mid-attempt;
 * a fresh attempt constructs its own state at First.
 */
inline EPhase PhaseForHealthFraction(float Fraction, EPhase Current = EPhase::First)
{
	if (!IsFinite(Fraction)) { return Current; }
	const float Clamped = Fraction < 0.f ? 0.f : (Fraction > 1.f ? 1.f : Fraction);
	EPhase Reached = EPhase::First;
	if (Clamped <= EnrageFraction) { Reached = EPhase::Enrage; }
	else if (Clamped <= ThirdPhaseFraction) { Reached = EPhase::Third; }
	else if (Clamped <= SecondPhaseFraction) { Reached = EPhase::Second; }
	return static_cast<unsigned char>(Reached) > static_cast<unsigned char>(Current) ? Reached : Current;
}

/** Multiplies the interval between the boss's attacks: later phases press harder. */
inline float AttackIntervalScale(EPhase Phase)
{
	switch (Phase)
	{
	case EPhase::Second: return .85f;
	case EPhase::Third: return .7f;
	case EPhase::Enrage: return .5f;
	default: return 1.f;
	}
}

/**
 * Additive damage resistance per phase. The enrage deliberately drops below the third phase: the
 * boss trades protection for pressure, so an enraged boss is dangerous rather than merely spongy.
 */
inline float DamageResistance(EPhase Phase)
{
	switch (Phase)
	{
	case EPhase::Second: return .10f;
	case EPhase::Third: return .20f;
	case EPhase::Enrage: return .10f;
	default: return 0.f;
	}
}

/** Adds summoned during a phase. None in the opening phase: the first minute stays readable. */
inline int SummonCountForPhase(EPhase Phase)
{
	switch (Phase)
	{
	case EPhase::Second: return 2;
	case EPhase::Third: return 3;
	case EPhase::Enrage: return 3;
	default: return 0;
	}
}
}
