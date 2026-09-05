// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include <cmath>
namespace SovEvidencePolicy
{
	inline bool CanAdvance(unsigned Current, unsigned Requested, bool IndependentSupport, bool AuthenticatedSource, bool ValidDestination, bool AlreadyCopied)
	{
		if (Current > 5 || Requested == 0 || Requested > 5) { return false; }
		if (Requested == 5) { return Current >= 4 && ValidDestination && !AlreadyCopied; }
		if (Requested != Current + 1) { return false; }
		return (Requested != 3 || IndependentSupport) && (Requested != 4 || AuthenticatedSource);
	}
	inline bool InScanCone(double Distance, double Range, double FacingDot, double MinimumDot)
	{
		return std::isfinite(Distance) && std::isfinite(Range) && std::isfinite(FacingDot) && std::isfinite(MinimumDot)
			&& Distance >= 0.0 && Range > 0.0 && Range <= 5000.0 && Distance <= Range
			&& MinimumDot >= 0.0 && MinimumDot <= 1.0 && FacingDot >= MinimumDot && FacingDot <= 1.0;
	}
}
