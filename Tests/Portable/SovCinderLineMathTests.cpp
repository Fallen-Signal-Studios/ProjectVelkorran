// Compiles the same bounded line/timing helpers used by production abilities.
#include "Combat/SovCinderLineMath.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main()
{
	using namespace SovCinderLine;
	int Checks = 0;
	auto Check = [&Checks](bool Condition) { assert(Condition); ++Checks; };
	Check(NodeCount(0.0, 350.0) == 1);
	Check(NodeCount(1.0, 350.0) == 2);
	Check(NodeCount(350.0, 350.0) == 2);
	Check(NodeCount(351.0, 350.0) == 3);
	Check(NodeCount(12000.0, 350.0) == 36);
	Check(NodeCount(30000.0, 1.0) == 64);
	Check(NodeCount(std::numeric_limits<double>::max(), 1.0) == 64);
	Check(NodeCount(-1.0, 350.0) == 0);
	Check(NodeCount(1.0, 0.0) == 0);
	Check(NodeCount(1.0, -1.0) == 0);
	Check(NodeCount(std::numeric_limits<double>::quiet_NaN(), 350.0) == 0);
	Check(NodeCount(1.0, std::numeric_limits<double>::infinity()) == 0);
	Check(NodeAlpha(0, 1) == 0.0);
	Check(NodeAlpha(0, 36) == 0.0);
	Check(NodeAlpha(35, 36) == 1.0);
	Check(NodeAlpha(100, 36) == 1.0);
	Check(NodeAlpha(-1, 36) == 0.0);
	for (int N = 2; N <= 64; ++N)
	{
		Check(NodeAlpha(0, N) == 0.0 && NodeAlpha(N - 1, N) == 1.0);
		for (int I = 1; I < N; ++I) { Check(NodeAlpha(I, N) > NodeAlpha(I - 1, N)); }
	}
	Check(ValidTiming(0.35, 0.4, 5.0));
	Check(ValidTiming(0.55, 0.65, 5.0));
	Check(!ValidTiming(0.55, 0.65, 1.0));
	Check(!ValidTiming(-0.1, 0.4, 5.0));
	Check(!ValidTiming(0.1, -0.4, 5.0));
	Check(!ValidTiming(0.1, 0.4, std::numeric_limits<double>::infinity()));
	std::cout << Checks << " Cinder line production-math checks passed\n";
}
