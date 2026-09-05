// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Feedback/SovHapticPolicy.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
	using namespace SovHapticPolicy;
	Mixer M;
	const auto Low = M.Play(0, .8f, 2.f, 10, 0.);
	const auto High = M.Play(0, .4f, 1.f, 80, 0.);
	assert(Low && High && M.Output(0, .5f, .5f, .5) == .1f);
	assert(M.Output(1, 1.f, 1.f, .5) == 0.f);
	assert(M.Output(0, 1.f, 1.f, 1.) == .8f); // Expiry exposes the lower priority request, not silence.
	assert(M.Cancel(High) && !M.Cancel(High));
	const auto Equal = M.Play(0, .9f, 1.f, 10, .5);
	assert(Equal && M.Output(0, 1.f, 1.f, .5) == .9f);
	assert(M.Output(0, 0.f, 1.f, .5) == 0.f);
	assert(M.Output(0, 1.f, 0.f, .5) == 0.f);
	M.Clear();
	const auto Fresh = M.Play(0, 1.f, 1.f, 10, 0.);
	assert(Fresh > Equal && !M.Cancel(Low) && M.Count() == 1);
	M.CancelChannel(0); assert(M.Count() == 0);
	for (unsigned I = 0; I < Capacity; ++I) { assert(M.Play(I % ChannelCount, .5f, 5.f, 0, 0.)); }
	assert(M.Count() == Capacity && !M.Play(0, 1.f, 1.f, 100, 0.));
	M.Expire(5.); assert(M.Count() == 0);
	const float Bad[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.f, 1.01f};
	for (float Value : Bad)
	{
		assert(!M.Play(0, Value, 1.f, 0, 0.));
		assert(M.Output(0, Value, 1.f, 0.) == 0.f);
	}
	assert(!M.Play(5, 1.f, 1.f, 0, 0.));
	assert(!M.Play(0, 1.f, 0.f, 0, 0.));
	assert(!M.Play(0, 1.f, 5.01f, 0, 0.));
	assert(!M.Play(0, 1.f, 1.f, -1, 0.));
	assert(!M.Play(0, 1.f, 1.f, 101, 0.));
	assert(!M.Play(0, 1.f, 1.f, 0, -1.));
	assert(!M.Play(0, 1.f, 1.f, 0, std::numeric_limits<double>::quiet_NaN()));
	assert(!M.Play(0, 1.f, 1.f, 0, std::numeric_limits<double>::max()));
	for (unsigned I = 0; I < 5000; ++I)
	{
		const double Now = I * .01;
		M.Play(I % ChannelCount, (I % 101) / 100.f, .1f, I % 101, Now);
		for (unsigned Channel = 0; Channel < ChannelCount; ++Channel)
		{
			const float Output = M.Output(Channel, .7f, .5f, Now);
			assert(std::isfinite(Output) && Output >= 0.f && Output <= .35f);
		}
		assert(M.Count() <= Capacity);
	}
	M.Expire(std::numeric_limits<double>::quiet_NaN()); assert(M.Count() == 0);
	std::cout << "PASS: haptic priority, exact cancellation, expiry, accessibility scales, fixed capacity and 5000 mixed requests\n";
}
