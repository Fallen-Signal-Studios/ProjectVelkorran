// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace SovHapticPolicy
{
	constexpr unsigned ChannelCount = 5;
	constexpr unsigned Capacity = 16;
	inline bool Unit(float Value) { return std::isfinite(Value) && Value >= 0.f && Value <= 1.f; }
	struct Request
	{
		std::uint64_t Id = 0;
		unsigned Channel = 0;
		float Intensity = 0.f;
		int Priority = 0;
		double Expires = 0.;
	};
	/** Fixed storage and monotonic receipts. Higher priority wins within a channel; equal priority takes max. */
	class Mixer
	{
	public:
		std::uint64_t Play(unsigned Channel, float Intensity, float Duration, int Priority, double Now)
		{
			if (Channel >= ChannelCount || !Unit(Intensity) || Intensity == 0.f || !std::isfinite(Duration)
				|| Duration <= 0.f || Duration > 5.f || Priority < 0 || Priority > 100 || !std::isfinite(Now) || Now < 0.
				|| Next >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) { return 0; }
			const double Expires = Now + Duration;
			if (!std::isfinite(Expires) || Expires <= Now) { return 0; }
			Expire(Now);
			for (Request& Item : Requests)
			{
				if (!Item.Id) { Item = {++Next, Channel, Intensity, Priority, Expires}; return Item.Id; }
			}
			return 0;
		}
		bool Cancel(std::uint64_t Id)
		{
			if (!Id) { return false; }
			for (Request& Item : Requests) { if (Item.Id == Id) { Item = {}; return true; } }
			return false;
		}
		void CancelChannel(unsigned Channel) { for (Request& Item : Requests) { if (Item.Channel == Channel) { Item = {}; } } }
		void Clear() { Requests = {}; }
		void Expire(double Now)
		{
			for (Request& Item : Requests) { if (!std::isfinite(Now) || Now < 0. || Item.Expires <= Now) { Item = {}; } }
		}
		float Output(unsigned Channel, float Master, float Scale, double Now) const
		{
			if (Channel >= ChannelCount || !Unit(Master) || !Unit(Scale) || !std::isfinite(Now) || Now < 0.) { return 0.f; }
			int Priority = -1;
			float Intensity = 0.f;
			for (const Request& Item : Requests)
			{
				if (!Item.Id || Item.Channel != Channel || Item.Expires <= Now || Item.Priority < Priority) { continue; }
				if (Item.Priority > Priority || Item.Intensity > Intensity) { Intensity = Item.Intensity; }
				Priority = Item.Priority;
			}
			return Intensity * Master * Scale;
		}
		unsigned Count() const { unsigned Result = 0; for (const Request& Item : Requests) { if (Item.Id) { ++Result; } } return Result; }
		bool Contains(std::uint64_t Id) const { for (const Request& Item : Requests) { if (Id && Item.Id == Id) { return true; } } return false; }
	private:
		std::array<Request, Capacity> Requests{};
		std::uint64_t Next = 0;
	};
}
