// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstdint>

/** Pure authority policy shared with the native handshake and portable tests. */
namespace SovResonancePolicy
{
inline bool Complementary(bool PlayerTarrik, bool PlayerSelene, bool PartnerTarrik, bool PartnerSelene)
{ return PlayerTarrik != PlayerSelene && PartnerTarrik != PartnerSelene && PlayerTarrik != PartnerTarrik; }
inline bool OfferLive(double Now, double Deadline, bool SameMission, bool BothAlive, bool TargetValid, bool SeparateASCs)
{ return std::isfinite(Now) && std::isfinite(Deadline) && Now < Deadline && SameMission && BothAlive && TargetValid && SeparateASCs; }
inline float AddPressure(float Current, float Accepted, float Cap)
{
	if (!std::isfinite(Current) || !std::isfinite(Accepted) || !std::isfinite(Cap) || Current < 0 || Accepted <= 0 || Cap <= 0) { return Current; }
	return Current + Accepted > Cap ? Cap : Current + Accepted;
}
inline bool WithinContributionBudget(float CompanionDamage, float PlayerDamage, float Fraction)
{ return std::isfinite(CompanionDamage) && std::isfinite(PlayerDamage) && std::isfinite(Fraction) && CompanionDamage >= 0 && PlayerDamage > 0 && Fraction > 0 && Fraction <= .25f && CompanionDamage < PlayerDamage * Fraction / (1.f - Fraction); }
inline float RemainingContribution(float CompanionDamage, float PlayerDamage, float Fraction)
{
	if (!WithinContributionBudget(CompanionDamage, PlayerDamage, Fraction)) { return 0.f; }
	return PlayerDamage * Fraction / (1.f - Fraction) - CompanionDamage;
}
/**
 * Whether the companion is inside the opening stretch of an encounter scope.
 *
 * Contribution is budgeted from the player's damage, so before the player's first hit the budget is
 * zero and the companion attacks nothing - including every encounter's opening, and any stretch spent
 * guarding or evading. §12.2 asks it to contribute visibly, so for a bounded opening it fights without
 * a budget. Its damage still accrues, so the ordinary cap takes over as soon as the window closes.
 */
inline bool WithinOpeningContribution(double Now, double ScopeOpenedAt, float WindowSeconds)
{
	return std::isfinite(Now) && std::isfinite(ScopeOpenedAt) && std::isfinite(WindowSeconds)
		&& WindowSeconds > 0. && Now >= ScopeOpenedAt && Now - ScopeOpenedAt < WindowSeconds;
}

/**
 * Whether the companion may commit to a new attack.
 *
 * Refuses rather than clamps: an attack whose budget cannot cover a meaningful amount used to be
 * selected anyway, animate in full, and land for a clamped or zero amount with no feedback. Declining
 * to start it reads as the companion choosing its moment instead of swinging at nothing.
 */
inline bool MayCommitAttack(bool InOpening, float CompanionDamage, float PlayerDamage, float Fraction,
	float MinimumMeaningful)
{
	if (InOpening) { return true; }
	if (!std::isfinite(MinimumMeaningful) || MinimumMeaningful < 0.f) { return false; }
	return RemainingContribution(CompanionDamage, PlayerDamage, Fraction) >= MinimumMeaningful;
}

inline bool MayRecoverSeparation(float SeparationSquared, float AnchorDistanceSquared, bool SplitPhase, bool ValidAnchor)
{
	return std::isfinite(SeparationSquared) && std::isfinite(AnchorDistanceSquared) && !SplitPhase && ValidAnchor
		&& SeparationSquared >= 2500.f * 2500.f && AnchorDistanceSquared >= 0.f && AnchorDistanceSquared <= 2000.f * 2000.f;
}
}
