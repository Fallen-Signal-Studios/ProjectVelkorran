#include "Settings/SovSettingsPolicy.h"
#include "Sovereign/SovLookInputPolicy.h"
#include "Diagnostics/SovDiagnosticsPolicy.h"
#include <cassert>
#include <limits>
#include <string>
#include <iostream>
int main()
{
	using namespace SovSettingsPolicy;
	assert(ValidGameplay(1, 1.f, 1.f, false));
	assert(!ValidGameplay(3, 1.f, 1.f, false));
	assert(ValidGameplay(3, 1.f, .7f, true));
	assert(!ValidGameplay(5, 1.f, 1.f, true));
	assert(!ValidGameplay(1, 0.f, 1.f, true));
	assert(!ValidGameplay(1, std::numeric_limits<float>::quiet_NaN(), 1.f, true));
	assert(!ValidGameplay(1, 1.f, std::numeric_limits<float>::infinity(), true));
	assert(ValidAssists(1.f, 1.f, 0.f, 0.f, 0.f));
	assert(ValidAssists(2.f, .1f, .2f, 1.f, 1.f));
	assert(!ValidAssists(.99f, 1.f, 0.f, 0.f, 0.f));
	assert(!ValidAssists(1.f, 1.01f, 0.f, 0.f, 0.f));
	assert(!ValidAssists(1.f, 1.f, .21f, 0.f, 0.f));
	assert(!ValidAssists(1.f, 1.f, 0.f, -1.f, 0.f));
	assert(!ValidAssists(1.f, 1.f, 0.f, 0.f, 1.1f));
	using namespace SovDiagnosticsPolicy;
	for (std::size_t Count = 0; Count < 10000; ++Count) { assert(DropOldestBeforeAppend(Count) == (Count >= 512)); }
	const std::string Valid = "M01.Courtyard_Guard-2";
	assert(SafeId(Valid.c_str(), Valid.size()));
	for (const std::string& Bad : std::initializer_list<std::string>{"name@example.com", "player name", "../../saved", "x\ty", "line\nbreak", std::string(97, 'a')})
	{ assert(!SafeId(Bad.c_str(), Bad.size())); }
	assert(SafeId(static_cast<const char*>(nullptr), 0));
	assert(!SafeId(static_cast<const char*>(nullptr), 1));
	for (int Fps : {30, 60, 120})
	{
		const float Delta = 1.f / Fps;
		float Elapsed = 0.f; double Rotation = 0.0;
		for (int Frame = 0; Frame < Fps; ++Frame)
		{
			Rotation += 60.0 * Delta * SovLookInputPolicy::AverageAcceleration(Elapsed, Delta, .5f);
			Elapsed = std::min(.5f, Elapsed + Delta);
		}
		assert(std::abs(Rotation - 48.75) < .0001);
	}
	std::cout << "Settings/diagnostics/input policy: 10026 checks passed\n";
}
