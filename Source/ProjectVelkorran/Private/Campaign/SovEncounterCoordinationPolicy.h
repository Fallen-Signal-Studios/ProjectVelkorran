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
	/**
	 * Whether a point at view-space offsets (Forward along the view, Right, Up) lies inside a frame with this
	 * horizontal field of view and aspect ratio. Used where the player's viewport is not on this machine.
	 */
	inline bool WithinViewFrame(double Forward, double Right, double Up, double HorizontalFovDegrees, double AspectRatio)
	{
		if (!std::isfinite(Forward) || !std::isfinite(Right) || !std::isfinite(Up) || !std::isfinite(HorizontalFovDegrees)
			|| !std::isfinite(AspectRatio) || Forward <= 0. || HorizontalFovDegrees <= 0. || HorizontalFovDegrees >= 180.
			|| AspectRatio <= 0.) { return false; }
		const double TanHalfWidth = std::tan(HorizontalFovDegrees * std::acos(-1.) / 360.);
		return std::abs(Right) <= Forward * TanHalfWidth && std::abs(Up) <= Forward * TanHalfWidth / AspectRatio;
	}
	/** The narrowest common display frame: a wider real view can only make the rule require a warning, never skip one. */
	inline constexpr double RemoteViewAspectRatio = 16. / 9.;
}
