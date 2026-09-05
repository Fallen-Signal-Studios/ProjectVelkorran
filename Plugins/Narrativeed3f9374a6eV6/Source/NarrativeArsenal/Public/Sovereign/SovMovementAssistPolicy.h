// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace SovMovementAssistPolicy
{
inline bool WantsAutomaticSprint(bool Enabled, float InputMagnitude, bool Grounded, bool GameplayAvailable)
{
	return Enabled && Grounded && GameplayAvailable && std::isfinite(InputMagnitude) && InputMagnitude >= .85f;
}
}
