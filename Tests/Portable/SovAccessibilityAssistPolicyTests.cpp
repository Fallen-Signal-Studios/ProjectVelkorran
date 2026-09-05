#include "Targeting/SovAimAssistPolicy.h"
#include "Sovereign/SovMovementAssistPolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main()
{
	using SovMovementAssistPolicy::WantsAutomaticSprint;
	unsigned Checks = 0;
	for (unsigned Mask = 0; Mask < 8; ++Mask)
	{
		for (const float Magnitude : {0.f, .5f, .849f, .85f, 1.f, 1.4143f})
		{
			assert(WantsAutomaticSprint((Mask & 1) != 0, Magnitude, (Mask & 2) != 0, (Mask & 4) != 0)
				== (Mask == 7 && Magnitude >= .85f)); ++Checks;
		}
	}
	assert(!WantsAutomaticSprint(true, std::numeric_limits<float>::quiet_NaN(), true, true)); ++Checks;
	assert(!WantsAutomaticSprint(true, std::numeric_limits<float>::infinity(), true, true)); ++Checks;
	double Time = 0.;
	using SovAimAssistPolicy::SolveIntercept;
	assert(SolveIntercept(10000., 0., 0., 1000., Time) && std::abs(Time - .1) < 1.e-10); ++Checks;
	assert(SolveIntercept(10000., 10000., 10000., 1000., Time) && std::abs(Time - 100./900.) < 1.e-10); ++Checks;
	assert(SolveIntercept(10000., -10000., 10000., 1000., Time) && std::abs(Time - 100./1100.) < 1.e-10); ++Checks;
	assert(!SolveIntercept(10000., 200000., 4000000., 1000., Time)); ++Checks;
	assert(!SolveIntercept(1000000., 0., 0., 1000., Time)); ++Checks;
	assert(!SolveIntercept(0., 0., 0., 1000., Time)); ++Checks;
	assert(!SolveIntercept(100., 0., 0., 0., Time)); ++Checks;
	assert(!SolveIntercept(100., 0., -1., 1000., Time)); ++Checks;
	assert(!SolveIntercept(std::numeric_limits<double>::infinity(), 0., 0., 1000., Time)); ++Checks;
	assert(SolveIntercept(10000., -100000., 1000000., 1000., Time) && std::abs(Time - .05) < 1.e-10); ++Checks;
	for (int Speed = 1000; Speed <= 6000; Speed += 100)
	{
		for (int Velocity = -500; Velocity <= 500; Velocity += 25)
		{
			const double X = 300., V = Velocity;
			assert(SolveIntercept(X*X, 0., V*V, Speed, Time));
			const double PredictedDistance = std::sqrt(X*X + V*V*Time*Time);
			assert(std::abs(PredictedDistance - Speed*Time) < 1.e-7);
			assert(Time > 0. && Time <= SovAimAssistPolicy::MaximumLeadSeconds); Checks += 3;
		}
	}
	std::cout << "Accessibility assistance: " << Checks << " production-policy checks passed\n";
}
