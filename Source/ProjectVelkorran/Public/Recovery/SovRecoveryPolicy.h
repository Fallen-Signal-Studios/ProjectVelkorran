// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>

namespace SovRecoveryPolicy
{
	constexpr double DecisionDelaySeconds = .05;
	constexpr double RetryDelaySeconds = .75;
	constexpr double RescueProtectionSeconds = 1.0;
	constexpr double CheckpointProtectionSeconds = 1.5;
	inline bool CanRescue(bool OwnsFatalPawn, bool ActiveEncounter, bool Permitted, bool AllowedDifficulty,
		bool AlreadyUsed, bool VerifiedCombatFatal, bool Environmental, bool CompanionReady, bool SafePosition)
	{
		return OwnsFatalPawn && ActiveEncounter && Permitted && AllowedDifficulty && !AlreadyUsed
			&& VerifiedCombatFatal && !Environmental && CompanionReady && SafePosition;
	}
	inline double RescueHealth(double Maximum)
	{ return std::isfinite(Maximum) && Maximum > 0.0 ? Maximum * .35 : 0.0; }
}
