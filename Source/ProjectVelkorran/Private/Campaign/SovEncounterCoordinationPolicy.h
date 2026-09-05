// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
#include <algorithm>
namespace SovEncounterCoordinationPolicy
{
	inline bool HasSlot(int Current, int Capacity) { return Current >= 0 && Capacity > 0 && Current < Capacity; }
	inline bool LowResources(double Health, double MaxHealth, double Shield, double MaxShield, double Stamina, double MaxStamina)
	{
		if (!std::isfinite(Health) || !std::isfinite(MaxHealth) || !std::isfinite(Shield) || !std::isfinite(MaxShield)
			|| !std::isfinite(Stamina) || !std::isfinite(MaxStamina) || MaxHealth <= 0. || Health <= 0.
			|| Shield < 0. || MaxShield < 0. || Stamina < 0. || MaxStamina < 0.) { return false; }
		return Health / MaxHealth <= .25 || (MaxShield > 0. && MaxStamina > 0.
			&& Shield / MaxShield <= .1 && Stamina / MaxStamina <= .1);
	}
	inline bool WarningReady(bool OnScreen, bool RequiresWarning, bool Acknowledged, double Now, double WarningAt, double Lead)
	{
		return OnScreen || !RequiresWarning || (Acknowledged && std::isfinite(Now) && std::isfinite(WarningAt)
			&& std::isfinite(Lead) && Lead >= 0. && Now >= WarningAt + Lead);
	}
	inline int Slots(int Normal, bool Relief) { return Relief ? std::min(Normal, 1) : Normal; }
}
