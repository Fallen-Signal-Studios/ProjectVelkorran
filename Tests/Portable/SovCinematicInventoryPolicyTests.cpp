// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Items/NarrativeCinematicTransactionPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
    using namespace NarrativeCinematicTransactionPolicy;
    unsigned Checks = 0;
    const auto Check = [&](bool Value) { ++Checks; assert(Value); };
    for (int Maximum = 1; Maximum <= 100; ++Maximum)
    {
        for (int Quantity = -1; Quantity <= 102; ++Quantity)
        {
            Check(ValidQuantity(Quantity, Maximum) == (Quantity >= 1 && Quantity <= Maximum));
            Check(DedicatedStackFits(Quantity, Maximum, 4, 5, 10., 2., 50.) == (Quantity >= 1 && Quantity <= Maximum && Quantity <= 20));
            Check(!DedicatedStackFits(Quantity, Maximum, 5, 5, 0., 0., 100.));
        }
    }
    Check(ValidQuantity(100000, 100000)); Check(!ValidQuantity(100001, 100001));
    Check(!ValidQuantity(std::numeric_limits<int>::max(), std::numeric_limits<int>::max()));
    Check(DedicatedStackFits(1, 1, 0, 1, 0., 0., 0.));
    for (double Bad : {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), -1.})
    {
        Check(!DedicatedStackFits(1, 1, 0, 1, Bad, 1., 100.));
        Check(!DedicatedStackFits(1, 1, 0, 1, 0., Bad, 100.));
        Check(!DedicatedStackFits(1, 1, 0, 1, 0., 1., Bad));
    }
    for (std::uint64_t Revision = 0; Revision < 1000; ++Revision)
    { Check(OwnsWrite(Revision, Revision)); Check(!OwnsWrite(Revision + 1, Revision)); }
    Check(!OwnsWrite(0, std::numeric_limits<std::uint64_t>::max()));
    std::cout << "Cinematic inventory production policy: " << Checks << " assertions passed\n";
}
