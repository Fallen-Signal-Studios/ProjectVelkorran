// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <cstdint>
namespace SovCampaignMassPolicy
{
	constexpr bool CanTransition(unsigned State, bool Authority, bool Boundary, bool OptIn,
		bool Busy, bool Pending, bool Defeated, unsigned Tier)
	{ return State == 1u && Authority && Boundary && OptIn && !Busy && !Pending && !Defeated && Tier <= 2u; }
	constexpr bool SameReceipt(std::uint64_t Expected, std::uint64_t Observed, bool Owner, bool Entity, bool Identity)
	{ return Expected != 0u && Expected == Observed && Owner && Entity && Identity; }
	inline bool ValidRoute(unsigned Count, double Speed)
	{ return Count <= 128u && std::isfinite(Speed) && Speed >= 0. && Speed <= 1200.; }
	inline bool TransferableEffect(double Duration, double Period, bool SelfSource)
	{ return std::isfinite(Duration) && Duration == -1. && std::isfinite(Period) && Period == 0. && SelfSource; }
}
