#include "Narrative/SovNarrativeCuePolicy.h"
#include <cassert>
#include <iostream>
int main()
{
	unsigned Checks = 0;
	for (unsigned Incoming = 0; Incoming < 8; ++Incoming)
	for (unsigned Current = 0; Current < 8; ++Current)
	{
		assert(SovNarrativeCuePolicy::MayInterrupt(Incoming, Current) == (Incoming < 6 && Current < 6 && Incoming < Current)); ++Checks;
	}
	for (unsigned Priority = 0; Priority < 6; ++Priority)
	{
		assert(!SovNarrativeCuePolicy::MayPlayInCombat(Priority, true)); ++Checks;
		assert(SovNarrativeCuePolicy::MayPlayInCombat(Priority, false) == (Priority <= 3)); ++Checks;
	}
	assert(SovNarrativeCuePolicy::Cooldown(8., 0) == 8.); ++Checks;
	assert(SovNarrativeCuePolicy::Cooldown(8., 1) == 16.); ++Checks;
	assert(SovNarrativeCuePolicy::Cooldown(8., 99) == 32.); ++Checks;
	assert(SovNarrativeCuePolicy::Cooldown(-2., 0) == 0.); ++Checks;
	std::cout << "Narrative speech policy: " << Checks << " assertions passed\n";
}
