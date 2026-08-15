// Copyright Narrative Tools 2025.

#include "NarrativeWidgetHelpers.h"

FVector2D UNarrativeWidgetHelpers::TransformLocalSpace(const FGeometry& ToGeometry, const FGeometry& FromGeometry, const FVector2D& LocalPosition)
{
	return ToGeometry.AbsoluteToLocal(FromGeometry.LocalToAbsolute(LocalPosition));
}

FVector2D UNarrativeWidgetHelpers::WorldLocationToPaint(FMarkerOnPaintData& OnPaintData, const FVector& WorldLocation)
{
	const FVector2D MapLocalPosition = OnPaintData.MapOrigin - FVector2D(WorldLocation) + OnPaintData.MapPan;
	return TransformLocalSpace(OnPaintData.ParentGeometry, OnPaintData.MapGeometry, MapLocalPosition);
}
