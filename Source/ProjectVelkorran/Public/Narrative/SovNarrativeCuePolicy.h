#pragma once
#include <algorithm>
#include <cmath>
namespace SovNarrativeCuePolicy
{
	inline bool MayInterrupt(unsigned Incoming, unsigned Current) { return Incoming < 6 && Current < 6 && Incoming < Current; }
	inline bool MayPlayInCombat(unsigned Priority, bool Conversation) { return !Conversation && Priority <= 3; }
	inline double Cooldown(double Base, unsigned Repetitions)
	{ return std::isfinite(Base) && Base >= 0.0 ? Base * (Repetitions >= 3u ? 4u : Repetitions + 1u) : 0.0; }
}
