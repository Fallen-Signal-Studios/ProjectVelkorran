// Portable boundary tests of the exact production readiness/precision-chain policy.
#include "Combat/SovEchoAwardPolicy.h"
#include <cassert>
#include <limits>
int main()
{
	using namespace SovEchoAwardPolicy;
	assert(!IsMeterReady(89.f, 90.f));
	assert(IsMeterReady(90.f, 90.f));
	assert(!IsMeterReady(90.f, 95.f));
	assert(!IsMeterReady(100.f, -1.f));
	assert(!IsMeterReady(std::numeric_limits<float>::quiet_NaN(), 90.f));
	assert(!IsMeterReady(100.f, std::numeric_limits<float>::infinity()));
	TPrecisionChain<int> Chain;
	assert(!Chain.Advance(1, 0., 3., 3));
	assert(!Chain.Advance(1, 1., 3., 3));
	assert(Chain.Advance(2, 2., 3., 3));
	assert(Chain.Advance(3, 3., 3., 3));
	assert(Chain.Advance(4, 4., 3., 3));
	assert(!Chain.Advance(5, 5., 3., 3));
	assert(!Chain.Advance(6, 6., 3., 3));
	assert(Chain.Targets.size() == 4 && Chain.BonusLinks == 3);
	assert(!Chain.Advance(7, 10., 3., 3)); // expired, begins a fresh chain
	assert(Chain.Advance(8, 13., 3., 3)); // exact deadline is included
	Chain.Reset();
	assert(!Chain.Advance(9, 14., 3., 3));
	assert(!Chain.Advance(10, 1., 3., 3)); // a reset world clock cannot inherit links
	assert(!Chain.Advance(11, 2., -1., 3));
	assert(Chain.Targets.empty());
	assert(!Chain.Advance(1, 1., 3., 0));
	assert(!Chain.Advance(2, 2., 3., 0));
}
