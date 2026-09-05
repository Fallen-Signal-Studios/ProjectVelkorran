// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace SovCarryPolicy
{
inline bool ValidBody(double Radius, double HalfHeight)
{ return std::isfinite(Radius) && std::isfinite(HalfHeight) && Radius > 0. && Radius <= 150. && HalfHeight >= Radius && HalfHeight <= 220.; }
inline bool CanRescue(bool CurrentCarrier, bool MatchingTarget, bool AtDestination, bool SafeRelease, bool BeatAvailable)
{ return CurrentCarrier && MatchingTarget && AtDestination && SafeRelease && BeatAvailable; }
}
