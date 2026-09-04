// Build: g++ -std=c++17 -Wall -Wextra -Werror Tests/Portable/SovCorruptionMathTests.cpp -o /tmp/sov_corruption_math
#include "../../Source/ProjectVelkorran/Private/Corruption/SovCorruptionMath.h"
#include <cassert>
#include <limits>
int main()
{
	using namespace SovCorruptionMath;
	assert(ValidThresholds(1, 25, 50, 80, 5));
	assert(!ValidThresholds(25, 1, 50, 80, 5));
	assert(!ValidThresholds(1, 25, 50, 80, 30));
	assert(Band(0, 4, 1, 25, 50, 80, 5) == 0);
	assert(Band(25, 0, 1, 25, 50, 80, 5) == 2);
	assert(Band(24, 2, 1, 25, 50, 80, 5) == 2);
	assert(Band(19.9, 2, 1, 25, 50, 80, 5) == 1);
	assert(Band(80, 0, 1, 25, 50, 80, 5) == 4);
	assert(Band(50, 0, 1, 25, 50, 80, 0) == 3);
	assert(Band(std::numeric_limits<double>::quiet_NaN(), 0, 1, 25, 50, 80, 5) == 0);
	assert(AddCapped(20, 10, 24) == 4);
	assert(AddCapped(60, 10, 24) == 0);
	assert(AddCapped(99, 10, 100) == 1);
	assert(AddCapped(10, -10, 100) == 0);
	assert(Falloff(250, 500, true) == 0.5);
	assert(Falloff(250, 500, false) == 1.0);
	assert(Falloff(501, 500, false) == 0.0);
	assert(Falloff(0, 0, false) == 0.0);
}
