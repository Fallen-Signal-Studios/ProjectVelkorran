// Exercises the production policy included by NarrativeThreatMemory.cpp.
#include "AI/NarrativeThreatPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
	using namespace NarrativeThreat;
	const double NaN = std::numeric_limits<double>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	assert(ValidObservation(0, 1, 1, 8));
	assert(!ValidObservation(0, 0, 1, 8));
	assert(!ValidObservation(0, 1, 0, 8));
	assert(!ValidObservation(0, 1, 1, 0));
	assert(!ValidObservation(-1, 1, 1, 8));
	assert(!ValidObservation(0, 1, 1.1, 8));
	assert(!ValidObservation(0, 1, 1, 60.01));
	for (double Invalid : {NaN, Infinity, -Infinity})
	{
		assert(!ValidObservation(Invalid, 1, 1, 8));
		assert(!ValidObservation(0, Invalid, 1, 8));
		assert(!ValidObservation(0, 1, Invalid, 8));
		assert(!ValidObservation(0, 1, 1, Invalid));
		assert(ConfidenceAt(Invalid, 0, 8, 1) == 0);
		assert(ConfidenceAt(1, Invalid, 8, 1) == 0);
		assert(ConfidenceAt(1, 0, Invalid, 1) == 0);
		assert(ConfidenceAt(1, 0, 8, Invalid) == 0);
		assert(!CanDirectTarget(Invalid, true, false));
		assert(SharedLifetime(Invalid, 0, 4) == 0);
		assert(Score(Invalid, 1) == 0);
	}
	assert(ConfidenceAt(1, 4, 12, 3) == 0); // Out-of-order clocks fail closed.
	assert(ConfidenceAt(1, 4, 4, 4) == 0);
	assert(ConfidenceAt(1, 4, 12, 12) == 0); // Exact expiry is forgotten.
	assert(ConfidenceAt(1, 4, 12, 4) == 1);
	assert(ConfidenceAt(1, 4, 12, 8) == .5);
	assert(CanDirectTarget(.65, true, false));
	assert(!CanDirectTarget(.64999, true, false));
	assert(!CanDirectTarget(1, false, false)); // Last known information is not direct sight.
	assert(!CanDirectTarget(1, true, true)); // Cloak blocks even perfect old confidence.
	assert(SharedLifetime(9, 8, 4) == 1);
	assert(SharedLifetime(7, 8, 4) == 0);
	assert(SharedLifetime(80, 8, 4) == 4);
	assert(Score(100, .5) == 5);
	int Checks = 0;
	for (int Lifetime = 1; Lifetime <= 60; ++Lifetime)
	{
		double Previous = 1.0;
		for (int Step = 0; Step <= Lifetime * 20; ++Step)
		{
			const double Now = 100 + Step / 20.0;
			const double Confidence = ConfidenceAt(1, 100, 100 + Lifetime, Now);
			assert(Confidence >= 0 && Confidence <= Previous);
			assert(!CanDirectTarget(Confidence, false, false));
			assert(!CanDirectTarget(Confidence, true, true));
			const double Shared = SharedLifetime(100 + Lifetime, Now, 4);
			assert(Shared >= 0 && Shared <= 4);
			assert(Shared == 0 || Now + Shared <= 100 + Lifetime); // Sharing cannot renew origin expiry.
			Previous = Confidence;
			++Checks;
		}
	}
	std::cout << "PASS: threat confidence, concealment, bounded strength and expiry, " << Checks << " decay/share boundaries\n";
}
