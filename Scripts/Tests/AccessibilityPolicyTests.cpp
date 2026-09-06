// Production-used finite/layout/pressure/contrast boundaries; no claim of testing Slate or a display.
#include "UI/SovAccessibilityPolicy.h"
#include <cassert>
#include <limits>
#include <iostream>
int main()
{
	using namespace SovAccessibilityPolicy;
	assert(ValidLayout(1,1,.85f,42,3,2));
	assert(ValidLayout(2,2.5f,1,64,4,6));
	assert(!ValidLayout(.9f,1,1,42,3,2)); assert(!ValidLayout(1,2.6f,1,42,3,2));
	assert(!ValidLayout(1,1,1.1f,42,3,2)); assert(!ValidLayout(1,1,1,19,3,2));
	assert(!ValidLayout(1,1,1,65,3,2)); assert(!ValidLayout(1,1,1,42,0,2));
	assert(!ValidLayout(1,1,1,42,5,2)); assert(!ValidLayout(1,1,1,42,3,7));
	for (float Bad : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
	{
		assert(!ValidLayout(Bad,1,1,42,3,2)); assert(!ValidLayout(1,Bad,1,42,3,2));
		assert(!ValidLayout(1,1,Bad,42,3,2)); assert(!ValidLayout(1,1,1,42,3,Bad));
		assert(!ValidPressure(0,Bad,2)); assert(!ValidPressure(0,5,Bad)); assert(!ValidColor(Bad,1,1,1));
		assert(Pulse(Bad,true)==1.f);
	}
	for (uint8_t Mode=0;Mode<3;++Mode) { assert(ValidPressure(Mode,2,1)); assert(ValidPressure(Mode,30,5)); }
	assert(!ValidPressure(3,5,2)); assert(!ValidPressure(0,1,2)); assert(!ValidPressure(0,31,2)); assert(!ValidPressure(0,5,6));
	assert(ValidColor(0,1,.5f,1)); assert(!ValidColor(0,1,.5f,.5f)); assert(!ValidColor(-.1f,1,.5f,1)); assert(!ValidColor(1,1.1f,.5f,1));
	for (int Tick=0;Tick<10000;++Tick) { float Alpha=Pulse(Tick*.01f,true); assert(Alpha>=.6999f && Alpha<=1.0001f); assert(Pulse(Tick*.01f,false)==1.f); }
	std::cout << "PASS: accessibility finite/layout/timing/color boundaries and 10,000 slow-pulse samples\n";
}
