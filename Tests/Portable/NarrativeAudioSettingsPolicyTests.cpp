// Copyright Fallen Signal Studios. All Rights Reserved.
#include "../../Plugins/Narrativeed3f9374a6eV6/Source/NarrativeArsenal/Public/UnrealFramework/NarrativeAudioSettingsPolicy.h"
#include <cassert>
#include <limits>
int main()
{
	using namespace NarrativeAudioSettingsPolicy;
	assert(Volume(0.f) == 0.f); assert(Volume(1.f) == 1.f); assert(Volume(.375f) == .375f);
	assert(Volume(-9.f) == 0.f); assert(Volume(9.f) == 1.f);
	assert(Volume(std::numeric_limits<float>::quiet_NaN()) == 1.f);
	assert(Volume(std::numeric_limits<float>::infinity()) == 1.f);
	assert(Volume(-std::numeric_limits<float>::infinity(), .3f) == .3f);
	for (unsigned Range = 0; Range < 256; ++Range) { assert(ValidRange(Range) == (Range < 3)); }
	return 0;
}
