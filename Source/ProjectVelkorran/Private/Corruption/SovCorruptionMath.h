// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovCorruptionMath
{
	inline bool ValidThresholds(double Trace, double Intrusion, double Contest, double Overwrite, double Hysteresis)
	{
		return std::isfinite(Trace) && std::isfinite(Intrusion) && std::isfinite(Contest) && std::isfinite(Overwrite)
			&& std::isfinite(Hysteresis) && Trace > 0.0 && Trace < Intrusion && Intrusion < Contest
			&& Contest < Overwrite && Overwrite <= 100.0 && Hysteresis >= 0.0
			&& Hysteresis < std::min(Intrusion - Trace, std::min(Contest - Intrusion, Overwrite - Contest));
	}
	inline int Band(double Exposure, int Previous, double Trace, double Intrusion, double Contest, double Overwrite, double Hysteresis)
	{
		if (!std::isfinite(Exposure) || !ValidThresholds(Trace, Intrusion, Contest, Overwrite, Hysteresis)) { return 0; }
		Exposure = std::clamp(Exposure, 0.0, 100.0);
		int Result = std::clamp(Previous, 0, 4);
		const double Thresholds[] = {0.0, Trace, Intrusion, Contest, Overwrite};
		while (Result < 4 && Exposure >= Thresholds[Result + 1]) { ++Result; }
		while (Result > 0 && (Exposure <= 0.0 || Exposure < std::max(0.0, Thresholds[Result] - Hysteresis))) { --Result; }
		return Result;
	}
	inline double AddCapped(double Current, double Amount, double Cap)
	{
		if (!std::isfinite(Current) || !std::isfinite(Amount) || !std::isfinite(Cap) || Amount < 0.0) { return 0.0; }
		Current = std::clamp(Current, 0.0, 100.0);
		return std::max(0.0, std::min(Amount, std::clamp(Cap, 0.0, 100.0) - Current));
	}
	inline double Falloff(double Distance, double Radius, bool Linear)
	{
		if (!std::isfinite(Distance) || !std::isfinite(Radius) || Distance < 0.0 || Radius <= 0.0 || Distance > Radius) { return 0.0; }
		return Linear ? std::clamp(1.0 - Distance / Radius, 0.0, 1.0) : 1.0;
	}
}
