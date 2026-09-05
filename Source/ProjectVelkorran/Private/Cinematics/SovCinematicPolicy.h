// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace SovCinematicPolicy
{
    inline bool ValidProgress(double Previous, double Position, double Elapsed, double Rate)
    {
        return std::isfinite(Previous) && std::isfinite(Position) && std::isfinite(Elapsed) && std::isfinite(Rate)
            && Elapsed >= 0.0 && std::abs(Rate - 1.0) < .001 && Position >= Previous - .001
            && Position - Previous <= Elapsed + .15;
    }
    inline bool CompleteView(double Watched, double Duration, double Position, double End)
    {
        return std::isfinite(Watched) && std::isfinite(Duration) && std::isfinite(Position) && std::isfinite(End)
            && Duration > 0.0 && Duration <= 3600.0 && Watched >= Duration - .1
            && Position >= End - .1 && Position <= End + .1;
    }
    inline bool CapsulesOverlap(double DistanceSquaredXY, double DeltaZ, double RadiusSum, double HalfHeightSum)
    {
        return !std::isfinite(DistanceSquaredXY) || !std::isfinite(DeltaZ) || !std::isfinite(RadiusSum) || !std::isfinite(HalfHeightSum)
            || DistanceSquaredXY < 0.0 || RadiusSum <= 0.0 || HalfHeightSum <= 0.0
            || (DistanceSquaredXY < RadiusSum * RadiusSum && std::abs(DeltaZ) < HalfHeightSum);
    }
}
