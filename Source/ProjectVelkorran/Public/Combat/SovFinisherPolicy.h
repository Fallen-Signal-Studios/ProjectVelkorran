// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>
namespace SovFinisher
{
inline bool Vulnerable(bool Alive, bool PoiseBroken, double Health, double Maximum, double Threshold)
{
    return Alive && std::isfinite(Health) && std::isfinite(Maximum) && Health > 0. && Maximum > 0.
        && (PoiseBroken || (std::isfinite(Threshold) && Threshold > 0. && Threshold <= 1. && Health / Maximum <= Threshold));
}
inline double StrikeDamage(double Health, double Authored, bool Normal, bool Aligned)
{
    if (!std::isfinite(Health) || !std::isfinite(Authored) || Health <= 0. || Authored < 0.) { return 0.; }
    if (Normal && Aligned) { return Health; }
    return std::min(Authored, Normal ? Health : std::max(0., Health - 1.));
}
inline double Duration(double Authored, bool Aligned)
{
    return Aligned && std::isfinite(Authored) ? std::clamp(Authored, .8, 1.8) : .35;
}
}
