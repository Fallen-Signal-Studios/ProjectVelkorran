// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovDisplayPolicy
{
inline bool ValidCalibration(double Black, double White, double UI)
{
    return std::isfinite(Black) && Black >= .000001 && Black <= 1.
        && std::isfinite(White) && White >= 80. && White <= 500.
        && std::isfinite(UI) && UI >= 80. && UI <= 500.;
}
// UE's scene calibration is an 18%-gray output target, not its peak-output selector.
inline double GrayNits(double ReferenceWhite) { return ReferenceWhite * .18; }
inline double BlackLog10(double Black) { return std::log10(Black); }
struct Rect { double Left, Top, Right, Bottom; };
inline bool ValidRect(const Rect& R)
{
    return std::isfinite(R.Left) && std::isfinite(R.Top) && std::isfinite(R.Right)
        && std::isfinite(R.Bottom) && R.Right > R.Left && R.Bottom > R.Top
        && R.Left >= -1.e7 && R.Top >= -1.e7 && R.Right <= 1.e7 && R.Bottom <= 1.e7;
}
inline double IntersectionArea(const Rect& A, const Rect& B)
{
    if (!ValidRect(A) || !ValidRect(B)) { return 0.; }
    return std::max(0., std::min(A.Right, B.Right) - std::max(A.Left, B.Left))
        * std::max(0., std::min(A.Bottom, B.Bottom) - std::max(A.Top, B.Top));
}
}
