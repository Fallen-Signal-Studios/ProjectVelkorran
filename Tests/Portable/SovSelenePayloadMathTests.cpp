// Build: g++ -std=c++17 -Wall -Wextra -Werror Tests/Portable/SovSelenePayloadMathTests.cpp -o /tmp/sov_selene_math
#include "../../Source/ProjectVelkorran/Private/Combat/SovSelenePayloadMath.h"
#include <cassert>
#include <limits>
int main()
{
	using namespace SovSelenePayloadMath;
	assert(IsInCenterline(400.0, 60.0, 4.0, 0.0, 120.0));
	assert(!IsInCenterline(400.0, 60.01, 4.0, 0.0, 120.0));
	assert(IsInCenterline(60.0, 400.0, 0.0, 2.0, 120.0));
	assert(!IsInCenterline(0.0, 0.0, 0.0, 0.0, 120.0));
	assert(!IsInCenterline(std::numeric_limits<double>::infinity(), 0.0, 1.0, 0.0, 120.0));
	assert(!ShouldRecall(2.49, 2399.9, 2.5, 2400.0));
	assert(ShouldRecall(2.5, 10.0, 2.5, 2400.0));
	assert(ShouldRecall(0.1, 2400.0, 2.5, 2400.0));
	assert(ShouldRecall(std::numeric_limits<double>::quiet_NaN(), 0.0, 2.5, 2400.0));
	assert(RemainingStep(2200.0, 0.1, 2100.0, 2200.0) == 100.0);
	assert(RemainingStep(2200.0, 0.1, 2200.0, 2200.0) == 0.0);
	assert(RemainingStep(2200.0, 0.1, 2300.0, 2200.0) == 0.0);
	assert(RemainingStep(2200.0, -1.0, 0.0, 2200.0) == 0.0);
	assert(RemainingStep(2200.0, 0.1, 0.0, 2200.0) == 220.0);
}
