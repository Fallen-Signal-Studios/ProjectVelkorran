// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "NPCSpawnVisualizerComponent.generated.h"


UCLASS(ClassGroup=(Custom))
class NARRATIVEARSENAL_API UNPCSpawnVisualizerComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UNPCSpawnVisualizerComponent();

#if UE_ENABLE_DEBUG_DRAWING
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
#endif
};
