// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovWorldMotionPolicy
{
inline bool ValidDuration(double Seconds) { return std::isfinite(Seconds) && Seconds >= .1 && Seconds <= 30.; }
inline double Advance(double Elapsed, double Delta, double Duration)
{
    if (!ValidDuration(Duration) || !std::isfinite(Elapsed) || Elapsed < 0. || !std::isfinite(Delta) || Delta <= 0.) { return Elapsed; }
    return std::min(Duration, Elapsed + Delta);
}
inline bool TimedOut(double Now, double Started, double Limit)
{ return !std::isfinite(Now) || !std::isfinite(Started) || !std::isfinite(Limit) || Limit <= 0. || Now < Started || Now - Started >= Limit; }
inline bool CanSaveEndpoint(bool Mutating, bool Moving, bool Waiting, bool AtStableEndpoint)
{ return !Mutating && !Moving && !Waiting && AtStableEndpoint; }
inline bool CanEnableLink(bool Stable, bool Healthy, bool Powered, bool Unlocked)
{ return Stable && Healthy && Powered && Unlocked; }
}
