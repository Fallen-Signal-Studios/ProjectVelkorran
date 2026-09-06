// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovObjectivePolicy.h"
#include <cassert>
#include <iostream>

int main()
{
	using namespace SovObjectivePolicy;
	for (unsigned int From = 0; From <= 255; ++From)
	for (unsigned int To = 0; To <= 255; ++To)
	for (unsigned int Flags = 0; Flags < 16; ++Flags)
	{
		const auto A = static_cast<State>(From), B = static_cast<State>(To);
		const bool Optional = (Flags & 1) != 0, Canon = (Flags & 2) != 0;
		const bool Choice = (Flags & 4) != 0, Failure = (Flags & 8) != 0;
		const bool Allowed = CanTransition(A, B, Optional, Canon, Choice, Failure);
		if (From > 6 || To > 6 || From == To || IsTerminal(A) || A == State::Inactive) { assert(!Allowed); }
		if (Allowed)
		{
			assert(A == State::Available || A == State::Active);
			assert(B == State::Active || B == State::Failed || B == State::Skipped);
			if (B != State::Active) { assert(Optional && !Canon && !Choice); }
			if (B == State::Failed) { assert(Failure); }
		}
	}
	assert(CanTransition(State::Available, State::Active, false, true, false, false));
	assert(CanTransition(State::Active, State::Failed, true, false, false, true));
	assert(CanTransition(State::Available, State::Skipped, true, false, false, false));
	assert(!CanTransition(State::Active, State::Failed, true, false, true, true));
	assert(!CanTransition(State::Available, State::Succeeded, true, false, false, false));
	assert(!CanTransition(State::Active, State::Superseded, true, false, false, false));
	assert(CanComplete(State::Active) && CanComplete(State::Available));
	for (State Closed : { State::Inactive, State::Succeeded, State::Failed, State::Skipped, State::Superseded }) { assert(!CanComplete(Closed)); }
	std::cout << "Objective lifecycle: all 1,048,576 state/flag combinations and completion restrictions passed.\n";
}
