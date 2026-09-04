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
inline bool CanClaim(int Earned, int Available, int Points, int Budget, bool AlreadyClaimed)
{
	return !AlreadyClaimed && ValidBudget(Budget) && Earned >= 0 && Earned <= Budget
		&& Available >= 0 && Available <= Earned && Points >= 1 && Points <= 5
		&& Points <= Budget - Earned;
}
}
