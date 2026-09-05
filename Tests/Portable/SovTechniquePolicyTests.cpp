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
	std::cout << Checks << " Technique production-policy checks passed\n";
}
