// Copyright Fallen Signal Studios. All Rights Reserved.
#include "FieldRecovery/SovFieldRecoveryPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
	using namespace SovFieldRecoveryPolicy;
	int Checks = 0;
	for (int Capacity = -1; Capacity <= 11; ++Capacity)
	{
		for (int Current = -1; Current <= 12; ++Current)
		{
			for (int Health = 0; Health <= 100; ++Health)
			{
				const bool Expected = Capacity >= 1 && Capacity <= 10 && Current > 0 && Current <= Capacity && Health > 0 && Health < 100;
				assert(CanBegin(Current, Capacity, Health, 100., .35) == Expected); ++Checks;
			}
		}
	}
	assert(HealAmount(20., 100., .35) == 35.); ++Checks;
	assert(HealAmount(90., 100., .35) == 10.); ++Checks;
	assert(HealAmount(0., 100., .35) == 0.); ++Checks;
	assert(HealAmount(20., 100., std::numeric_limits<double>::quiet_NaN()) == 0.); ++Checks;
	assert(!Completed(.9999, 0., 1.)); ++Checks;
	assert(Completed(1., 0., 1.)); ++Checks;
	assert(!Completed(1., 2., 1.)); ++Checks;
	assert(!Completed(2., 0., 0.)); ++Checks;
	assert(!Completed(std::numeric_limits<double>::infinity(), 0., 1.)); ++Checks;
	std::cout << "Field recovery: " << Checks << " charge admission, bounded healing and completion checks passed\n";
}
