// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovPlayerInformationPolicy.h"
#include <array>
#include <cassert>
#include <limits>

int main()
{
	using namespace SovPlayerInformationPolicy;
	assert(CanPreempt(0, 2, true));
	assert(CanPreempt(1, 2, true));
	for (int Incoming = 0; Incoming <= 2; ++Incoming) { assert(!CanPreempt(2, Incoming, true)); }
	assert(!CanPreempt(1, 0, true));
	assert(!CanPreempt(1, 1, true));
	assert(CanPreempt(2, 0, false));
	// Three bounded refreshes discover every actor, including those after the old first-256 cutoff.
	std::array<bool, 700> Seen{};
	std::size_t Cursor = 0;
	for (int Refresh = 0; Refresh < 3; ++Refresh)
	{
		for (int Inspected = 0; Inspected < 256; ++Inspected) { Seen[TakeNext(Seen.size(), Cursor)] = true; }
	}
	for (bool Discovered : Seen) { assert(Discovered); }
	Cursor = std::numeric_limits<std::size_t>::max();
	assert(TakeNext(1, Cursor) == 0 && Cursor == 0);
	assert(TakeNext(0, Cursor) == 0 && Cursor == 0);
	// Roster shrink and expansion do not produce out-of-range indices or restart a prefix-only scan.
	Cursor = 688;
	assert(TakeNext(12, Cursor) < 12);
	std::array<bool, 13> Expanded{};
	for (int Index = 0; Index < 13; ++Index) { Expanded[TakeNext(13, Cursor)] = true; }
	for (bool Discovered : Expanded) { assert(Discovered); }
}
