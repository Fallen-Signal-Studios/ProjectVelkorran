// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovResourceSnapshotPolicy
{
inline bool ValidBase(double Base, double MaximumBase)
{
    return std::isfinite(Base) && std::isfinite(MaximumBase) && MaximumBase >= 0.;
}
// A retuned lower maximum caps the underlying resource. An unchanged maximum
// preserves even a clamped aggregate's hidden base, so effect expiry cannot drift.
inline double RestoredBase(double Base, double SavedMaximum, double CurrentMaximum)
{
    return CurrentMaximum < SavedMaximum ? std::min(Base, CurrentMaximum) : Base;
}
inline double Resolved(double Base, double Additive, double Maximum)
{
    return std::clamp(Base + Additive, 0., Maximum);
}
inline bool CanRestoreLegacy(bool HasContinuousCurrentModifier)
{
    return !HasContinuousCurrentModifier;
}
}
