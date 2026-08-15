// Copyright Narrative Tools 2025.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Navigation/MapMarker.h"
#include "NarrativeWidgetHelpers.generated.h"

class UNarrativeNavigationComponent;

/// collection of functions that simplify or expose extra widget functionality
UCLASS()
class NARRATIVEARSENAL_API UNarrativeWidgetHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// takes a widgets location position and converts local position from the start geometry to the end geometry local space
	UFUNCTION(BlueprintPure, Category="User Interface|Geometry")
	static FVector2D TransformLocalSpace(const FGeometry& ToGeometry, const FGeometry& FromGeometry, const FVector2D& LocalPosition);

	UFUNCTION(BlueprintPure, Category="User Interface|Geometry", meta=(ReturnDisplayName="Paint Local Position", Keywords="world location paint"))
	static FVector2D WorldLocationToPaint(UPARAM(ref) FMarkerOnPaintData& OnPaintData, const FVector& WorldLocation);
	
};
