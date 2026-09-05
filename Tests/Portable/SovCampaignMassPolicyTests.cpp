// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignMassPolicy.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

int main()
{
	using namespace SovCampaignMassPolicy;
	std::uint64_t Checks = 0;
	for (unsigned State = 0; State <= 6; ++State)
	{
		for (unsigned Tier = 0; Tier <= 4; ++Tier)
		{
			for (unsigned Bits = 0; Bits < 128; ++Bits)
			{
				const bool Authority = (Bits & 1u) != 0;
				const bool Boundary = (Bits & 2u) != 0;
				const bool OptIn = (Bits & 4u) != 0;
				const bool Busy = (Bits & 8u) != 0;
				const bool Pending = (Bits & 16u) != 0;
				const bool Defeated = (Bits & 32u) != 0;
				const unsigned TestedState = (Bits & 64u) != 0 ? std::numeric_limits<unsigned>::max() : State;
				const bool Expected = TestedState == 1u && Authority && Boundary && OptIn
					&& !Busy && !Pending && !Defeated && Tier <= 2u;
				assert(CanTransition(TestedState, Authority, Boundary, OptIn, Busy, Pending, Defeated, Tier) == Expected);
				++Checks;
			}
		}
	}
	assert(!CanTransition(1u, true, true, true, false, false, false, std::numeric_limits<unsigned>::max())); ++Checks;

	const std::uint64_t Generations[] = { 0, 1, 2, 3, 0xFFFFFFFFull, 0x100000000ull,
		std::numeric_limits<std::uint64_t>::max() - 1, std::numeric_limits<std::uint64_t>::max() };
	for (const auto Expected : Generations)
	{
		for (const auto Observed : Generations)
		{
			for (unsigned Bits = 0; Bits < 8; ++Bits)
			{
				const bool Owner = (Bits & 1u) != 0, Entity = (Bits & 2u) != 0, Identity = (Bits & 4u) != 0;
				assert(SameReceipt(Expected, Observed, Owner, Entity, Identity)
					== (Expected != 0 && Expected == Observed && Owner && Entity && Identity));
				++Checks;
			}
		}
	}

	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	const unsigned Counts[] = { 0, 1, 127, 128, 129, std::numeric_limits<unsigned>::max() };
	const double Speeds[] = { -Infinity, -1200., -0.000001, -0., 0., 0.000001,
		1199.999999, 1200., 1200.000001, Infinity, NaN };
	for (const unsigned Count : Counts)
	{
		for (const double Speed : Speeds)
		{
			assert(ValidRoute(Count, Speed) == (Count <= 128 && std::isfinite(Speed) && Speed >= 0. && Speed <= 1200.));
			++Checks;
		}
	}
	const double Durations[] = { -Infinity, -1000., -1., -0.000001, -0., 0., 0.000001, 1., Infinity, NaN };
	const double Periods[] = { -Infinity, -1., -0.000001, -0., 0., 0.000001, 1., Infinity, NaN };
	for (const double Duration : Durations)
	{
		for (const double Period : Periods)
		{
			for (unsigned Source = 0; Source < 2; ++Source)
			{
				const bool Expected = std::isfinite(Duration) && Duration == -1.
					&& std::isfinite(Period) && Period == 0. && Source != 0;
				assert(TransferableEffect(Duration, Period, Source != 0) == Expected);
				++Checks;
			}
		}
	}
	std::cout << "Campaign Mass: " << Checks << " transition, receipt, route and effect boundary checks passed\n";
}
