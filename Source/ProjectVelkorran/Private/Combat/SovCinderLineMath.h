// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace SovCinderLine
{
/** Endpoints included; a fixed cap bounds work even with malformed content. */
inline int NodeCount(double Length, double Spacing)
{
	if (!std::isfinite(Length) || !std::isfinite(Spacing) || Length < 0.0 || Spacing <= 0.0) { return 0; }
	if (Length <= 0.001) { return 1; }
	const double Intervals = std::ceil(Length / Spacing);
	return Intervals >= 63.0 ? 64 : static_cast<int>(Intervals) + 1;
}
inline double NodeAlpha(int Index, int Count)
{
	if (Count <= 1 || Index <= 0) { return 0.0; }
	if (Index >= Count - 1) { return 1.0; }
	return static_cast<double>(Index) / static_cast<double>(Count - 1);
}
inline bool ValidTiming(double Release, double Recovery, double Lifetime)
{
	return std::isfinite(Release) && std::isfinite(Recovery) && std::isfinite(Lifetime)
		&& Release >= 0.0 && Recovery >= 0.0 && Lifetime >= Release + Recovery;
}
}
