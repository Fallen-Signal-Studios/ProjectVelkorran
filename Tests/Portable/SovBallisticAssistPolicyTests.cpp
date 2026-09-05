#include "Targeting/SovAimAssistPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace SovAimAssistPolicy;
int main()
{
	unsigned Checks = 0;
	const auto CheckSolution = [&](Vector R, Vector V, Vector G, double Speed, double Horizon, bool High)
	{
		Vector Launch; double Time = 0.;
		assert(SolveBallisticIntercept(R, V, G, Speed, Horizon, High, Launch, Time));
		assert(Time > 0. && Time <= Horizon);
		assert(std::abs(Launch.Dot(Launch) - Speed * Speed) <= Speed * Speed * 1.e-7);
		const Vector Error = PositionAtTime({}, Launch, G, Time) - (R + V * Time);
		assert(Error.Dot(Error) < 1.e-12);
		Checks += 4;
		return Time;
	};
	CheckSolution({500., 0., 0.}, {}, {0., 0., -980.}, 1600., .6, false);
	CheckSolution({300., 20., 100.}, {100., 200., -100.}, {0., 0., -980.}, 1600., .6, false);
	CheckSolution({300., 0., 0.}, {}, {}, 1600., .6, false);
	CheckSolution({300., 0., 0.}, {}, {0., 0., 980.}, 1600., .6, false);
	// Both arcs fit the deliberately short horizon under strong gravity.
	const double Low = CheckSolution({50., 0., 0.}, {}, {0., 0., -2000.}, 500., .6, false);
	const double High = CheckSolution({50., 0., 0.}, {}, {0., 0., -2000.}, 500., .6, true);
	assert(High > Low); ++Checks;
	// At maximum range the two roots coincide; no sign-change sampling can find this reliably.
	const double Tangent = CheckSolution({125., 0., 0.}, {}, {0., 0., -2000.}, 500., .6, false);
	assert(std::abs(Tangent - std::sqrt(.125)) < 1.e-7); ++Checks;
	CheckSolution({125., 0., 0.}, {}, {0., 0., -2000.}, 500., .6, true);
	Vector Launch; double Time = 1.;
	assert(!SolveBallisticIntercept({500., 0., 0.}, {}, {0., 0., -980.}, 1600., .6, true, Launch, Time)); ++Checks;
	assert(!SolveBallisticIntercept({126., 0., 0.}, {}, {0., 0., -2000.}, 500., .6, false, Launch, Time)); ++Checks;
	assert(!SolveBallisticIntercept({500., 0., 0.}, {2000., 0., 0.}, {0., 0., -980.}, 1600., .6, false, Launch, Time)); ++Checks;
	assert(!SolveBallisticIntercept({500., 0., 0.}, {}, {0., 0., -980.}, 1600., .1, false, Launch, Time)); ++Checks;
	assert(Time == 0. && Launch.Dot(Launch) == 0.); ++Checks;
	for (double Invalid : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
	{
		assert(!SolveBallisticIntercept({Invalid, 0., 0.}, {}, {}, 1600., .6, false, Launch, Time));
		assert(!SolveBallisticIntercept({100., 0., 0.}, {0., Invalid, 0.}, {}, 1600., .6, false, Launch, Time));
		assert(!SolveBallisticIntercept({100., 0., 0.}, {}, {0., 0., Invalid}, 1600., .6, false, Launch, Time));
		assert(!SolveBallisticIntercept({100., 0., 0.}, {}, {}, Invalid, .6, false, Launch, Time));
		assert(!SolveBallisticIntercept({100., 0., 0.}, {}, {}, 1600., Invalid, false, Launch, Time)); Checks += 5;
	}
	assert(!SolveBallisticIntercept({}, {}, {}, 1600., .6, false, Launch, Time)); ++Checks;
	assert(!SolveBallisticIntercept({100., 0., 0.}, {}, {}, -1600., .6, false, Launch, Time)); ++Checks;
	assert(!SolveBallisticIntercept({100., 0., 0.}, {}, {}, 1600., .6001, false, Launch, Time)); ++Checks;
	// Construct physically reachable endpoints, including oblique gravity and rising/falling targets.
	for (int GZ : {-1960, -980, -100, 0, 980})
	for (int VY = -300; VY <= 300; VY += 100)
	for (int VZ = -300; VZ <= 300; VZ += 100)
	for (int Milliseconds = 20; Milliseconds <= 580; Milliseconds += 20)
	{
		const double T = Milliseconds / 1000.;
		const Vector OriginalLaunch{1590., 50., 170.};
		const double Speed = std::sqrt(OriginalLaunch.Dot(OriginalLaunch));
		const Vector G{20., -10., static_cast<double>(GZ)}, V{75., static_cast<double>(VY), static_cast<double>(VZ)};
		const Vector R = PositionAtTime({}, OriginalLaunch, G, T) - V * T;
		CheckSolution(R, V, G, Speed, .6, false);
	}
	// Exact acceleration stepping must agree across frame partitions with the production position function.
	for (int Steps : {1, 2, 17, 36, 60})
	{
		Vector Position{20., 10., 500.}, Velocity{1500., 20., 200.};
		const Vector Gravity{0., 0., -450.};
		const Vector Expected = PositionAtTime(Position, Velocity, Gravity, .6);
		for (int I = 0; I < Steps; ++I)
		{
			Position = PositionAtTime(Position, Velocity, Gravity, .6 / Steps);
			Velocity = Velocity + Gravity * (.6 / Steps);
		}
		const Vector Error = Position - Expected;
		assert(Error.Dot(Error) < 1.e-16); ++Checks;
	}
	std::cout << "Ballistic assistance: " << Checks << " production-policy checks passed\n";
}
