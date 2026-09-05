#include "../../Source/ProjectVelkorran/Private/UI/Dialogue/SovDialoguePressurePolicy.h"
#include <cassert>
#include <limits>
using namespace SovDialoguePressure;
int main()
{
	State S;
	S.Begin(1, 0, true, Mode::Standard, 2, 5, false);
	S.TextReady = true;
	assert(!S.Advance(1, 10000, false)); // default choice has no pressure
	assert(!S.Advance(1, 10000, false));
	S.Begin(2, 4, false, Mode::Standard, 2, 5, false);
	S.TextReady = true; S.Advance(2, 100, false);
	assert(!S.Advance(2, 100, false)); // no authored valid silence
	S.Begin(3, 4, true, Mode::Standard, 2, 5, true);
	assert(!S.Advance(3, 100, false)); // no rendered text
	S.TextReady = true;
	assert(!S.Advance(3, 100, false)); // no real speech completion
	S.SetSpeechComplete(2, true);
	assert(!S.Advance(3, 100, false)); // stale completion
	S.SetSpeechComplete(3, true);
	assert(!S.Advance(3, 100, true)); // pause charges nothing
	assert(S.Remaining() == 4);
	assert(!S.Advance(3, 3, false));
	S.SuspendReading(); S.TextReady = true;
	assert(!S.Advance(3, 100, false)); // resume requires replay completion
	assert(S.Remaining() == 1);
	S.SetSpeechComplete(3, true);
	assert(S.Advance(3, 1, false));
	assert(S.TryCommit(3)); assert(!S.TryCommit(3)); // exactly once
	S.Begin(4, 4, true, Mode::Extended, 3, 5, false);
	S.TextReady = true;
	assert(!S.Advance(4, 5, false)); // min read cannot consume pressure in same tick
	assert(S.Remaining() == 12);
	assert(!S.Advance(4, 11, false));
	assert(S.Advance(4, 1, false));
	S.Begin(5, 4, true, Mode::Disabled, 3, 5, false);
	S.TextReady = true; S.Advance(5, 100, false);
	assert(!S.Advance(5, 100, false));
	S.Begin(6, 4, true, Mode::Standard, 2, 5, false);
	S.TextReady = true;
	assert(!S.Advance(5, 100, false));
	assert(!S.Advance(6, std::numeric_limits<double>::quiet_NaN(), false));
	assert(!S.Advance(6, -1, false));
	S.Cancel(); assert(!S.Advance(6, 1000, false)); assert(!S.TryCommit(6));
	S.Begin(7, std::numeric_limits<double>::infinity(), true, Mode::Standard, 2, 5, false);
	assert(S.Duration == 0);
	S.Begin(8, 4, true, Mode::Standard, 2, 5, true);
	S.TextReady = true; S.SetSpeechComplete(8, true);
	assert(!S.Advance(8, 4, false)); assert(!S.Ready());
	assert(!S.Advance(8, 1, false)); assert(S.Ready());
	S.SetSpeechComplete(8, false); assert(!S.Advance(8, 100, false));
	assert(S.Remaining() == 4); // selection speech pauses pressure
	return 0;
}
