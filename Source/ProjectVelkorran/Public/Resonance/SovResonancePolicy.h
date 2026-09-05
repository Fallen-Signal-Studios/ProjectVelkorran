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
inline bool MayRecoverSeparation(float SeparationSquared, float AnchorDistanceSquared, bool SplitPhase, bool ValidAnchor)
{
	return std::isfinite(SeparationSquared) && std::isfinite(AnchorDistanceSquared) && !SplitPhase && ValidAnchor
		&& SeparationSquared >= 2500.f * 2500.f && AnchorDistanceSquared >= 0.f && AnchorDistanceSquared <= 2000.f * 2000.f;
}
}
