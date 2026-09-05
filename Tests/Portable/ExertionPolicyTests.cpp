// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Exertion/SovExertionPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
	using namespace SovExertionPolicy;
	assert(CanPay(24., 24.)); assert(!CanPay(23.999, 24.)); assert(Spend(24., 24.) == 0.);
	assert(Spend(23., 24.) == 23.); assert(!CanPay(-1., 0.)); assert(!CanPay(20., -1.));
	assert(!CanPay(20., std::numeric_limits<double>::quiet_NaN()));
	assert(!CanPay(std::numeric_limits<double>::infinity(), 20.));
	assert(SprintDrain(100., 16., 10., false, true) == 0.);
	assert(SprintDrain(100., 16., 10., true, false) == 0.);
	assert(SprintDrain(1., 16., 0.5, true, true) == 1.);
	assert(FrameRegen(0., 120., 32., 0.65, 0.65, false, 0.5) == 0.);
	assert(std::abs(FrameRegen(0., 120., 32., 1.65, 0.65, false, 0.5) - 32.) < 1e-10);
	assert(std::abs(FrameRegen(0., 120., 32., 1.65, 0.65, true, 0.5) - 16.) < 1e-10);
	assert(FrameRegen(119., 120., 32., 1., 0., false, 0.5) == 120.);
	for (int Partitions = 1; Partitions <= 1000; ++Partitions)
	{
		double Current = 0., Delay = .65;
		const double Delta = 1.65 / Partitions;
		for (int Frame = 0; Frame < Partitions; ++Frame)
		{
			Current = FrameRegen(Current, 120., 32., Delta, Delay, false, .5);
			Delay = std::max(0., Delay - Delta);
		}
		assert(std::abs(Current - 32.) < 1e-8);
		Current = 120.;
		for (int Frame = 0; Frame < Partitions; ++Frame)
		{
			Current -= SprintDrain(Current, 16., 20. / Partitions, true, true);
		}
		assert(Current >= 0. && Current < 1e-8);
	}
	std::cout << "Exertion policy: 2015 admission, exact-payment, delay, partition and exhaustion checks passed\n";
}
