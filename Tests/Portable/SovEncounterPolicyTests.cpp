// Run: g++ -std=c++17 -Wall -Wextra -Werror -ISource/ProjectVelkorran/Public Tests/Portable/SovEncounterPolicyTests.cpp -o /tmp/sov_encounter_policy_tests
#include "Campaign/SovEncounterPolicy.h"
#include <cassert>
#include <limits>
#include <iostream>

int main()
{
	using namespace SovEncounterPolicy;
	for (unsigned state = 0; state != 8; ++state)
	{
		assert(CanBegin(state) == (state == 0));
		assert(CanResolve(state) == (state == 1));
		assert(!CanRetry(state, false));
		assert(CanRetry(state, true) == (state == 1 || state == 3));
		for (bool valid : {false, true})
		{
			for (bool claimed : {false, true})
			{
				assert(CanClaimCompletionReward(state, valid, claimed) == (state == 2 && valid && !claimed));
			}
		}
	}
	assert(StateAfterLoad(0) == 0);
	assert(StateAfterLoad(0, true) == 3);
	assert(StateAfterLoad(1) == 3);
	assert(StateAfterLoad(2) == 2);
	assert(StateAfterLoad(3) == 3);
	assert(StateAfterLoad(4) == 3);
	assert(StateAfterLoad(255) == 3);
	// Retry chains never create a second completion reward or make a restored
	// in-progress transaction active without initialization.
	unsigned state = 0;
	assert(CanBegin(state)); state = 1;
	state = StateAfterLoad(state); assert(state == 3 && CanRetry(state, true));
	state = 4; assert(!CanRetry(state, true) && !CanResolve(state));
	state = 1; assert(CanResolve(state)); state = 2;
	assert(CanClaimCompletionReward(state, true, false));
	assert(!CanClaimCompletionReward(StateAfterLoad(state), true, true));
	assert(!CanRetry(state, true));
	// A retuned definition owns the new maxima, while saved current values keep
	// their absolute amount (not their old percentage).
	assert(ClampRestoredResource(75, 200) == 75);
	assert(ClampRestoredResource(75, 50) == 50);
	assert(ClampRestoredResource(0, 100) == 0);
	assert(ClampRestoredResource(30, 0) == 0);
	assert(ClampRestoredResource(-10, 100) == 0);
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	assert(!ValidResource(nan, 100) && !ValidResource(20, nan));
	assert(!ValidResource(inf, 100) && !ValidResource(20, inf));
	assert(!ValidResource(-1, 100) && !ValidResource(101, 100));
	assert(ValidResource(0, 0) && ValidResource(100, 100));
	assert(ClampRestoredResource(nan, 100) == 0 && ClampRestoredResource(20, nan) == 0);
	for (int current = -5; current < 205; ++current)
	{
		for (int maximum = 0; maximum < 200; ++maximum)
		{
			const float restored = ClampRestoredResource(static_cast<float>(current), static_cast<float>(maximum));
			assert(restored >= 0 && restored <= maximum);
			if (current >= 0 && current <= maximum) { assert(restored == current); }
		}
	}
	std::cout << "Encounter policy: transitions, load recovery, reward replay, invalid values, and 42,000 resource cases passed\n";
}
