// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"
#include "MapTileRenderingComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class NARRATIVEARSENAL_API UMapTileRenderingComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UMapTileRenderingComponent();

	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};
