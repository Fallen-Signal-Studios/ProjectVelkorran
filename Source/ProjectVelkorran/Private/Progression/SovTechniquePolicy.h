// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
namespace SovTechniquePolicy
{
inline bool ValidBudget(int Budget) { return Budget >= 18 && Budget <= 22; }
inline bool ValidLedger(int Earned, int Available, int Spent, int Budget)
{
	return ValidBudget(Budget) && Earned >= 0 && Earned <= Budget
		&& Available >= 0 && Spent >= 0 && Available <= Earned
		&& Spent <= Earned && Available == Earned - Spent;
}
/**
 * Points one purchased rank accounts for. PerkLevel is zero-based, so a first rank counts as one.
 *
 * The same weight is both what a rank costs and what it contributes toward a branch's tier gate,
 * which is why spend accounting, investment gating and the branch-level rebuild all share it.
 * Before this existed the rule was written out five times across those three concerns, and a rule
 * spelled out in five places is a rule that drifts.
 *
 * A negative level is not a purchase and contributes nothing. A failed grant sets the level to -1,
 * so negative levels do occur; contributing zero keeps a corrupt level from reducing some other
 * perk's investment, which a bare PerkLevel + 1 would do for levels below -1.
 */
inline int RankWeight(int PerkLevel) { return PerkLevel >= 0 ? PerkLevel + 1 : 0; }

/**
 * Tier/capstone gate. A perk never counts toward its own requirement, so callers exclude it while
 * accumulating; this compares the total only.
 */
inline bool MeetsBranchInvestment(int Investment, int Required) { return Investment >= Required; }

inline bool CanClaim(int Earned, int Available, int Points, int Budget, bool AlreadyClaimed)
{
	return !AlreadyClaimed && ValidBudget(Budget) && Earned >= 0 && Earned <= Budget
		&& Available >= 0 && Available <= Earned && Points >= 1 && Points <= 5
		&& Points <= Budget - Earned;
}
}
