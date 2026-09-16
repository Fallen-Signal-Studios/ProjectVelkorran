// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Weapons/SovAdsComponent.h"
#include "SovAdsSightWidget.generated.h"

class ASovPlayerController;

/**
 * The sight picture the player looks through while aiming.
 *
 * Drawn entirely in code rather than authored as an asset, the same way the combat vitals readout
 * is: there is no widget asset to keep in step, and it themes itself from the current protagonist.
 * It is never interactive and never accepts focus.
 */
UCLASS()
class PROJECTVELKORRAN_API USovAdsSightWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USovAdsSightWidget(const FObjectInitializer& Initializer);

	/** Called each frame the sight is up. Alpha 0 collapses it rather than drawing nothing repeatedly. */
	void UpdateSight(ESovAdsSight Sight, float Alpha, float Blend, const ASovPlayerController* Controller);

	ESovAdsSight GetDisplayedSight() const { return DisplayedSight; }
	float GetDisplayedAlpha() const { return DisplayedAlpha; }

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	int32 PaintScope(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const struct FLinearColor& Accent) const;
	int32 PaintIronSight(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const struct FLinearColor& Accent) const;

	ESovAdsSight DisplayedSight = ESovAdsSight::Default;
	float DisplayedAlpha = 0.f;
	float DisplayedBlend = 0.f;
	FGameplayTag Protagonist;
};
