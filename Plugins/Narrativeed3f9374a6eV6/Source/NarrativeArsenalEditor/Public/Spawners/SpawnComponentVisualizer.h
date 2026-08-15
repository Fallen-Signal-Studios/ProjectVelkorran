// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include <ComponentVisualizer.h>

/** Base class for clickable spline editing proxies */
struct HSpawnVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	HSpawnVisProxy(const UActorComponent* InComponent)
	: HComponentVisProxy(InComponent, HPP_Wireframe)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Hand;
	}
};

/**
 * Visualizer for InteractableComponent
 */
class NARRATIVEARSENALEDITOR_API FSpawnComponentVisualizer : public FComponentVisualizer
{
protected:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;

};

/**
 * Visualizer for InteractableComponent
 */
class NARRATIVEARSENALEDITOR_API FNPCSpawnComponentVisualizer : public FSpawnComponentVisualizer
{
protected:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

		/** Draw HUD on viewport for the supplied component */
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
};
