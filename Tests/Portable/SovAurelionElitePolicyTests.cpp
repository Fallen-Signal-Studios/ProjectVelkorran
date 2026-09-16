// Copyright Fallen Signal Studios. All Rights Reserved.
// Portable coverage for the Aurelion Elite boss scaling and phase thresholds.
#include "AI/SovAurelionElitePolicy.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{
using SovAurelionElitePolicy::EPhase;

void ScalingMatchesTheMeasuredBaseline()
{
	// The measured pre-pass pool, and the agreed multipliers against it.
	assert(std::fabs(SovAurelionElitePolicy::BaselineHealth - 53.2f) < .001f);
	assert(std::fabs(SovAurelionElitePolicy::LinkPhaseHealth - 212.8f) < .01f);
	assert(std::fabs(SovAurelionElitePolicy::CruciblePhaseHealth - 532.f) < .01f);
	// The crucible boss is strictly harder than the link-phase boss on both axes.
	assert(SovAurelionElitePolicy::CruciblePhaseHealth > SovAurelionElitePolicy::LinkPhaseHealth);
	assert(SovAurelionElitePolicy::CruciblePhaseArmor > SovAurelionElitePolicy::LinkPhaseArmor);
}

void PhasesOpenAtTheirThresholds()
{
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(1.f) == EPhase::First);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.67f) == EPhase::First);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.66f) == EPhase::Second);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.34f) == EPhase::Second);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.33f) == EPhase::Third);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.16f) == EPhase::Third);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.15f) == EPhase::Enrage);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(0.f) == EPhase::Enrage);
}

void PhasesNeverWalkBackwards()
{
	// Healing, relief, or a restored fraction cannot return an enraged boss to its opening behaviour.
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(1.f, EPhase::Enrage) == EPhase::Enrage);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.9f, EPhase::Third) == EPhase::Third);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.5f, EPhase::Second) == EPhase::Second);
	// Advancing still works from any earlier phase.
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.1f, EPhase::First) == EPhase::Enrage);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(.3f, EPhase::Second) == EPhase::Third);
}

void UnreadableFractionsHoldTheCurrentPhase()
{
	const float NaNValue = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(NaNValue, EPhase::Second) == EPhase::Second);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(Infinity, EPhase::Third) == EPhase::Third);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(-Infinity, EPhase::First) == EPhase::First);
	// Out-of-range but finite values clamp rather than hold.
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(-5.f) == EPhase::Enrage);
	assert(SovAurelionElitePolicy::PhaseForHealthFraction(5.f) == EPhase::First);
}

void PressureRisesWithEachPhase()
{
	assert(SovAurelionElitePolicy::AttackIntervalScale(EPhase::First) == 1.f);
	assert(SovAurelionElitePolicy::AttackIntervalScale(EPhase::Second) < SovAurelionElitePolicy::AttackIntervalScale(EPhase::First));
	assert(SovAurelionElitePolicy::AttackIntervalScale(EPhase::Third) < SovAurelionElitePolicy::AttackIntervalScale(EPhase::Second));
	assert(SovAurelionElitePolicy::AttackIntervalScale(EPhase::Enrage) < SovAurelionElitePolicy::AttackIntervalScale(EPhase::Third));
	// Never free: an interval scale of zero would mean a boss attacking every frame.
	assert(SovAurelionElitePolicy::AttackIntervalScale(EPhase::Enrage) > 0.f);
}

void EnrageTradesProtectionForPressure()
{
	assert(SovAurelionElitePolicy::DamageResistance(EPhase::First) == 0.f);
	assert(SovAurelionElitePolicy::DamageResistance(EPhase::Third) > SovAurelionElitePolicy::DamageResistance(EPhase::Second));
	// The enrage is deliberately easier to hurt than the third phase, and faster than all of them.
	assert(SovAurelionElitePolicy::DamageResistance(EPhase::Enrage) < SovAurelionElitePolicy::DamageResistance(EPhase::Third));
	assert(SovAurelionElitePolicy::AttackIntervalScale(EPhase::Enrage) < SovAurelionElitePolicy::AttackIntervalScale(EPhase::Third));
	// Resistance never reaches immunity at any phase.
	for (const EPhase Phase : {EPhase::First, EPhase::Second, EPhase::Third, EPhase::Enrage})
	{
		assert(SovAurelionElitePolicy::DamageResistance(Phase) >= 0.f && SovAurelionElitePolicy::DamageResistance(Phase) < 1.f);
	}
}

void SummonsStayOutOfTheOpeningPhase()
{
	assert(SovAurelionElitePolicy::SummonCountForPhase(EPhase::First) == 0);
	assert(SovAurelionElitePolicy::SummonCountForPhase(EPhase::Second) > 0);
	assert(SovAurelionElitePolicy::SummonCountForPhase(EPhase::Third) >= SovAurelionElitePolicy::SummonCountForPhase(EPhase::Second));
	// Bounded: summons pressure the player, they do not bury the encounter's living cap.
	for (const EPhase Phase : {EPhase::First, EPhase::Second, EPhase::Third, EPhase::Enrage})
	{
		assert(SovAurelionElitePolicy::SummonCountForPhase(Phase) <= 3);
	}
}
}

int main()
{
	ScalingMatchesTheMeasuredBaseline();
	PhasesOpenAtTheirThresholds();
	PhasesNeverWalkBackwards();
	UnreadableFractionsHoldTheCurrentPhase();
	PressureRisesWithEachPhase();
	EnrageTradesProtectionForPressure();
	SummonsStayOutOfTheOpeningPhase();
	return 0;
}
