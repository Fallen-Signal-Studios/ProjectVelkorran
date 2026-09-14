// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterCoordinationPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
	using namespace SovEncounterCoordinationPolicy;
	int Checks = 0;
	for (int Capacity = -1; Capacity <= 40; ++Capacity)
	{
		for (int Current = -1; Current <= 41; ++Current)
		{ assert(HasSlot(Current, Capacity) == (Current >= 0 && Capacity > 0 && Current < Capacity)); ++Checks; }
	}
	for (int Milliseconds = 0; Milliseconds <= 2000; ++Milliseconds)
	{
		const double Now = Milliseconds / 1000.;
		assert(WarningReady(false, true, true, Now, .5, .75) == (Now >= 1.25)); ++Checks;
		assert(!WarningReady(false, true, false, Now, .5, .75)); ++Checks;
		assert(WarningReady(true, true, false, Now, .5, .75)); ++Checks;
	}
	assert(LowResources(25., 100., 100., 100., 100., 100.)); ++Checks;
	assert(!LowResources(25.001, 100., 100., 100., 100., 100.)); ++Checks;
	assert(LowResources(90., 100., 10., 100., 10., 100.)); ++Checks;
	assert(!LowResources(90., 100., 10., 100., 10.001, 100.)); ++Checks;
	assert(!LowResources(0., 100., 0., 100., 0., 100.)); ++Checks;
	assert(!LowResources(10., 0., 0., 100., 0., 100.)); ++Checks;
	assert(!LowResources(10., 100., std::numeric_limits<double>::quiet_NaN(), 100., 0., 100.)); ++Checks;
	assert(!LowResources(10., 100., -1., 100., 0., 100.)); ++Checks;
	assert(!WarningReady(false, true, true, 2., .5, std::numeric_limits<double>::infinity())); ++Checks;
	assert(Slots(2, false) == 2 && Slots(2, true) == 1 && Slots(1, true) == 1); ++Checks;
	// A 90 degree horizontal frame at 16:9 spans +/-1 horizontally and +/-0.5625 vertically per unit of depth.
	const double Aspect = RemoteViewAspectRatio;
	const double HalfWidth = std::tan(90. * std::acos(-1.) / 360.);
	for (int Step = -200; Step <= 200; ++Step)
	{
		const double Offset = Step / 100.;
		assert(WithinViewFrame(1., Offset, 0., 90., Aspect) == (std::abs(Offset) <= HalfWidth)); ++Checks;
		assert(WithinViewFrame(2., Offset, 0., 90., Aspect) == (std::abs(Offset) <= 2. * HalfWidth)); ++Checks;
		assert(WithinViewFrame(1., 0., Offset, 90., Aspect) == (std::abs(Offset) <= HalfWidth / Aspect)); ++Checks;
		assert(!WithinViewFrame(-1., 0., Offset, 90., Aspect)); ++Checks;
	}
	assert(WithinViewFrame(500., 0., 0., 90., Aspect) && !WithinViewFrame(0., 0., 0., 90., Aspect)); ++Checks;
	assert(!WithinViewFrame(1., 0., 0., 0., Aspect) && !WithinViewFrame(1., 0., 0., 180., Aspect) && !WithinViewFrame(1., 0., 0., 90., 0.)); ++Checks;
	assert(!WithinViewFrame(std::numeric_limits<double>::quiet_NaN(), 0., 0., 90., Aspect)); ++Checks;
	assert(!WithinViewFrame(1., std::numeric_limits<double>::infinity(), 0., 90., Aspect)); ++Checks;
	assert(WithinViewFrame(1., .99, .55, 90., Aspect) && !WithinViewFrame(1., .99, .57, 90., Aspect)); ++Checks;
	std::cout << "Encounter coordination: " << Checks << " budget, warning timing and relief boundary checks passed\n";
}
