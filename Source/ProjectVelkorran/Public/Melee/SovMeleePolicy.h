// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>
namespace SovMelee
{
inline bool ValidWindows(double Startup,double Active,double Recovery,double BranchOpen,double BranchClose)
{
    return std::isfinite(Startup)&&std::isfinite(Active)&&std::isfinite(Recovery)&&std::isfinite(BranchOpen)&&std::isfinite(BranchClose)
        && Startup>=0. && Active>0. && Recovery>=0. && Startup+Active+Recovery<=5.
        && BranchOpen+1.e-6>=Startup+Active && BranchClose+1.e-6>=BranchOpen && BranchClose<=Startup+Active+Recovery+1.e-6;
}
inline bool ValidFollowUp(int Current,int Next,int Count)
{ return Next>Current && Next<Count && Count>0 && Count<=8; }
inline int SpatialSamples(double Length,double Radius)
{
    if (!std::isfinite(Length)||!std::isfinite(Radius)||Length<0.||Length>500.||Radius<2.||Radius>60.) { return 0; }
    return static_cast<int>(std::ceil(Length/Radius))+1;
}
inline double AimCorrection(double DesiredDegrees,double MaximumDegrees,double Strength)
{
    if (!std::isfinite(DesiredDegrees)||!std::isfinite(MaximumDegrees)||!std::isfinite(Strength)) { return 0.; }
    return std::clamp(DesiredDegrees,-std::clamp(MaximumDegrees,0.,25.),std::clamp(MaximumDegrees,0.,25.))*std::clamp(Strength,0.,1.);
}
inline int TemporalSamples(double AngleDegrees,double Travel,double Radius)
{
    if (!std::isfinite(AngleDegrees)||!std::isfinite(Travel)||!std::isfinite(Radius)
        ||AngleDegrees<0.||AngleDegrees>180.||Travel<0.||Travel>1500.||Radius<2.||Radius>60.) { return 0; }
    // Each spatial sample is itself continuously swept. Temporal subdivision additionally
    // follows component rotation while keeping worst-case collision work finite.
    return std::clamp(static_cast<int>(std::ceil(std::max(AngleDegrees/8.,Travel/(Radius*4.)))),1,24);
}
}
