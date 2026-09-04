// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <algorithm>
#include <cmath>

namespace SovSelenePayloadMath
{
	inline bool IsInCenterline(double OffsetX, double OffsetY, double ForwardX, double ForwardY, double FullWidth)
	{
		if (!std::isfinite(OffsetX) || !std::isfinite(OffsetY) || !std::isfinite(ForwardX)
			|| !std::isfinite(ForwardY) || !std::isfinite(FullWidth) || FullWidth < 0.0) { return false; }
		const double Length = std::hypot(ForwardX, ForwardY);
		return Length > 1.e-8 && std::abs(OffsetX * ForwardY - OffsetY * ForwardX) / Length <= FullWidth * 0.5;
	}
	inline bool ShouldRecall(double Elapsed, double Distance, double MaximumDuration, double MaximumDistance)
	{
		return !std::isfinite(Elapsed) || !std::isfinite(Distance) || !std::isfinite(MaximumDuration)
			|| !std::isfinite(MaximumDistance) || MaximumDuration <= 0.0 || MaximumDistance <= 0.0
			|| Elapsed >= MaximumDuration || Distance >= MaximumDistance;
	}
	inline double RemainingStep(double Speed, double Delta, double Travelled, double Range)
	{
		if (!std::isfinite(Speed) || !std::isfinite(Delta) || !std::isfinite(Travelled)
			|| !std::isfinite(Range) || Speed < 0.0 || Delta < 0.0 || Travelled < 0.0 || Range <= 0.0) { return 0.0; }
		return std::min(Speed * Delta, std::max(0.0, Range - Travelled));
	}
}
