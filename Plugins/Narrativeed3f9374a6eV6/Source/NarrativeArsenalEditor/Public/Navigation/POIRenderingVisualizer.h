// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

class APOIActor;

struct HPOIVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	TWeakObjectPtr<AActor> OtherPOI;

	HPOIVisProxy(const UActorComponent* InComponent, AActor* InOtherPOI)
	: HComponentVisProxy(InComponent, HPP_Wireframe)
	{
		OtherPOI = InOtherPOI;
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Hand;
	}
};

struct FPOILink
{
	FPOILink()
	{
		POIActor = nullptr;
		OtherPOIActor = nullptr;
	};
	FPOILink(const TWeakObjectPtr<APOIActor>& InPOIActor, const TWeakObjectPtr<APOIActor>& InOtherPOIActor)
	{
		POIActor = InPOIActor;
		OtherPOIActor = InOtherPOIActor;
	}

	FPOILink(APOIActor* InPOIActor, APOIActor* InOtherPOIActor);

	bool IsValid() const
	{
		return POIActor.IsValid() && OtherPOIActor.IsValid();
	}
	
	TWeakObjectPtr<APOIActor> POIActor;
	TWeakObjectPtr<APOIActor> OtherPOIActor;
};


struct HPOILinkVisProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY();

	FPOILink POILink;

	HPOILinkVisProxy(const UActorComponent* InComponent, const FPOILink& InPOILink)
	: HComponentVisProxy(InComponent, HPP_Wireframe)
	{
		POILink = InPOILink;
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Hand;
	}
};

class NARRATIVEARSENALEDITOR_API FPOIRenderingVisualizer : public FComponentVisualizer
{
protected:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	void ConnectPOI();
	void TwoWayConnectPOI();
	void TwoWayDisconnectPOI();
	void DisconnectPOI();
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual UActorComponent* GetEditedComponent() const override;

protected:
	FPOILink SelectedPOILink = FPOILink();
	TWeakObjectPtr<AActor> SelectedPOI;
};