#include "Combat/SovAxiomPulseMath.h"
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
int Checks = 0;
void Check(bool Condition, const char* Description)
{
	++Checks;
	if (!Condition)
	{
		std::cerr << "FAIL: " << Description << '\n';
		std::exit(1);
	}
}
bool Near(float A, float B) { return std::abs(A - B) < 0.0001f; }
}

int main()
{
	using namespace SovAxiomPulse;
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const double Infinity = std::numeric_limits<double>::infinity();
	Check(ChargeAlpha(-1.f, .65f) == 0.f, "clock before activation is uncharged");
	Check(ChargeAlpha(0.f, .65f) == 0.f, "immediate release is uncharged");
	Check(Near(ChargeAlpha(.325f, .65f), .5f), "half charge uses elapsed time");
	Check(ChargeAlpha(.65f, .65f) == 1.f, "full-charge boundary");
	Check(ChargeAlpha(100.f, .65f) == 1.f, "late timer clamps full charge");
	Check(ChargeAlpha(NaN, .65f) == 0.f, "non-finite elapsed rejected");
	Check(ChargeAlpha(.2f, 0.f) == 0.f, "zero charge duration rejected");
	Check(ChargeAlpha(.2f, -1.f) == 0.f, "negative charge duration rejected");
	Check(ChargeAlpha(.2f, NaN) == 0.f, "non-finite duration rejected");
	Check(Lerp(1500.f, 4000.f, 0.f) == 1500.f, "tap range");
	Check(Lerp(1500.f, 4000.f, 1.f) == 4000.f, "full range");
	Check(Lerp(1500.f, 4000.f, .5f) == 2750.f, "half range");
	Check(Near(Lerp(8.f, 22.5f, .5f), 15.25f), "half cone");
	Check(Near(Lerp(1.5f, 4.f, .5f), 2.75f), "half suppression duration");
	Check(Lerp(1.f, 2.f, -1.f) == 1.f && Lerp(1.f, 2.f, 2.f) == 2.f, "interpolation clamps");
	Check(Lerp(2.f, 1.f, .5f) == 0.f, "inverted tuning rejected");
	Check(Lerp(NaN, 2.f, .5f) == 0.f && Lerp(1.f, 2.f, NaN) == 0.f, "non-finite interpolation rejected");
	Check(ContainsPoint(1500, 0, 0, 1, 0, 0, 1500, 8), "inclusive range edge");
	Check(!ContainsPoint(1500.01, 0, 0, 1, 0, 0, 1500, 8), "beyond range rejected");
	Check(!ContainsPoint(-100, 0, 0, 1, 0, 0, 1500, 8), "behind source rejected");
	Check(ContainsPoint(100, 0, 0, 100, 0, 0, 1500, 8), "forward vector normalized");
	Check(ContainsPoint(0, 0, 0, 1, 0, 0, 1500, 8), "coincident target included");
	Check(!ContainsPoint(0, 0, 0, 0, 0, 0, 1500, 8), "zero direction rejected even at origin");
	const double Angle = 22.5 * 0.017453292519943295769;
	Check(ContainsPoint(1000 * std::cos(Angle), 1000 * std::sin(Angle), 0, 1, 0, 0, 4000, 22.5), "inclusive cone edge");
	Check(!ContainsPoint(1000 * std::cos(Angle + .001), 1000 * std::sin(Angle + .001), 0, 1, 0, 0, 4000, 22.5), "outside cone rejected");
	Check(ContainsPoint(0, 0, 1000, 0, 0, 1, 1500, 8), "vertical aim");
	Check(!ContainsPoint(100, 0, 0, 1, 0, 0, 0, 8), "zero range rejected");
	Check(!ContainsPoint(100, 0, 0, 1, 0, 0, 1500, -1), "negative cone rejected");
	Check(!ContainsPoint(100, 0, 0, 1, 0, 0, 1500, 181), "invalid cone rejected");
	Check(!ContainsPoint(Infinity, 0, 0, 1, 0, 0, 1500, 8), "infinite target rejected");
	Check(!ContainsPoint(100, 0, 0, Infinity, 0, 0, 1500, 8), "infinite aim rejected");
	Check(!ContainsPoint(100, 0, 0, 1, 0, 0, Infinity, 8), "infinite range rejected");
	Check(!ContainsPoint(100, 0, 0, 1, 0, 0, 1500, NaN), "non-finite cone rejected");
	Check(!ContainsPoint(1500, 400, 0, 1, 0, 0, 4000, 8) &&
		ContainsPoint(1500, 400, 0, 1, 0, 0, 4000, 22.5), "charging widens selected targets");
	std::cout << "PASS: " << Checks << " production Axiom charge/geometry checks\n";
}
