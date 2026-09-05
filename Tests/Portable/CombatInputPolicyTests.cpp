// Copyright Fallen Signal Studios. All Rights Reserved.
#include "GAS/SovCombatInputPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
	using namespace SovCombatInputPolicy;
	assert(ValidWindow(0., 1.)); assert(!ValidWindow(-1., 1.)); assert(!ValidWindow(1., 1.));
	assert(!ValidWindow(2., 1.)); assert(!ValidWindow(0., 11.));
	assert(!ValidWindow(0., std::numeric_limits<double>::quiet_NaN()));
	assert(CanConsume(.22, 0., .2, .8, .22));
	assert(!CanConsume(.22001, 0., .2, .8, .22));
	assert(!CanConsume(.19, .18, .2, .8, .22));
	assert(!CanConsume(.81, .8, .2, .8, .22));
	assert(!CanConsume(.3, .4, .2, .8, .22));
	assert(CanConsume(.4, .1, .2, .8, .42));
	assert(!CanConsume(.1, 0., .2, .8, .42));
	assert(!CanConsume(.3, .2, .2, .8, -1.));
	for (int Milliseconds = 0; Milliseconds <= 1000; ++Milliseconds)
	{
		const double Now = Milliseconds / 1000.;
		const bool Expected = Now >= .4 && Now <= .6 && Now >= .25 && Now - .25 <= .22;
		assert(CanConsume(Now, .25, .4, .6, .22) == Expected);
	}
	std::cout << "Combat input policy: 1015 window, freshness, assistance and invalid-time checks passed\n";
}
