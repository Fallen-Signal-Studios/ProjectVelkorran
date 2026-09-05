// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstddef>

namespace SovDamageChannels
{
// Semantic mixtures divide a packet, rather than letting one surviving tag restore immune damage.
// Missing channel weights are supplied as one by the GAS adapter; zero explicitly disables a portion.
struct FPortion { double Weight = 1.; bool Immune = false; double Multiplier = 1.; };
inline double Resolve(const FPortion* Portions, std::size_t Count)
{
    if (!Count) { return 1.; }
    double Total = 0., Accepted = 0.;
    for (std::size_t Index = 0; Index < Count; ++Index)
    {
        const FPortion& Part = Portions[Index];
        if (!std::isfinite(Part.Weight) || Part.Weight < 0. || !std::isfinite(Part.Multiplier) || Part.Multiplier < 0.) { return 0.; }
        Total += Part.Weight;
        if (!Part.Immune) { Accepted += Part.Weight * Part.Multiplier; }
    }
    return Total > 0. && std::isfinite(Total) && std::isfinite(Accepted) ? Accepted / Total : 0.;
}
}
