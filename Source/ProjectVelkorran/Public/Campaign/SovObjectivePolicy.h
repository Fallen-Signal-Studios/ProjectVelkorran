// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

/** Production-used policy, independent of Unreal so every state pair can be checked portably. */
namespace SovObjectivePolicy
{
	enum class State : unsigned char { Inactive, Available, Active, Succeeded, Failed, Skipped, Superseded };
	constexpr bool IsValid(State Value) { return Value >= State::Inactive && Value <= State::Superseded; }
	constexpr bool IsTerminal(State Value) { return Value >= State::Succeeded && Value <= State::Superseded; }
	constexpr bool CanTransition(State From, State To, bool Optional, bool CanonGate, bool Choice, bool FailureAuthored)
	{
		if (!IsValid(From) || !IsValid(To) || (From != State::Available && From != State::Active)) { return false; }
		if (To == State::Active) { return From == State::Available; }
		if (!Optional || CanonGate || Choice) { return false; }
		return To == State::Skipped || (To == State::Failed && FailureAuthored);
	}
	constexpr bool CanComplete(State From) { return From == State::Available || From == State::Active; }
}
