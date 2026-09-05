#include "Recovery/SovRecoveryPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
	unsigned Checks = 0;
	for (unsigned Mask = 0; Mask < 512; ++Mask)
	{
		const bool Owns = (Mask & 1) != 0, Active = (Mask & 2) != 0, Permit = (Mask & 4) != 0;
		const bool Difficulty = (Mask & 8) != 0, Used = (Mask & 16) != 0, Verified = (Mask & 32) != 0;
		const bool Hazard = (Mask & 64) != 0, Companion = (Mask & 128) != 0, Safe = (Mask & 256) != 0;
		const bool Expected = Mask == (1u | 2u | 4u | 8u | 32u | 128u | 256u);
		assert(SovRecoveryPolicy::CanRescue(Owns, Active, Permit, Difficulty, Used, Verified, Hazard, Companion, Safe) == Expected); ++Checks;
	}
	assert(SovRecoveryPolicy::RescueHealth(100.0) == 35.0); ++Checks;
	assert(SovRecoveryPolicy::RescueHealth(-1.0) == 0.0); ++Checks;
	assert(SovRecoveryPolicy::RescueHealth(std::numeric_limits<double>::quiet_NaN()) == 0.0); ++Checks;
	assert(SovRecoveryPolicy::RescueHealth(std::numeric_limits<double>::infinity()) == 0.0); ++Checks;
	assert(SovRecoveryPolicy::DecisionDelaySeconds + SovRecoveryPolicy::RetryDelaySeconds < 3.0); ++Checks;
	std::cout << "Recovery policy: " << Checks << " assertions passed\n";
}
