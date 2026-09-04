#include "Combat/SovProtectionAwardPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
	using SovProtectionAwardPolicy::HasCooldownElapsed;
	assert(!HasCooldownElapsed(10., 10., 5.));
	assert(!HasCooldownElapsed(14.999, 10., 5.));
	assert(HasCooldownElapsed(15., 10., 5.));
	assert(HasCooldownElapsed(15.001, 10., 5.));
	assert(!HasCooldownElapsed(9., 10., 5.));
	assert(!HasCooldownElapsed(0., 10., 0.));
	assert(!HasCooldownElapsed(0.099, 0., 0.));
	assert(HasCooldownElapsed(0.1, 0., 0.));
	assert(!HasCooldownElapsed(0., 0., -5.));
	assert(HasCooldownElapsed(0.1, 0., -5.));
	assert(!HasCooldownElapsed(std::numeric_limits<double>::infinity(), 0., 5.));
	assert(!HasCooldownElapsed(15., std::numeric_limits<double>::quiet_NaN(), 5.));
	assert(!HasCooldownElapsed(15., 0., std::numeric_limits<double>::quiet_NaN()));
	std::cout << "Protection award cooldown: 13 assertions passed\n";
}
