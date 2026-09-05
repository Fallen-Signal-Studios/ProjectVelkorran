// Executes the production policy; no claim of running Slate or console hardware.
#include "UI/SovConsoleUIPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
	using SovConsoleUIPolicy::ScrollOffset;
	assert(ScrollOffset(100.f, 1000.f, 0.f, .02f) == 100.f);
	assert(ScrollOffset(100.f, 1000.f, .2f, .02f) == 100.f);
	assert(ScrollOffset(100.f, 1000.f, -.2f, .02f) == 100.f);
	assert(ScrollOffset(100.f, 1000.f, 1.f, .02f) < 100.f);
	assert(ScrollOffset(100.f, 1000.f, -1.f, .02f) > 100.f);
	assert(ScrollOffset(0.f, 1000.f, 1.f, .05f) == 0.f);
	assert(ScrollOffset(1000.f, 1000.f, -1.f, .05f) == 1000.f);
	assert(ScrollOffset(100.f, 1000.f, -1.f, 3600.f) == ScrollOffset(100.f, 1000.f, -1.f, .05f));
	assert(ScrollOffset(100.f, 1000.f, -1.f, -1.f) == 100.f);
	assert(ScrollOffset(100.f, 0.f, -1.f, .02f) == 0.f);
	for (float Bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
	{
		assert(ScrollOffset(100.f, 1000.f, Bad, .02f) == 100.f);
		assert(ScrollOffset(100.f, 1000.f, -1.f, Bad) == 100.f);
		assert(ScrollOffset(100.f, Bad, -1.f, .02f) == 0.f);
		assert(std::isfinite(ScrollOffset(Bad, 1000.f, -1.f, .02f)));
	}
	float Offset = 0.f;
	for (int Frame = 0; Frame < 1000; ++Frame) { Offset = ScrollOffset(Offset, 5000.f, -1.f, 1.f / 60.f); }
	assert(Offset == 5000.f); // Full long record remains reachable with continuous input.
	for (int Frame = 0; Frame < 1000; ++Frame) { Offset = ScrollOffset(Offset, 5000.f, 1.f, 1.f / 60.f); }
	assert(Offset == 0.f);
	std::cout << "PASS: console record scrolling, dead zone, bounds, invalid input and resume delta\n";
}
