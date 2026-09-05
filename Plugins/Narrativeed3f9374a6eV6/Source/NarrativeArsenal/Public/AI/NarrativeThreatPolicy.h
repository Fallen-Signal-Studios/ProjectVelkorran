// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

// Engine-independent policy used by the existing Narrative controller.
namespace NarrativeThreat
{
	inline bool ValidObservation(double Now, double Strength, double Confidence, double Lifetime)
	{
		return std::isfinite(Now) && Now >= 0.0 && std::isfinite(Strength) && Strength > 0.0
			&& std::isfinite(Confidence) && Confidence > 0.0 && Confidence <= 1.0
			&& std::isfinite(Lifetime) && Lifetime > 0.0 && Lifetime <= 60.0;
	}
	inline double ConfidenceAt(double Initial, double ObservedAt, double ExpiresAt, double Now)
	{
		if (!std::isfinite(Initial) || !std::isfinite(ObservedAt) || !std::isfinite(ExpiresAt)
			|| !std::isfinite(Now) || Initial <= 0.0 || Initial > 1.0 || Now < ObservedAt
			|| ExpiresAt <= ObservedAt || Now >= ExpiresAt) { return 0.0; }
		return Initial * (ExpiresAt - Now) / (ExpiresAt - ObservedAt);
	}
	inline bool CanDirectTarget(double Confidence, bool Direct, bool Cloaked)
	{
		return Direct && !Cloaked && std::isfinite(Confidence) && Confidence >= 0.65 && Confidence <= 1.0;
	}
	inline double SharedLifetime(double SenderExpiry, double Now, double Maximum)
	{
		if (!std::isfinite(SenderExpiry) || !std::isfinite(Now) || !std::isfinite(Maximum)
			|| Maximum <= 0.0 || SenderExpiry <= Now) { return 0.0; }
		return std::min(SenderExpiry - Now, Maximum);
	}
	inline double Score(double Strength, double Confidence)
	{
		return std::isfinite(Strength) && Strength > 0.0 && std::isfinite(Confidence)
			&& Confidence > 0.0 && Confidence <= 1.0 ? std::min(Strength, 10.0) * Confidence : 0.0;
	}
}
