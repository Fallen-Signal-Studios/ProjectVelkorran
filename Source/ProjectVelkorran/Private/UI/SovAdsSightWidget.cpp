// Copyright Fallen Signal Studios. All Rights Reserved.

#include "UI/SovAdsSightWidget.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerController.h"
#include "Rendering/DrawElements.h"
#include "UI/SovHUDStyle.h"

namespace
{
/** A ring of line segments. Slate draws lines, so curves are described rather than filled. */
void BuildRing(TArray<FVector2D>& Out, const FVector2D& Centre, float Radius, int32 Segments)
{
	Out.Reset(Segments + 1);
	for (int32 Index = 0; Index <= Segments; ++Index)
	{
		const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(FMath::Max(Segments, 3));
		Out.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
}

void DrawLine(FSlateWindowElementList& Elements, int32 Layer, const FPaintGeometry& Paint,
	const FVector2D& From, const FVector2D& To, const FLinearColor& Colour, float Thickness)
{
	TArray<FVector2D> Points;
	Points.Add(From);
	Points.Add(To);
	FSlateDrawElement::MakeLines(Elements, Layer, Paint, Points, ESlateDrawEffect::None, Colour, true, Thickness);
}
}

USovAdsSightWidget::USovAdsSightWidget(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	bIsFocusable = false;
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USovAdsSightWidget::UpdateSight(ESovAdsSight Sight, float Alpha, float Blend, const ASovPlayerController* Controller)
{
	DisplayedSight = Sight;
	DisplayedAlpha = FMath::Clamp(FMath::IsFinite(Alpha) ? Alpha : 0.f, 0.f, 1.f);
	DisplayedBlend = FMath::Clamp(FMath::IsFinite(Blend) ? Blend : 0.f, 0.f, 1.f);
	// Identity comes from the pawn actually being played, never from a cached protagonist.
	const auto* Pawn = IsValid(Controller) ? Cast<ASovPlayerCharacterBase>(Controller->GetPawn()) : nullptr;
	Protagonist = IsValid(Pawn) ? Pawn->GetProtagonistIdentityTag() : FGameplayTag();
	SetVisibility(DisplayedAlpha > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

int32 USovAdsSightWidget::PaintScope(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FLinearColor& Accent) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Centre = Size * .5f;
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const float Radius = FMath::Min(Size.X, Size.Y) * .34f;

	// Vignette: concentric rings outside the sight, thickening and darkening outward. Slate has no
	// filled circle, so the darkening is described as bands rather than painted as a mask.
	TArray<FVector2D> Ring;
	const int32 Bands = 7;
	for (int32 Band = 0; Band < Bands; ++Band)
	{
		const float Fraction = static_cast<float>(Band) / static_cast<float>(Bands);
		const float BandRadius = Radius * (1.06f + Fraction * .95f);
		const float BandAlpha = DisplayedAlpha * (.16f + Fraction * .58f);
		BuildRing(Ring, Centre, BandRadius, 96);
		FSlateDrawElement::MakeLines(Elements, Layer, Paint, Ring, ESlateDrawEffect::None,
			FLinearColor(0.f, 0.f, 0.f, BandAlpha), true, Radius * .30f);
	}

	// The sight ring itself, with a finer inner ring for depth.
	BuildRing(Ring, Centre, Radius, 128);
	FSlateDrawElement::MakeLines(Elements, Layer + 1, Paint, Ring, ESlateDrawEffect::None,
		Accent.CopyWithNewOpacity(DisplayedAlpha), true, 2.2f);
	BuildRing(Ring, Centre, Radius * .965f, 128);
	FSlateDrawElement::MakeLines(Elements, Layer + 1, Paint, Ring, ESlateDrawEffect::None,
		Accent.CopyWithNewOpacity(DisplayedAlpha * .35f), true, 1.f);

	// Reticle: a cross with a clear centre, so the target is never hidden by the sight.
	const float Gap = Radius * .12f;
	const FLinearColor Line = Accent.CopyWithNewOpacity(DisplayedAlpha * .9f);
	DrawLine(Elements, Layer + 2, Paint, FVector2D(Centre.X - Radius, Centre.Y), FVector2D(Centre.X - Gap, Centre.Y), Line, 1.4f);
	DrawLine(Elements, Layer + 2, Paint, FVector2D(Centre.X + Gap, Centre.Y), FVector2D(Centre.X + Radius, Centre.Y), Line, 1.4f);
	DrawLine(Elements, Layer + 2, Paint, FVector2D(Centre.X, Centre.Y - Radius), FVector2D(Centre.X, Centre.Y - Gap), Line, 1.4f);
	DrawLine(Elements, Layer + 2, Paint, FVector2D(Centre.X, Centre.Y + Gap), FVector2D(Centre.X, Centre.Y + Radius), Line, 1.4f);

	// Elevation ticks below the centre, shortening with distance: a readable sense of holdover.
	for (int32 Tick = 1; Tick <= 4; ++Tick)
	{
		const float Y = Centre.Y + Radius * .18f * static_cast<float>(Tick);
		const float Half = Radius * (.075f - static_cast<float>(Tick) * .012f);
		if (Half <= 0.f) { break; }
		DrawLine(Elements, Layer + 2, Paint, FVector2D(Centre.X - Half, Y), FVector2D(Centre.X + Half, Y),
			Accent.CopyWithNewOpacity(DisplayedAlpha * .55f), 1.f);
	}
	return Layer + 3;
}

int32 USovAdsSightWidget::PaintIronSight(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FLinearColor& Accent) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Centre = Size * .5f;
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const float Reach = FMath::Min(Size.X, Size.Y) * .06f;
	const FLinearColor Line = Accent.CopyWithNewOpacity(DisplayedAlpha * .85f);

	// Two posts and a centre gap: frames the shot without covering what is being shot at.
	DrawLine(Elements, Layer, Paint, FVector2D(Centre.X - Reach * 2.2f, Centre.Y - Reach * .35f),
		FVector2D(Centre.X - Reach * 2.2f, Centre.Y + Reach * .9f), Line, 2.f);
	DrawLine(Elements, Layer, Paint, FVector2D(Centre.X + Reach * 2.2f, Centre.Y - Reach * .35f),
		FVector2D(Centre.X + Reach * 2.2f, Centre.Y + Reach * .9f), Line, 2.f);
	// A shallow chevron under the point of aim rather than a dot over it.
	DrawLine(Elements, Layer, Paint, FVector2D(Centre.X - Reach * .55f, Centre.Y + Reach * .75f),
		FVector2D(Centre.X, Centre.Y + Reach * .3f), Line, 1.6f);
	DrawLine(Elements, Layer, Paint, FVector2D(Centre.X, Centre.Y + Reach * .3f),
		FVector2D(Centre.X + Reach * .55f, Centre.Y + Reach * .75f), Line, 1.6f);
	return Layer + 1;
}

int32 USovAdsSightWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	int32 Result = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
	if (DisplayedAlpha <= 0.f) { return Result; }
	// High contrast is not wired here yet; it belongs with the holographic HUD pass, which owns the
	// accessibility contract for every surface rather than settling it one widget at a time.
	const SovHUDStyle::FTheme Theme = SovHUDStyle::ForProtagonist(Protagonist, false);
	Result = DisplayedSight == ESovAdsSight::Scope
		? PaintScope(Geometry, Elements, Result, Theme.Accent)
		: PaintIronSight(Geometry, Elements, Result, Theme.Accent);
	return Result;
}
