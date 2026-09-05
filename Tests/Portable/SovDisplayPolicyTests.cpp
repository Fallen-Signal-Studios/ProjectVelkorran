#include "Settings/SovDisplayPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>
int main()
{
    using namespace SovDisplayPolicy;
    unsigned Checks = 0;
    for (double Black : {.000001, .0001, .01, 1.})
    for (int White = 80; White <= 500; ++White)
    {
        assert(ValidCalibration(Black, White, White));
        assert(std::abs(std::pow(10., BlackLog10(Black)) - Black) < 1.e-10);
        assert(std::abs(GrayNits(White) / .18 - White) < 1.e-10); Checks += 3;
    }
    for (double Bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -1., 0.})
    {
        assert(!ValidCalibration(Bad, 200., 200.)); assert(!ValidCalibration(.01, Bad, 200.));
        assert(!ValidCalibration(.01, 200., Bad)); Checks += 3;
    }
    const Rect Left{-1920., 0., 0., 1080.}, Right{0., 0., 1920., 1080.};
    assert(IntersectionArea(Left, Right) == 0.);
    assert(IntersectionArea({-1000., 50., -10., 800.}, Left) > 0.);
    assert(IntersectionArea({-1000., 50., -10., 800.}, Right) == 0.);
    assert(IntersectionArea({-50., 0., 50., 100.}, Left) == IntersectionArea({-50., 0., 50., 100.}, Right));
    assert(IntersectionArea({0., 0., 0., 0.}, Left) == 0.);
    for (int X = -2500; X < 2500; ++X)
    {
        const Rect Window{static_cast<double>(X), 50., static_cast<double>(X + 100), 150.};
        const double A = IntersectionArea(Window, Left), B = IntersectionArea(Window, Right);
        assert(A >= 0. && B >= 0. && A + B <= 10000.); ++Checks;
    }
    std::cout << "PASS: " << Checks << " display calibration and physical viewport overlap policy checks\n";
}
