// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Resonance/SovResonancePolicy.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main()
{
    using namespace SovResonancePolicy;
    int checks = 0;
    for (unsigned bits = 0; bits < 16; ++bits)
    {
        const bool expected = bits == 6 || bits == 9;
        assert(Complementary(bits & 1, bits & 2, bits & 4, bits & 8) == expected); ++checks;
    }
    for (unsigned bits = 0; bits < 16; ++bits)
    {
        assert(OfferLive(1., 2., bits & 1, bits & 2, bits & 4, bits & 8) == (bits == 15)); ++checks;
    }
    assert(!OfferLive(2., 2., true,true,true,true));
    assert(!OfferLive(std::numeric_limits<double>::quiet_NaN(), 2.,true,true,true,true));
    assert(!OfferLive(1., std::numeric_limits<double>::infinity(),true,true,true,true));
    for (int percent = 15; percent <= 25; ++percent)
    {
        const float fraction = static_cast<float>(percent) / 100.f;
        float player = 0.f, ally = 0.f;
        assert(RemainingContribution(ally, player, fraction) == 0.f);
        for (int hit = 1; hit <= 1000; ++hit)
        {
            player += 1.f + static_cast<float>(hit % 37);
            const float room = RemainingContribution(ally, player, fraction);
            const float requested = 1.f + static_cast<float>(hit % 43);
            ally += std::fmin(room, requested);
            assert(ally / (player + ally) <= fraction + .00001f);
            assert(RemainingContribution(ally, player, fraction) >= 0.f); checks += 2;
        }
    }
    assert(RemainingContribution(1.f, 100.f, .5f) == 0.f);
    assert(RemainingContribution(-1.f, 100.f, .2f) == 0.f);
    assert(RemainingContribution(0.f, std::numeric_limits<float>::infinity(), .2f) == 0.f);
    float pressure = 0.f;
    for (int hit = 0; hit < 1000; ++hit) { pressure = AddPressure(pressure, 2.f, 100.f); assert(pressure >= 0.f && pressure <= 100.f); ++checks; }
    assert(pressure == 100.f); assert(AddPressure(pressure, -1.f, 100.f) == pressure);
    assert(!MayRecoverSeparation(2499.f * 2499.f, 0.f, false, true));
    assert(MayRecoverSeparation(2500.f * 2500.f, 2000.f * 2000.f, false, true));
    assert(!MayRecoverSeparation(2500.f * 2500.f, 2001.f * 2001.f, false, true));
    assert(!MayRecoverSeparation(2500.f * 2500.f, 0.f, true, true));
    assert(!MayRecoverSeparation(2500.f * 2500.f, 0.f, false, false));
    assert(!MayRecoverSeparation(std::numeric_limits<float>::infinity(), 0.f, false, true));
    std::cout << "PASS: " << checks << " Resonance/contribution policy checks plus expiry and hidden-recovery boundaries\n";
}
