// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstdint>
namespace NarrativeCinematicTransactionPolicy
{
    inline bool ValidQuantity(int Quantity, int Maximum)
    { return Quantity >= 1 && Quantity <= 100000 && Maximum >= 1 && Quantity <= Maximum; }
    inline bool DedicatedStackFits(int Quantity, int Maximum, int Stacks, int Capacity,
        double CurrentWeight, double UnitWeight, double WeightCapacity)
    {
        return ValidQuantity(Quantity, Maximum) && Stacks >= 0 && Capacity > Stacks
            && std::isfinite(CurrentWeight) && CurrentWeight >= 0.0
            && std::isfinite(UnitWeight) && UnitWeight >= 0.0
            && std::isfinite(WeightCapacity) && WeightCapacity >= 0.0
            && CurrentWeight + UnitWeight * Quantity <= WeightCapacity;
    }
    inline bool OwnsWrite(std::uint64_t Current, std::uint64_t Expected)
    { return Current == Expected; }
}
