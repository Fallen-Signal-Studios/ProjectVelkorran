#include "Progression/SovTechniquePolicy.h"
#include <cassert>
#include <climits>
#include <iostream>
int main()
{
	using namespace SovTechniquePolicy;
	int Checks = 0;
	auto Check = [&Checks](bool Value) { assert(Value); ++Checks; };
	Check(!ValidBudget(17)); Check(ValidBudget(18)); Check(ValidBudget(20)); Check(ValidBudget(22)); Check(!ValidBudget(23));
	Check(CanClaim(0, 0, 1, 20, false)); Check(CanClaim(19, 0, 1, 20, false));
	Check(!CanClaim(20, 20, 1, 20, false)); Check(!CanClaim(19, 0, 2, 20, false));
	Check(!CanClaim(0, 0, 1, 20, true)); Check(!CanClaim(0, 0, 0, 20, false)); Check(!CanClaim(0, 0, -1, 20, false));
	Check(!CanClaim(INT_MAX, 0, 1, 20, false)); Check(!CanClaim(0, INT_MAX, 1, 20, false));
	Check(!CanClaim(0, 0, INT_MAX, 20, false)); Check(!CanClaim(INT_MIN, 0, 1, 20, false));
	Check(ValidLedger(5, 3, 2, 20)); Check(ValidLedger(5, 5, 0, 20));
	Check(!ValidLedger(5, 4, 2, 20)); Check(!ValidLedger(5, -1, 6, 20));
	for (int Budget = 18; Budget <= 22; ++Budget)
	{
		for (int Earned = 0; Earned <= Budget; ++Earned)
		{
			for (int Spent = 0; Spent <= Earned; ++Spent)
			{
				Check(ValidLedger(Earned, Earned - Spent, Spent, Budget));
				Check(ValidLedger(Earned, Earned, 0, Budget)); // free respec preserves earnings
				Check(!ValidLedger(Earned, Earned - Spent + 1, Spent, Budget)); // duplicated point
			}
		}
	}
	// Rank weight: the zero-based rule that spend accounting, branch investment and branch-level
	// rebuild all share. It was written out five times in the component before being named once.
	Check(RankWeight(0) == 1);
	Check(RankWeight(1) == 2);
	Check(RankWeight(4) == 5);
	// A failed grant sets the level to -1, so negative levels genuinely occur. They are not
	// purchases and must contribute nothing; a bare PerkLevel + 1 would let a level below -1
	// subtract from another perk's investment.
	Check(RankWeight(-1) == 0);
	Check(RankWeight(-2) == 0);
	Check(RankWeight(-100) == 0);
	// Monotonic and never negative across the authored rank range and well past it.
	for (int Level = -20; Level <= 40; ++Level)
	{
		Check(RankWeight(Level) >= 0);
		if (Level > 0) { Check(RankWeight(Level) >= RankWeight(Level - 1)); }
	}
	// A tree carries 24-30 purchasable ranks; five ranks is the authored per-perk maximum.
	{
		int Total = 0;
		for (int Level = 0; Level < 5; ++Level) { Total += RankWeight(Level); }
		Check(Total == 15);
	}

	// Branch investment gate. A perk never counts toward its own requirement; callers exclude it
	// while accumulating, so this compares totals only.
	Check(MeetsBranchInvestment(0, 0));
	Check(MeetsBranchInvestment(1, 0));
	Check(MeetsBranchInvestment(3, 3));
	Check(!MeetsBranchInvestment(2, 3));
	Check(!MeetsBranchInvestment(0, 1));
	Check(MeetsBranchInvestment(30, 30));
	for (int Required = 0; Required <= 30; ++Required)
	{
		for (int Investment = 0; Investment <= 30; ++Investment)
		{
			Check(MeetsBranchInvestment(Investment, Required) == (Investment >= Required));
		}
	}

	std::cout << Checks << " Technique production-policy checks passed\n";
}
