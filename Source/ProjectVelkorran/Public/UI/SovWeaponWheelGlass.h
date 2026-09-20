// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/SovHUDStyle.h"
#include "SovWeaponWheelGlass.generated.h"

class UMaterialInterface;

/** Decorative identity lens for the authored radial menu. Selection remains owned by its Blueprint. */
UCLASS()
class PROJECTVELKORRAN_API USovWeaponWheelGlass : public UUserWidget
{
    GENERATED_BODY()
public:
    USovWeaponWheelGlass(const FObjectInitializer& Initializer);
    /** Turns each existing transparent weapon thumbnail into a legible projected silhouette. */
    UPROPERTY(EditAnywhere, Category = "Sovereign|Wheel") TObjectPtr<UMaterialInterface> WeaponGlyphMaterial;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
    SovHUDStyle::FTheme Theme;
};
