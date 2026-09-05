// Production-used playback proof and final-placement checks; no Unreal dependency.
#include "Cinematics/SovCinematicPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
    using namespace SovCinematicPolicy;
    unsigned Checks = 0;
    assert(ValidPartitionRegion(0, 0, 0, 100, 1)); ++Checks;
    assert(ValidPartitionRegion(1.e9, -1.e9, 1000, 200000, 16)); ++Checks;
    assert(!ValidPartitionRegion(0, 0, 0, 99.9, 1)); ++Checks;
    assert(!ValidPartitionRegion(0, 0, 0, 200001, 1)); ++Checks;
    assert(!ValidPartitionRegion(0, 0, 0, 100, 0)); ++Checks;
    assert(!ValidPartitionRegion(0, 0, 0, 100, 17)); ++Checks;
    assert(WitnessInsideRegion(10000, 100)); ++Checks;
    assert(!WitnessInsideRegion(10000.01, 100)); ++Checks;
    assert(!WitnessInsideRegion(-1, 100)); ++Checks;
    unsigned Observed = AdvanceReadyObservations(0, true);
    assert(Observed == 1); ++Checks; // One stale registration-frame query cannot authorize playback.
    Observed = AdvanceReadyObservations(Observed, false);
    assert(Observed == 0); ++Checks;
    Observed = AdvanceReadyObservations(AdvanceReadyObservations(Observed, true), true);
    assert(Observed == 2); ++Checks;
    assert(AdvanceReadyObservations(std::numeric_limits<unsigned>::max(), true) == 2); ++Checks;
    assert(!LoadingExpired(14.99, 15)); ++Checks;
    assert(LoadingExpired(15, 15)); ++Checks;
    assert(LoadingExpired(0, 0)); ++Checks;
    assert(LoadingExpired(-1, 15)); ++Checks;
    for (const double Step : {1.0 / 30.0, 1.0 / 60.0, .25, 1.0})
    {
        double Position = 0, Watched = 0;
        for (int I = 0; I < 300; ++I)
        {
            assert(ValidProgress(Position, Position + Step, Step, 1.0)); ++Checks;
            Position += Step; Watched += Step;
        }
        assert(CompleteView(Watched, Position, Position, Position)); ++Checks;
        assert(!CompleteView(Watched / 2.0, Position, Position, Position)); ++Checks;
        assert(!CompleteView(Watched, Position, Position - .5, Position)); ++Checks;
    }
    assert(ValidProgress(10, 10, 500, 1)); ++Checks; // A frozen player earns no watched progress.
    assert(!CompleteView(0, 10, 10, 10)); ++Checks;
    assert(!ValidProgress(1, 8, .016, 1)); ++Checks; // Seeking cannot mint a full viewing.
    assert(!ValidProgress(8, 1, 7, 1)); ++Checks;
    assert(!ValidProgress(1, 2, 1, 2)); ++Checks;
    assert(!ValidProgress(1, 2, -1, 1)); ++Checks;
    for (const double Bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        assert(!ValidPartitionRegion(Bad, 0, 0, 100, 1)); ++Checks;
        assert(!ValidPartitionRegion(0, Bad, 0, 100, 1)); ++Checks;
        assert(!ValidPartitionRegion(0, 0, Bad, 100, 1)); ++Checks;
        assert(!ValidPartitionRegion(0, 0, 0, Bad, 1)); ++Checks;
        assert(!WitnessInsideRegion(Bad, 100)); ++Checks;
        assert(!WitnessInsideRegion(0, Bad)); ++Checks;
        assert(LoadingExpired(Bad, 15)); ++Checks;
        assert(LoadingExpired(0, Bad)); ++Checks;
        assert(!ValidProgress(Bad, 1, 1, 1)); ++Checks;
        assert(!ValidProgress(0, Bad, 1, 1)); ++Checks;
        assert(!ValidProgress(0, 1, Bad, 1)); ++Checks;
        assert(!ValidProgress(0, 1, 1, Bad)); ++Checks;
        assert(!CompleteView(Bad, 1, 1, 1)); ++Checks;
        assert(!CompleteView(1, Bad, 1, 1)); ++Checks;
        assert(!CompleteView(1, 1, Bad, 1)); ++Checks;
        assert(!CompleteView(1, 1, 1, Bad)); ++Checks;
        assert(CapsulesOverlap(Bad, 0, 1, 2)); ++Checks;
        assert(CapsulesOverlap(0, Bad, 1, 2)); ++Checks;
    }
    assert(!CompleteView(10, 0, 10, 10)); ++Checks;
    assert(!CompleteView(4000, 4000, 4000, 4000)); ++Checks;
    for (int X = 0; X <= 100; ++X)
    {
        for (int Z = -100; Z <= 100; ++Z)
        {
            const bool Expected = X < 60 && Z > -80 && Z < 80;
            assert(CapsulesOverlap(double(X * X), Z, 60, 80) == Expected); ++Checks;
            assert(CapsulesOverlap(double(X * X), -Z, 60, 80) == Expected); ++Checks;
        }
    }
    assert(CapsulesOverlap(-1, 0, 1, 2)); ++Checks;
    assert(CapsulesOverlap(0, 0, 0, 2)); ++Checks;
    assert(CapsulesOverlap(0, 0, 1, -2)); ++Checks;
    std::cout << "Cinematic progress and capsule policy: " << Checks << " assertions passed\n";
}
