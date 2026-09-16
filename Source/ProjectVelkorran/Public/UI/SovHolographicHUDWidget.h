// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI/SovProximityDetectionComponent.h"
#include "Settings/SovGameUserSettings.h"
#include "UI/SovCombatReadinessWidget.h"
#include "UI/SovCombatVitalsWidget.h"
#include "SovHolographicHUDWidget.generated.h"

class ASovPlayerController;

/** One reading of everything the surface draws, taken together so the frame is internally consistent. */
struct FSovHolographicHUDSnapshot
{
	FSovCombatVitalsSnapshot Vitals;
	FSovCombatReadinessSnapshot Readiness;
	TArray<FSovProximityContact> Contacts;
	FSovUserSettingsSnapshot Settings;
	/** -1 when the wielded weapon has no magazine, so the readout is omitted rather than showing zero. */
	int32 AmmoInClip = -1;
	int32 AmmoReserve = -1;
	float Echo = 0.f;
	float MaxEcho = 0.f;
	bool bValid = false;
};

/**
 * The holographic combat HUD for both protagonists.
 *
 * Drawn entirely in code as one painted surface rather than a tree of child widgets or an authored
 * asset: the layout is geometric, it themes itself from the current protagonist, and there is no
 * widget asset to keep in step with the code.
 *
 * The reference direction is minimal to the point of austerity, and the HUD it replaces carries
 * accessibility behaviour that minimalism could quietly discard. Those are kept deliberately:
 * every resource keeps a text label so identity survives without colour, the whole surface honours
 * the user's UI scale, and high contrast swaps the translucent optical veil for an opaque backing
 * rather than removing information.
 */
UCLASS()
class PROJECTVELKORRAN_API USovHolographicHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USovHolographicHUDWidget(const FObjectInitializer& Initializer);

	/** Reads every source once. The owner decides cadence; this never writes gameplay state. */
	static bool ReadSnapshot(const ASovPlayerController* Controller, FSovHolographicHUDSnapshot& Out);

	/** Called by the frontend each refresh. Collapses the surface when there is nothing to show. */
	void RefreshHolographicHUD();

	const FSovHolographicHUDSnapshot& GetDisplayed() const { return Displayed; }

	/** Compact paint diagnostics, readable from a capture session rather than inferred from symptoms. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|HUD")
	FString GetPaintDiagnostics() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	friend struct FSovHolographicHUDTestAccess;

	struct FPalette
	{
		// Initialised, because this is reported as diagnostics before the first paint assigns it and
		// an uninitialised colour would read as a plausible-looking measurement.
		FLinearColor Accent = FLinearColor::Transparent;
		FLinearColor Warm = FLinearColor::Transparent;      // health, and anything urgent
		FLinearColor Backing = FLinearColor::Transparent;   // optical veil, opaque under high contrast
		FLinearColor Line = FLinearColor::Transparent;
		bool bHighContrast = false;
	};

	FPalette BuildPalette() const;
	/** The torn plasma edge. Deterministic jitter, so the frame never crawls between frames. */
	int32 PaintEdging(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FPalette& Palette) const;
	int32 PaintIdentityPlate(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FPalette& Palette, float Scale) const;
	int32 PaintAbilityPips(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FPalette& Palette, float Scale) const;
	int32 PaintAmmo(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FPalette& Palette, float Scale) const;
	int32 PaintEchoArc(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FPalette& Palette, float Scale) const;
	int32 PaintRadar(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer, const FPalette& Palette, float Scale) const;

	FSovHolographicHUDSnapshot Displayed;
	/** Advances only with real time, so the radar sweep reads as motion rather than a stutter. */
	mutable float SweepSeconds = 0.f;
	/** Measured, not assumed: whether paint runs, at what size, and what it was given to draw. */
	mutable int32 PaintCount = 0;
	mutable FVector2D LastPaintSize = FVector2D::ZeroVector;
	/** Whether a paint reached the drawing stage, and the colours it would have drawn with. */
	mutable bool bLastPaintDrew = false;
	mutable FPalette LastPalette;
	/** The clip rectangle the paint was handed, and where the surface sits in absolute space. */
	mutable FVector4 LastCulling = FVector4(0.f, 0.f, 0.f, 0.f);
	mutable FVector2D LastAbsolutePosition = FVector2D::ZeroVector;
	int32 RefreshCount = 0;
	bool bLastRefreshReady = false;
};
