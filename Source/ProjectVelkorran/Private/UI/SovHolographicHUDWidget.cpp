// Copyright Fallen Signal Studios. All Rights Reserved.

#include "UI/SovHolographicHUDWidget.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovEchoComponent.h"
#include "Framework/SovPlayerController.h"
#include "Items/WeaponItem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/SovHUDStyle.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SovHolographicHUD"

namespace
{
/** Slate draws lines, so every curve here is described as a short polyline. */
void BuildArc(TArray<FVector2D>& Out, const FVector2D& Centre, float Radius, float FromDegrees, float ToDegrees, int32 Segments)
{
	Out.Reset(Segments + 1);
	const int32 Steps = FMath::Max(Segments, 2);
	for (int32 Index = 0; Index <= Steps; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Steps);
		const float Angle = FMath::DegreesToRadians(FMath::Lerp(FromDegrees, ToDegrees, Alpha));
		Out.Add(Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
	}
}

void DrawPolyline(FSlateWindowElementList& Elements, int32 Layer, const FPaintGeometry& Paint,
	const TArray<FVector2D>& Points, const FLinearColor& Colour, float Thickness)
{
	if (Points.Num() < 2 || Colour.A <= 0.f) { return; }
	FSlateDrawElement::MakeLines(Elements, Layer, Paint, Points, ESlateDrawEffect::None, Colour, true, Thickness);
}

void DrawSegment(FSlateWindowElementList& Elements, int32 Layer, const FPaintGeometry& Paint,
	const FVector2D& From, const FVector2D& To, const FLinearColor& Colour, float Thickness)
{
	TArray<FVector2D> Points; Points.Add(From); Points.Add(To);
	DrawPolyline(Elements, Layer, Paint, Points, Colour, Thickness);
}

/** A filled block, used for bar fills and the opaque high-contrast backing. */
void DrawBlock(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& Position, const FVector2D& Size, const FLinearColor& Colour)
{
	if (Colour.A <= 0.f || Size.X <= 0.f || Size.Y <= 0.f) { return; }
	static const FSlateColorBrush Brush(FLinearColor::White);
	FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
		&Brush, ESlateDrawEffect::None, Colour);
}

void DrawLabel(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& Position, const FText& Text, const FLinearColor& Colour, int32 FontSize)
{
	if (Text.IsEmpty() || Colour.A <= 0.f) { return; }
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(FontSize, 6));
	// A one pixel shadow keeps text legible against a bright environment, as the existing readout does.
	FSlateDrawElement::MakeText(Elements, Layer, Geometry.ToPaintGeometry(FVector2D(600.f, 24.f), FSlateLayoutTransform(Position + FVector2D(1.f, 1.f))),
		Text, Font, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Colour.A * .8f));
	FSlateDrawElement::MakeText(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(600.f, 24.f), FSlateLayoutTransform(Position)),
		Text, Font, ESlateDrawEffect::None, Colour);
}

/** Deterministic jitter: the torn edge must not crawl from frame to frame. */
float EdgeJitter(int32 Seed)
{
	const float Noise = FMath::Sin(static_cast<float>(Seed) * 12.9898f) * 43758.5453f;
	return Noise - FMath::FloorToFloat(Noise) - .5f;
}
}

USovHolographicHUDWidget::USovHolographicHUDWidget(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	bIsFocusable = false;
	SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> USovHolographicHUDWidget::RebuildWidget()
{
	// Everything is painted; there is no child tree to keep in step.
	return SNew(SBox);
}

bool USovHolographicHUDWidget::ReadSnapshot(const ASovPlayerController* Controller, FSovHolographicHUDSnapshot& Out)
{
	Out = FSovHolographicHUDSnapshot();
	if (!IsValid(Controller)) { return false; }
	if (!USovCombatVitalsWidget::ReadCurrentVitals(Controller, Out.Vitals)) { return false; }
	SovCombatReadiness::Read(Controller, Out.Readiness);
	if (const auto* Settings = USovGameUserSettings::Get()) { Out.Settings = Settings->GetSettingsSnapshot(); }

	auto* Pawn = Cast<ASovPlayerCharacterBase>(Out.Vitals.Pawn.Get());
	if (!IsValid(Pawn)) { return false; }
	if (const auto* Echo = Pawn->GetEchoComponent())
	{
		Out.Echo = Echo->GetEcho();
		Out.MaxEcho = Echo->GetMaxEcho();
	}
	// Ammo is omitted rather than shown as zero when the wielded weapon has no magazine.
	if (const auto* Weapon = Pawn->GetWeapon(true))
	{
		Out.AmmoInClip = Weapon->GetAmmoInClip();
		Out.AmmoReserve = Weapon->GetSpareAmmo();
	}
	if (const auto* Detection = Pawn->FindComponentByClass<USovProximityDetectionComponent>())
	{
		Out.Contacts = Detection->GetContacts();
	}
	Out.bValid = true;
	return true;
}

void USovHolographicHUDWidget::RefreshHolographicHUD()
{
	FSovHolographicHUDSnapshot Current;
	const bool bReady = ReadSnapshot(Cast<ASovPlayerController>(GetOwningPlayer()), Current);
	Displayed = Current;
	SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

USovHolographicHUDWidget::FPalette USovHolographicHUDWidget::BuildPalette() const
{
	const bool bHighContrast = Displayed.Settings.bHighContrastHUD;
	const SovHUDStyle::FTheme Theme = SovHUDStyle::ForProtagonist(Displayed.Vitals.Protagonist, bHighContrast);
	FPalette Palette;
	Palette.bHighContrast = bHighContrast;
	Palette.Accent = Theme.Accent;
	// Health reads warm for both protagonists: urgency should not depend on knowing whose HUD this is.
	Palette.Warm = bHighContrast ? FLinearColor::White : FLinearColor(.96f, .32f, .22f);
	// High contrast trades the optical veil for an opaque backing rather than dropping the readout.
	Palette.Backing = bHighContrast ? FLinearColor(0.f, 0.f, 0.f, 1.f) : Theme.Background;
	Palette.Line = bHighContrast ? FLinearColor::White : Theme.Accent;
	return Palette;
}

int32 USovHolographicHUDWidget::PaintEdging(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette) const
{
	// High contrast keeps the surface plain: torn glow is decoration and it costs legibility.
	if (Palette.bHighContrast) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const float Bleed = FMath::Min(Size.X, Size.Y) * .035f;

	// Four torn runs: two along the top shoulders, two along the bottom, leaving the centre clear.
	const FVector2D Runs[4][2] = {
		{ FVector2D(Size.X * .02f, Size.Y * .045f), FVector2D(Size.X * .30f, Size.Y * .025f) },
		{ FVector2D(Size.X * .70f, Size.Y * .025f), FVector2D(Size.X * .98f, Size.Y * .045f) },
		{ FVector2D(Size.X * .02f, Size.Y * .955f), FVector2D(Size.X * .32f, Size.Y * .975f) },
		{ FVector2D(Size.X * .68f, Size.Y * .975f), FVector2D(Size.X * .98f, Size.Y * .955f) },
	};
	for (int32 Run = 0; Run < 4; ++Run)
	{
		const FVector2D From = Runs[Run][0];
		const FVector2D To = Runs[Run][1];
		// Three passes: a wide dim bleed, a mid glow, then a bright filament.
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			const float Width = Bleed * (1.f - static_cast<float>(Pass) * .34f);
			const float Opacity = .10f + static_cast<float>(Pass) * .26f;
			TArray<FVector2D> Points;
			const int32 Steps = 26;
			for (int32 Index = 0; Index <= Steps; ++Index)
			{
				const float Alpha = static_cast<float>(Index) / static_cast<float>(Steps);
				FVector2D Point = FMath::Lerp(From, To, Alpha);
				// Torn rather than ruled: the offset is seeded per point and never changes.
				const float Offset = EdgeJitter(Run * 1000 + Pass * 100 + Index) * Width;
				Point.Y += Offset;
				// Fray toward the ends so each run dissolves instead of stopping abruptly.
				const float Taper = FMath::Sin(Alpha * PI);
				Point.Y += Offset * (1.f - Taper);
				Points.Add(Point);
			}
			const FLinearColor Glow = Pass == 2 ? Palette.Accent : Palette.Warm;
			DrawPolyline(Elements, Layer + Pass, Paint, Points, Glow.CopyWithNewOpacity(Opacity), Width * .55f);
		}
	}
	return Layer + 3;
}

int32 USovHolographicHUDWidget::PaintIdentityPlate(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const float PlateWidth = FMath::Min(Size.X * .42f, 720.f * Scale);
	const float Left = (Size.X - PlateWidth) * .5f;
	const float Top = Size.Y * .035f;
	const float BarHeight = 12.f * Scale;

	DrawBlock(Elements, Layer, Geometry, FVector2D(Left, Top), FVector2D(PlateWidth, 74.f * Scale), Palette.Backing);

	// Name, letterspaced by the theme's own identity text.
	DrawLabel(Elements, Layer + 1, Geometry, FVector2D(Left + PlateWidth * .5f - 48.f * Scale, Top + 2.f * Scale),
		SovHUDStyle::ForProtagonist(Displayed.Vitals.Protagonist, Palette.bHighContrast).Identity,
		Palette.Accent, FMath::RoundToInt(15.f * Scale));

	const auto BarAt = [&](int32 Index, float Y, float Height, const FLinearColor& Fill, const FText& Label)
	{
		const float Maximum = Displayed.Vitals.Values[Index].Maximum;
		const float Fraction = Maximum > KINDA_SMALL_NUMBER
			? FMath::Clamp(Displayed.Vitals.Values[Index].Current / Maximum, 0.f, 1.f) : 0.f;
		const float TrackLeft = Left + 46.f * Scale;
		const float TrackWidth = PlateWidth - 70.f * Scale;
		DrawBlock(Elements, Layer + 1, Geometry, FVector2D(TrackLeft, Y), FVector2D(TrackWidth, Height),
			FLinearColor(.10f, .16f, .20f, Palette.bHighContrast ? 1.f : .45f));
		DrawBlock(Elements, Layer + 2, Geometry, FVector2D(TrackLeft, Y), FVector2D(TrackWidth * Fraction, Height), Fill);
		DrawSegment(Elements, Layer + 3, Paint, FVector2D(TrackLeft, Y + Height), FVector2D(TrackLeft + TrackWidth, Y + Height),
			Palette.Line.CopyWithNewOpacity(.5f), 1.f);
		// The label is the reason this survives without colour.
		DrawLabel(Elements, Layer + 3, Geometry, FVector2D(Left + 6.f * Scale, Y - 2.f * Scale), Label,
			Palette.Line.CopyWithNewOpacity(.85f), FMath::RoundToInt(10.f * Scale));
	};
	// Shield above, health below and heavier: the reference's weighting, and the right reading order.
	BarAt(1, Top + 26.f * Scale, BarHeight * .55f, Palette.Accent, LOCTEXT("Shield", "SHD"));
	BarAt(0, Top + 42.f * Scale, BarHeight, Palette.Warm, LOCTEXT("Health", "HP"));
	return Layer + 4;
}

int32 USovHolographicHUDWidget::PaintAbilityPips(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const int32 Count = FMath::Min(Displayed.Readiness.Abilities.Num(), 6);
	if (Count <= 0) { return Layer; }
	const float PipWidth = 26.f * Scale;
	const float PipHeight = 8.f * Scale;
	const float Gap = 6.f * Scale;
	const float Total = Count * PipWidth + (Count - 1) * Gap;
	const float Left = (Size.X - Total) * .5f;
	const float Top = Size.Y * .035f + 86.f * Scale;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FSovAbilityHUDEntry& Entry = Displayed.Readiness.Abilities[Index];
		const FVector2D At(Left + Index * (PipWidth + Gap), Top);
		// Filled means usable now; an outline alone means known but not ready.
		const bool bReady = Entry.State == ESovAbilityHUDState::EchoReady;
		const bool bActive = Entry.State == ESovAbilityHUDState::Active;
		const FLinearColor Fill = bActive ? Palette.Warm : Palette.Accent;
		if (bReady || bActive)
		{
			DrawBlock(Elements, Layer + 1, Geometry, At, FVector2D(PipWidth, PipHeight), Fill.CopyWithNewOpacity(.9f));
		}
		else
		{
			DrawBlock(Elements, Layer, Geometry, At, FVector2D(PipWidth, PipHeight), Palette.Backing);
			// Cooldown fills the outline from the left, so progress is legible without a number.
			if (Entry.CooldownDuration > KINDA_SMALL_NUMBER && Entry.CooldownRemaining > 0.f)
			{
				const float Done = FMath::Clamp(1.f - Entry.CooldownRemaining / Entry.CooldownDuration, 0.f, 1.f);
				DrawBlock(Elements, Layer + 1, Geometry, At, FVector2D(PipWidth * Done, PipHeight), Fill.CopyWithNewOpacity(.35f));
			}
		}
		TArray<FVector2D> Outline;
		Outline.Add(At);
		Outline.Add(At + FVector2D(PipWidth, 0.f));
		Outline.Add(At + FVector2D(PipWidth, PipHeight));
		Outline.Add(At + FVector2D(0.f, PipHeight));
		Outline.Add(At);
		DrawPolyline(Elements, Layer + 2, Paint, Outline, Palette.Line.CopyWithNewOpacity(bReady ? .95f : .5f), 1.f);
	}
	return Layer + 3;
}

int32 USovHolographicHUDWidget::PaintAmmo(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	if (Displayed.AmmoInClip < 0) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize();
	const FVector2D Plate(190.f * Scale, 46.f * Scale);
	const FVector2D At(Size.X - Plate.X - 40.f * Scale, Size.Y * .045f);
	DrawBlock(Elements, Layer, Geometry, At, Plate, Palette.Backing);
	DrawLabel(Elements, Layer + 1, Geometry, At + FVector2D(14.f * Scale, 8.f * Scale),
		FText::Format(LOCTEXT("AmmoFormat", "{0} / {1}"), Displayed.AmmoInClip, FMath::Max(Displayed.AmmoReserve, 0)),
		Palette.Accent, FMath::RoundToInt(18.f * Scale));
	return Layer + 2;
}

int32 USovHolographicHUDWidget::PaintEchoArc(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	if (Displayed.MaxEcho <= KINDA_SMALL_NUMBER) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const FVector2D Centre(Size.X * .5f, Size.Y * 1.62f);
	const float Radius = Size.Y * .78f;
	const float Fraction = FMath::Clamp(Displayed.Echo / Displayed.MaxEcho, 0.f, 1.f);

	// A shallow arc across the bottom, segmented so charge is countable rather than estimated.
	const int32 Segments = 8;
	const float Span = 26.f;
	TArray<FVector2D> Points;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float From = -90.f - Span * .5f + Span * (static_cast<float>(Index) / Segments) + .6f;
		const float To = -90.f - Span * .5f + Span * (static_cast<float>(Index + 1) / Segments) - .6f;
		const float SegmentStart = static_cast<float>(Index) / Segments;
		const bool bFilled = Fraction > SegmentStart + KINDA_SMALL_NUMBER;
		BuildArc(Points, Centre, Radius, From, To, 12);
		DrawPolyline(Elements, Layer, Paint, Points, Palette.Line.CopyWithNewOpacity(.28f), 9.f * Scale);
		if (bFilled)
		{
			// The final partial segment fills proportionally rather than snapping on.
			const float SegmentFill = FMath::Clamp((Fraction - SegmentStart) * Segments, 0.f, 1.f);
			BuildArc(Points, Centre, Radius, From, FMath::Lerp(From, To, SegmentFill), 12);
			DrawPolyline(Elements, Layer + 1, Paint, Points, Palette.Accent.CopyWithNewOpacity(.95f), 9.f * Scale);
		}
	}
	DrawLabel(Elements, Layer + 2, Geometry, FVector2D(Size.X * .5f - 18.f * Scale, Size.Y - 34.f * Scale),
		LOCTEXT("Echo", "ECHO"), Palette.Line.CopyWithNewOpacity(.8f), FMath::RoundToInt(10.f * Scale));
	return Layer + 3;
}

int32 USovHolographicHUDWidget::PaintRadar(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const float Radius = FMath::Min(Size.Y * .16f, 150.f * Scale);
	const FVector2D Centre(52.f * Scale + Radius, Size.Y - 62.f * Scale - Radius);

	TArray<FVector2D> Ring;
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		BuildArc(Ring, Centre, Radius * (static_cast<float>(Step) / 3.f), 0.f, 360.f, 64);
		DrawPolyline(Elements, Layer, Paint, Ring, Palette.Line.CopyWithNewOpacity(Step == 3 ? .8f : .3f), Step == 3 ? 1.6f : 1.f);
	}
	// Cardinal ticks: which way is forward must be readable at a glance.
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		const float Angle = FMath::DegreesToRadians(90.f * Tick);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		DrawSegment(Elements, Layer, Paint, Centre + Direction * Radius * .92f, Centre + Direction * Radius * 1.12f,
			Palette.Line.CopyWithNewOpacity(.7f), 1.4f);
	}
	// The sweep, and the player's own facing wedge at the centre.
	const float SweepAngle = FMath::Fmod(SweepSeconds * 90.f, 360.f) - 90.f;
	if (!Palette.bHighContrast)
	{
		DrawSegment(Elements, Layer + 1, Paint, Centre,
			Centre + FVector2D(FMath::Cos(FMath::DegreesToRadians(SweepAngle)), FMath::Sin(FMath::DegreesToRadians(SweepAngle))) * Radius,
			Palette.Accent.CopyWithNewOpacity(.5f), 2.f);
	}
	TArray<FVector2D> Wedge;
	Wedge.Add(Centre + FVector2D(0.f, -9.f * Scale));
	Wedge.Add(Centre + FVector2D(-6.f * Scale, 6.f * Scale));
	Wedge.Add(Centre + FVector2D(6.f * Scale, 6.f * Scale));
	Wedge.Add(Centre + FVector2D(0.f, -9.f * Scale));
	DrawPolyline(Elements, Layer + 2, Paint, Wedge, Palette.Line.CopyWithNewOpacity(.95f), 1.6f);

	for (const FSovProximityContact& Contact : Displayed.Contacts)
	{
		// Screen up is the player's facing, so bearing rotates clockwise from straight up.
		const float Angle = FMath::DegreesToRadians(Contact.BearingDegrees - 90.f);
		const FVector2D At = Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Radius * Contact.NormalisedRange);
		const float Dot = (Contact.bLiveSighting ? 4.2f : 3.2f) * Scale;
		// A memory is drawn as an open mark, a live sighting as a filled one: certainty is visible.
		if (Contact.bLiveSighting)
		{
			DrawBlock(Elements, Layer + 3, Geometry, At - FVector2D(Dot * .5f), FVector2D(Dot, Dot),
				Palette.Warm.CopyWithNewOpacity(Contact.Alpha));
		}
		else
		{
			BuildArc(Ring, At, Dot, 0.f, 360.f, 12);
			DrawPolyline(Elements, Layer + 3, Paint, Ring, Palette.Warm.CopyWithNewOpacity(Contact.Alpha * .9f), 1.2f);
		}
	}
	return Layer + 4;
}

int32 USovHolographicHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	int32 Result = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
	if (!Displayed.bValid) { return Result; }
	if (const UWorld* const World = GetWorld()) { SweepSeconds = World->GetTimeSeconds(); }
	const FPalette Palette = BuildPalette();
	const float Scale = FMath::IsFinite(Displayed.Settings.UIScale) ? FMath::Clamp(Displayed.Settings.UIScale, .75f, 2.f) : 1.f;
	Result = PaintEdging(Geometry, Elements, Result, Palette);
	Result = PaintIdentityPlate(Geometry, Elements, Result, Palette, Scale);
	Result = PaintAbilityPips(Geometry, Elements, Result, Palette, Scale);
	Result = PaintAmmo(Geometry, Elements, Result, Palette, Scale);
	Result = PaintEchoArc(Geometry, Elements, Result, Palette, Scale);
	Result = PaintRadar(Geometry, Elements, Result, Palette, Scale);
	return Result;
}

#undef LOCTEXT_NAMESPACE
