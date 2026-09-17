// Copyright Fallen Signal Studios. All Rights Reserved.

#include "UI/SovHolographicHUDWidget.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovEchoComponent.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/SovPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Items/WeaponItem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Rendering/DrawElementTypes.h"
#include "Sovereign/SovGameplayTags.h"
#include "Styling/CoreStyle.h"
#include "Types/SlateEnums.h"
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovHolographicHUDLayout.h"
#include "UI/SovHUDStyle.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SovHolographicHUD"

/**
 * Submits one rectangle by three drawing routes so a blank surface can be attributed to a specific
 * path instead of guessed at. Off by default, so no probe can reach a player.
 *
 * It has already earned its keep once: every route drew correctly, which is what proved the blank
 * captures were the screenshot path excluding UI rather than anything wrong with this surface.
 */
static TAutoConsoleVariable<int32> CVarHolographicPaintProbe(
	TEXT("sov.HUD.HolographicPaintProbe"), 0,
	TEXT("Draw diagnostic marks on the holographic HUD to identify which submission path renders."),
	ECVF_Cheat);

namespace
{
enum class ETextAlign : uint8 { Left, Centre, Right };

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

/** The reference's plates and bars are cut at the corners rather than rounded. */
void BuildChamferedRect(TArray<FVector2D>& Out, const FVector2D& At, const FVector2D& Size, float Cut)
{
	const float C = FMath::Max(FMath::Min(Cut, FMath::Min(Size.X, Size.Y) * .5f), 0.f);
	Out.Reset(9);
	Out.Add(At + FVector2D(C, 0.f));
	Out.Add(At + FVector2D(Size.X - C, 0.f));
	Out.Add(At + FVector2D(Size.X, C));
	Out.Add(At + FVector2D(Size.X, Size.Y - C));
	Out.Add(At + FVector2D(Size.X - C, Size.Y));
	Out.Add(At + FVector2D(C, Size.Y));
	Out.Add(At + FVector2D(0.f, Size.Y - C));
	Out.Add(At + FVector2D(0.f, C));
	Out.Add(At + FVector2D(C, 0.f));
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

/** A filled block, used for bar tracks, glyphs and the opaque high-contrast backing. */
void DrawBlock(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& Position, const FVector2D& Size, const FLinearColor& Colour)
{
	if (Colour.A <= 0.f || Size.X <= 0.f || Size.Y <= 0.f) { return; }
	static const FSlateColorBrush Brush(FLinearColor::White);
	FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)),
		&Brush, ESlateDrawEffect::None, Colour);
}

/** The references fill every bar with a gradient, which Slate supports directly. */
void DrawGradientBlock(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& Position, const FVector2D& Size, const FLinearColor& From, const FLinearColor& To)
{
	if (Size.X <= 0.f || Size.Y <= 0.f || (From.A <= 0.f && To.A <= 0.f)) { return; }
	TArray<FSlateGradientStop> Stops;
	Stops.Add(FSlateGradientStop(FVector2f(0.f, 0.f), From));
	Stops.Add(FSlateGradientStop(FVector2f(static_cast<float>(Size.X), 0.f), To));
	// Orient_Vertical runs vertical bands along X, which is the left-to-right sweep the bars want.
	FSlateDrawElement::MakeGradient(Elements, Layer,
		Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Position)), Stops, Orient_Vertical);
}

/** Slate has no filled-circle primitive, so the radar's disc is scanned out in rows. */
void DrawDisc(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& Centre, float Radius, const FLinearColor& Colour)
{
	if (Colour.A <= 0.f || Radius <= 1.f) { return; }
	const int32 Rows = FMath::Clamp(FMath::RoundToInt(Radius * .7f), 16, 64);
	const float RowHeight = (2.f * Radius) / Rows + 1.f;
	for (int32 Index = 0; Index < Rows; ++Index)
	{
		const float Y = ((static_cast<float>(Index) + .5f) / Rows * 2.f - 1.f) * Radius;
		const float HalfWidth = FMath::Sqrt(FMath::Max(Radius * Radius - Y * Y, 0.f));
		DrawBlock(Elements, Layer, Geometry, Centre + FVector2D(-HalfWidth, Y - RowHeight * .5f),
			FVector2D(HalfWidth * 2.f, RowHeight), Colour);
	}
}

float MeasureText(const FText& Text, const FSlateFontInfo& Font)
{
	if (!FSlateApplication::IsInitialized()) { return Text.ToString().Len() * Font.Size * .6f; }
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return static_cast<float>(Measure->Measure(Text.ToString(), Font).X);
}

void DrawLabel(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& At, const FText& Text, const FLinearColor& Colour, int32 FontSize,
	ETextAlign Align = ETextAlign::Left, const ANSICHAR* Style = "Bold")
{
	if (Text.IsEmpty() || Colour.A <= 0.f) { return; }
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(Style, FMath::Max(FontSize, 6));
	const float Width = MeasureText(Text, Font);
	FVector2D Position = At;
	if (Align == ETextAlign::Centre) { Position.X -= Width * .5f; }
	else if (Align == ETextAlign::Right) { Position.X -= Width; }
	const FVector2D Box(Width + 12.f, FontSize * 2.f);
	// A one pixel shadow keeps text legible against a bright environment, as the old readout did.
	FSlateDrawElement::MakeText(Elements, Layer,
		Geometry.ToPaintGeometry(Box, FSlateLayoutTransform(Position + FVector2D(1.f, 1.f))),
		Text, Font, ESlateDrawEffect::None, FLinearColor(0.f, 0.f, 0.f, Colour.A * .75f));
	FSlateDrawElement::MakeText(Elements, Layer + 1,
		Geometry.ToPaintGeometry(Box, FSlateLayoutTransform(Position)),
		Text, Font, ESlateDrawEffect::None, Colour);
}

/** The references title the plate with the bare name, spaced out: T A R R I K. */
FText Letterspaced(const FText& Identity)
{
	FString Name = Identity.ToString();
	int32 Slash = INDEX_NONE;
	if (Name.FindChar(TEXT('/'), Slash)) { Name.LeftInline(Slash); }
	Name.TrimStartAndEndInline();
	FString Spaced;
	Spaced.Reserve(Name.Len() * 2);
	for (int32 Index = 0; Index < Name.Len(); ++Index)
	{
		if (Index > 0) { Spaced.AppendChar(TEXT(' ')); }
		Spaced.AppendChar(Name[Index]);
	}
	return FText::FromString(Spaced);
}

void DrawShieldGlyph(FSlateWindowElementList& Elements, int32 Layer, const FPaintGeometry& Paint,
	const FVector2D& Centre, float Size, const FLinearColor& Colour)
{
	const float W = Size * .48f, H = Size * .58f;
	TArray<FVector2D> Points;
	Points.Add(Centre + FVector2D(0.f, -H));
	Points.Add(Centre + FVector2D(W, -H * .55f));
	Points.Add(Centre + FVector2D(W * .72f, H * .55f));
	Points.Add(Centre + FVector2D(0.f, H));
	Points.Add(Centre + FVector2D(-W * .72f, H * .55f));
	Points.Add(Centre + FVector2D(-W, -H * .55f));
	Points.Add(Centre + FVector2D(0.f, -H));
	DrawPolyline(Elements, Layer, Paint, Points, Colour, 1.6f);
}

void DrawCrossGlyph(FSlateWindowElementList& Elements, int32 Layer, const FGeometry& Geometry,
	const FVector2D& Centre, float Size, const FLinearColor& Colour)
{
	const float Arm = Size * .46f, Thick = Size * .26f;
	DrawBlock(Elements, Layer, Geometry, Centre - FVector2D(Arm, Thick * .5f), FVector2D(Arm * 2.f, Thick), Colour);
	DrawBlock(Elements, Layer, Geometry, Centre - FVector2D(Thick * .5f, Arm), FVector2D(Thick, Arm * 2.f), Colour);
}

void DrawChevron(FSlateWindowElementList& Elements, int32 Layer, const FPaintGeometry& Paint,
	const FVector2D& Tip, float Size, float DirectionX, const FLinearColor& Colour, float Thickness)
{
	TArray<FVector2D> Points;
	Points.Add(Tip + FVector2D(-Size * DirectionX, -Size));
	Points.Add(Tip);
	Points.Add(Tip + FVector2D(-Size * DirectionX, Size));
	DrawPolyline(Elements, Layer, Paint, Points, Colour, Thickness);
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
	// Everything is painted rather than composed from child widgets, so the root carries no content.
	// The sibling threat overlay renders full screen from exactly this, which disproves an earlier
	// assumption here that a bare box would leave the paint routines drawing into a zero-sized
	// rectangle: a screen-added widget is allotted the screen regardless of its root's desired size.
	return SNew(SBox);
}

bool USovHolographicHUDWidget::ReadSnapshot(const ASovPlayerController* Controller, FSovHolographicHUDSnapshot& Out)
{
	Out = FSovHolographicHUDSnapshot();
	if (!IsValid(Controller)) { return false; }
	// Cinematic hiding needs nothing here. Narrative's documented route is a GameplayTag track adding
	// Narrative.State.Player.WantsHideHUD.All to the player - the sequence actor's own
	// bHideEvenEssentialHUDElements is deprecated in favour of it - and ReadCurrentVitals already
	// refuses on State_Player_WantsHideHUD, which parent-matches .All. A gate on the controller's
	// live sequence list was written here and removed: it would have blanked the surface during
	// every sequence, including ones authored to keep the HUD up.
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

void USovHolographicHUDWidget::EnsureEdgeMaterial()
{
	if (bEdgeMaterialChecked) { return; }
	// Attempted once. A missing asset is not an error: the edging falls back to drawn lines, so a
	// fresh clone without this content, and the content-free runtime tests, both still work.
	bEdgeMaterialChecked = true;
	auto* const Material = Cast<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/Aurelion/UI/M_AurelionHolographicEdge.M_AurelionHolographicEdge")).TryLoad());
	if (!Material) { return; }
	EdgeTop = UMaterialInstanceDynamic::Create(Material, this);
	EdgeBottom = UMaterialInstanceDynamic::Create(Material, this);
	if (!EdgeTop || !EdgeBottom) { EdgeTop = nullptr; EdgeBottom = nullptr; return; }
	// The only difference between them: which side of the band the screen edge is on.
	EdgeTop->SetScalarParameterValue(TEXT("FlipV"), 0.f);
	EdgeBottom->SetScalarParameterValue(TEXT("FlipV"), 1.f);
	EdgeTopBrush.SetResourceObject(EdgeTop);
	EdgeBottomBrush.SetResourceObject(EdgeBottom);
}

void USovHolographicHUDWidget::RefreshHolographicHUD()
{
	FSovHolographicHUDSnapshot Current;
	const bool bReady = ReadSnapshot(Cast<ASovPlayerController>(GetOwningPlayer()), Current);
	Displayed = Current;
	++RefreshCount;
	bLastRefreshReady = bReady;
	EnsureEdgeMaterial();
	if (bReady && EdgeTop && EdgeBottom)
	{
		// Tinted here rather than in paint: parameters are owner state, and paint is const.
		const FPalette Palette = BuildPalette();
		EdgeTop->SetVectorParameterValue(TEXT("EdgeColour"), Palette.Accent);
		EdgeTop->SetVectorParameterValue(TEXT("GlowColour"), Palette.Glow);
		EdgeBottom->SetVectorParameterValue(TEXT("EdgeColour"), Palette.Accent);
		EdgeBottom->SetVectorParameterValue(TEXT("GlowColour"), Palette.Glow);
		// High contrast drops the glow entirely, exactly as the polyline path does. Unity otherwise:
		// above it the colour clips channel by channel, turning amber into yellow and teal into
		// white, so the multiply erases the very identity it is applied to.
		const float Intensity = Palette.bHighContrast ? 0.f : 1.f;
		EdgeTop->SetScalarParameterValue(TEXT("EdgeIntensity"), Intensity);
		EdgeBottom->SetScalarParameterValue(TEXT("EdgeIntensity"), Intensity);
	}
	SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void USovHolographicHUDWidget::SetSafeAreaSource(USovAccessibilityPresentation* Presentation) { SafeAreaSource = Presentation; }

FString USovHolographicHUDWidget::GetPaintDiagnostics() const
{
	// The palette is reported because a theme that resolves to zero alpha would draw nothing at any
	// depth, and that failure is indistinguishable from being painted underneath something opaque.
	return FString::Printf(
		TEXT("paints=%d drew=%d size=%.0fx%.0f refreshes=%d ready=%d valid=%d inViewport=%d contacts=%d ")
		TEXT("accentA=%.2f lineA=%.2f healthA=%.2f backA=%.2f cull=%.0f,%.0f,%.0f,%.0f abs=%.0f,%.0f ")
		TEXT("edgeMaterial=%d protagonist=%s"),
		PaintCount, bLastPaintDrew ? 1 : 0, LastPaintSize.X, LastPaintSize.Y, RefreshCount,
		bLastRefreshReady ? 1 : 0, Displayed.bValid ? 1 : 0, IsInViewport() ? 1 : 0, Displayed.Contacts.Num(),
		LastPalette.Accent.A, LastPalette.Line.A, LastPalette.HealthTo.A, LastPalette.Backing.A,
		LastCulling.X, LastCulling.Y, LastCulling.Z, LastCulling.W,
		LastAbsolutePosition.X, LastAbsolutePosition.Y,
		// Which edging actually drew. Without this the material path and the polyline fallback are
		// indistinguishable in a report, and the only way to tell them apart is to look at a frame.
		(EdgeTop && EdgeBottom) ? 1 : 0,
		// The protagonist is an identity tag, not an enum: an unset tag is exactly the case that would
		// make the theme resolve to nothing, so it has to be readable rather than reduced to a number.
		*Displayed.Vitals.Protagonist.ToString());
}

USovHolographicHUDWidget::FPalette USovHolographicHUDWidget::BuildPalette() const
{
	const bool bHighContrast = Displayed.Settings.bHighContrastHUD;
	const SovHUDStyle::FTheme Theme = SovHUDStyle::ForProtagonist(Displayed.Vitals.Protagonist, bHighContrast);
	FPalette Palette;
	Palette.bHighContrast = bHighContrast;
	Palette.Accent = Theme.Accent;
	Palette.Line = bHighContrast ? FLinearColor::White : Theme.Accent;
	// High contrast trades the optical veil for an opaque backing rather than dropping the readout.
	Palette.Backing = bHighContrast ? FLinearColor(0.f, 0.f, 0.f, 1.f) : FLinearColor(.015f, .025f, .04f, .55f);
	if (bHighContrast)
	{
		// Glow and gradients are decoration that costs legibility; the bars stay flat and white.
		Palette.ShieldFrom = Palette.ShieldTo = FLinearColor::White;
		Palette.HealthFrom = Palette.HealthTo = FLinearColor::White;
		return Palette;
	}
	// The references give each protagonist a distinct health colour. That would normally make
	// urgency depend on knowing whose HUD this is, but the references also put a shield glyph and a
	// medical cross on the bars, which carries the meaning without colour - the job the old text
	// labels were doing. So the reference palettes are followed exactly and the glyphs are kept.
	if (Displayed.Vitals.Protagonist == FSovGameplayTags::Get().Character_Player_Selene)
	{
		Palette.Glow = FLinearColor(.12f, .70f, 1.f);
		Palette.ShieldFrom = FLinearColor(.22f, .60f, 1.f);
		Palette.ShieldTo = FLinearColor(.62f, .90f, 1.f);
		Palette.HealthFrom = FLinearColor(.05f, .72f, .44f);
		Palette.HealthTo = FLinearColor(.34f, .95f, .72f);
	}
	else
	{
		Palette.Glow = FLinearColor(1.f, .16f, .03f);
		Palette.ShieldFrom = FLinearColor(1.f, .58f, .10f);
		Palette.ShieldTo = FLinearColor(1.f, .86f, .48f);
		Palette.HealthFrom = FLinearColor(.70f, .05f, .02f);
		Palette.HealthTo = FLinearColor(1.f, .28f, .08f);
	}
	return Palette;
}

int32 USovHolographicHUDWidget::PaintEdging(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette) const
{
	// High contrast keeps the surface plain: torn glow is decoration and it costs legibility.
	if (Palette.bHighContrast) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();

	// Four bands hugging the corners, frayed in the shader. A line cannot bleed, which is why the
	// polyline version below reads as scribble rather than plasma; the material falls off into glow.
	// The centre is left clear either way: the references frame an empty screen.
	if (EdgeTop && EdgeBottom)
	{
		const float Band = Size.Y * .072f;
		// The material fades each run in at both ends along its length. Only the inner ends should
		// be seen doing it, so the outer ends are pushed past the screen edge where their taper
		// falls outside the viewport entirely.
		const float Overhang = Size.X * .045f;
		const FVector2D Bands[4][2] = {
			{ FVector2D(-Overhang, 0.f), FVector2D(Size.X * .30f + Overhang, Band) },
			{ FVector2D(Size.X * .70f, 0.f), FVector2D(Size.X * .30f + Overhang, Band) },
			{ FVector2D(-Overhang, Size.Y - Band), FVector2D(Size.X * .32f + Overhang, Band) },
			{ FVector2D(Size.X * .68f, Size.Y - Band), FVector2D(Size.X * .32f + Overhang, Band) },
		};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			// The brushes are members rather than locals: Slate keeps the brush's resource handle,
			// and a stack brush would be gone by the time the element list is drawn. Tinting is
			// white because the material already carries the protagonist's colours.
			FSlateDrawElement::MakeBox(Elements, Layer,
				Geometry.ToPaintGeometry(Bands[Index][1], FSlateLayoutTransform(Bands[Index][0])),
				Index < 2 ? &EdgeTopBrush : &EdgeBottomBrush, ESlateDrawEffect::None, FLinearColor::White);
		}
		return Layer + 1;
	}

	const float Bleed = FMath::Min(Size.X, Size.Y) * .030f;

	// Four runs hugging the corners and running off the sides. The centre is deliberately left
	// clear: the references frame an empty screen rather than drawing across it.
	const FVector2D Runs[4][2] = {
		{ FVector2D(-Size.X * .02f, Size.Y * .060f), FVector2D(Size.X * .265f, Size.Y * .020f) },
		{ FVector2D(Size.X * .735f, Size.Y * .020f), FVector2D(Size.X * 1.02f, Size.Y * .060f) },
		{ FVector2D(-Size.X * .02f, Size.Y * .940f), FVector2D(Size.X * .300f, Size.Y * .980f) },
		{ FVector2D(Size.X * .700f, Size.Y * .980f), FVector2D(Size.X * 1.02f, Size.Y * .940f) },
	};
	for (int32 Run = 0; Run < 4; ++Run)
	{
		const FVector2D From = Runs[Run][0];
		const FVector2D To = Runs[Run][1];
		// Four passes: a wide dim bleed in the glow colour tightening to a bright accent filament.
		for (int32 Pass = 0; Pass < 4; ++Pass)
		{
			const float Width = Bleed * (1.f - static_cast<float>(Pass) * .23f);
			const float Opacity = Pass == 3 ? .95f : .07f + static_cast<float>(Pass) * .09f;
			const FLinearColor Colour = Pass >= 2 ? Palette.Accent : Palette.Glow;
			TArray<FVector2D> Points;
			const int32 Steps = 30;
			for (int32 Index = 0; Index <= Steps; ++Index)
			{
				const float Alpha = static_cast<float>(Index) / static_cast<float>(Steps);
				FVector2D Point = FMath::Lerp(From, To, Alpha);
				// Torn rather than ruled: the offset is seeded per point and never changes.
				const float Offset = EdgeJitter(Run * 1000 + Pass * 100 + Index) * Width;
				// Fray toward the ends so each run dissolves instead of stopping abruptly.
				Point.Y += Offset + Offset * (1.f - FMath::Sin(Alpha * PI));
				Points.Add(Point);
			}
			DrawPolyline(Elements, Layer + Pass, Paint, Points, Colour.CopyWithNewOpacity(Opacity),
				FMath::Max(Width * (Pass == 3 ? .14f : .5f), 1.f));
		}
	}
	return Layer + 4;
}

int32 USovHolographicHUDWidget::PaintIdentityPlate(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const SovHolographicHUDLayout::FHUDGeometry Layout = SovHolographicHUDLayout::Compute(Geometry.GetLocalSize(), Scale);
	const float PlateWidth = static_cast<float>(Layout.Plate.GetSize().X);
	const float PlateHeight = static_cast<float>(Layout.Plate.GetSize().Y);
	const FVector2D At = Layout.Plate.Min;

	DrawBlock(Elements, Layer, Geometry, At, FVector2D(PlateWidth, PlateHeight), Palette.Backing);
	TArray<FVector2D> Frame;
	BuildChamferedRect(Frame, At, FVector2D(PlateWidth, PlateHeight), 26.f * Scale);
	DrawPolyline(Elements, Layer + 1, Paint, Frame, Palette.Line.CopyWithNewOpacity(.85f), 1.8f);

	// The bare name, letterspaced and centred on the plate's top edge.
	DrawLabel(Elements, Layer + 2, Geometry, FVector2D(At.X + PlateWidth * .5f, At.Y + 1.f * Scale),
		Letterspaced(SovHUDStyle::ForProtagonist(Displayed.Vitals.Protagonist, Palette.bHighContrast).Identity),
		Palette.Accent, FMath::RoundToInt(15.f * Scale), ETextAlign::Centre);

	const float TrackLeft = At.X + 56.f * Scale;
	const float TrackWidth = PlateWidth - 92.f * Scale;
	const auto Bar = [&](int32 Index, float Y, float Height, const FLinearColor& From, const FLinearColor& To, bool bChevron)
	{
		const float Maximum = Displayed.Vitals.Values[Index].Maximum;
		const float Fraction = Maximum > KINDA_SMALL_NUMBER
			? FMath::Clamp(Displayed.Vitals.Values[Index].Current / Maximum, 0.f, 1.f) : 0.f;
		DrawBlock(Elements, Layer + 2, Geometry, FVector2D(TrackLeft, Y), FVector2D(TrackWidth, Height),
			FLinearColor(.04f, .06f, .09f, Palette.bHighContrast ? 1.f : .60f));
		DrawGradientBlock(Elements, Layer + 3, Geometry, FVector2D(TrackLeft, Y),
			FVector2D(TrackWidth * Fraction, Height), From, To);
		TArray<FVector2D> Outline;
		BuildChamferedRect(Outline, FVector2D(TrackLeft, Y), FVector2D(TrackWidth, Height), Height * .42f);
		DrawPolyline(Elements, Layer + 4, Paint, Outline, Palette.Line.CopyWithNewOpacity(.8f), 1.4f);
		if (bChevron)
		{
			// The heavier bar ends in a point, as the references do.
			DrawChevron(Elements, Layer + 4, Paint, FVector2D(TrackLeft + TrackWidth + 11.f * Scale, Y + Height * .5f),
				Height * .46f, 1.f, Palette.Line.CopyWithNewOpacity(.9f), 2.f);
		}
	};
	const float ShieldY = At.Y + 29.f * Scale;
	const float ShieldHeight = 11.f * Scale;
	const float HealthY = At.Y + 48.f * Scale;
	const float HealthHeight = 21.f * Scale;
	// Shield above, health below and heavier: the references' weighting, and the right reading order.
	Bar(1, ShieldY, ShieldHeight, Palette.ShieldFrom, Palette.ShieldTo, false);
	Bar(0, HealthY, HealthHeight, Palette.HealthFrom, Palette.HealthTo, true);
	// These glyphs are the reason the readout survives without colour.
	DrawShieldGlyph(Elements, Layer + 4, Paint, FVector2D(At.X + 34.f * Scale, ShieldY + ShieldHeight * .5f),
		17.f * Scale, Palette.ShieldTo);
	DrawCrossGlyph(Elements, Layer + 4, Geometry, FVector2D(At.X + 34.f * Scale, HealthY + HealthHeight * .5f),
		18.f * Scale, Palette.HealthTo);
	return Layer + 5;
}

int32 USovHolographicHUDWidget::PaintAbilityPips(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	const FVector2D Size = Geometry.GetLocalSize();
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	// The references always show six slots, filled or not, so the row does not change width in play.
	const int32 Slots = 6;
	const float PipWidth = 27.f * Scale;
	const float PipHeight = 9.f * Scale;
	const float Gap = 7.f * Scale;
	const float Total = Slots * PipWidth + (Slots - 1) * Gap;
	const float Left = (Size.X - Total) * .5f;
	const float Top = static_cast<float>(SovHolographicHUDLayout::Compute(Size, Scale).Plate.Min.Y) + 76.f * Scale;

	for (int32 Index = 0; Index < Slots; ++Index)
	{
		const FVector2D At(Left + Index * (PipWidth + Gap), Top);
		const FSovAbilityHUDEntry* const Entry = Displayed.Readiness.Abilities.IsValidIndex(Index)
			? &Displayed.Readiness.Abilities[Index] : nullptr;
		const bool bReady = Entry && Entry->State == ESovAbilityHUDState::EchoReady;
		const bool bActive = Entry && Entry->State == ESovAbilityHUDState::Active;
		// Filled means usable now; an outline alone means known but not ready, or no ability at all.
		if (bReady || bActive)
		{
			DrawGradientBlock(Elements, Layer + 1, Geometry, At, FVector2D(PipWidth, PipHeight),
				bActive ? Palette.HealthFrom : Palette.ShieldFrom, bActive ? Palette.HealthTo : Palette.ShieldTo);
		}
		else if (Entry && Entry->CooldownDuration > KINDA_SMALL_NUMBER && Entry->CooldownRemaining > 0.f)
		{
			// Cooldown fills from the left, so progress is legible without a number.
			const float Done = FMath::Clamp(1.f - Entry->CooldownRemaining / Entry->CooldownDuration, 0.f, 1.f);
			DrawBlock(Elements, Layer + 1, Geometry, At, FVector2D(PipWidth * Done, PipHeight),
				Palette.Accent.CopyWithNewOpacity(.40f));
		}
		TArray<FVector2D> Outline;
		BuildChamferedRect(Outline, At, FVector2D(PipWidth, PipHeight), 3.f * Scale);
		DrawPolyline(Elements, Layer + 2, Paint, Outline, Palette.Line.CopyWithNewOpacity(bReady ? .95f : .45f), 1.2f);
	}
	return Layer + 3;
}

int32 USovHolographicHUDWidget::PaintAmmo(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	if (Displayed.AmmoInClip < 0) { return Layer; }
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const SovHolographicHUDLayout::FHUDGeometry Layout = SovHolographicHUDLayout::Compute(Geometry.GetLocalSize(), Scale);
	const FVector2D Plate = Layout.Ammo.GetSize();
	const FVector2D At = Layout.Ammo.Min;

	DrawBlock(Elements, Layer, Geometry, At, Plate, Palette.Backing);
	TArray<FVector2D> Frame;
	BuildChamferedRect(Frame, At, Plate, 20.f * Scale);
	DrawPolyline(Elements, Layer + 1, Paint, Frame, Palette.Line.CopyWithNewOpacity(.85f), 1.8f);

	// Three rounds, as the references mark the readout.
	const float RoundWidth = 5.f * Scale;
	const float RoundHeight = 20.f * Scale;
	for (int32 Round = 0; Round < 3; ++Round)
	{
		DrawBlock(Elements, Layer + 2, Geometry,
			At + FVector2D(24.f * Scale + Round * (RoundWidth + 4.f * Scale), Plate.Y * .5f - RoundHeight * .5f),
			FVector2D(RoundWidth, RoundHeight), Palette.Accent.CopyWithNewOpacity(.95f));
	}

	const FText Clip = FText::AsNumber(Displayed.AmmoInClip);
	const int32 ClipSize = FMath::RoundToInt(30.f * Scale);
	const FSlateFontInfo ClipFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(ClipSize, 6));
	const float ClipLeft = At.X + 66.f * Scale;
	DrawLabel(Elements, Layer + 2, Geometry, FVector2D(ClipLeft, At.Y + Plate.Y * .5f - ClipSize * .78f),
		Clip, Palette.Accent, ClipSize);
	// The reserve is quieter and trails the clip count, which is the number that matters in a fight.
	DrawLabel(Elements, Layer + 2, Geometry,
		FVector2D(ClipLeft + MeasureText(Clip, ClipFont) + 10.f * Scale, At.Y + Plate.Y * .5f - 2.f * Scale),
		FText::Format(LOCTEXT("AmmoReserve", "/ {0}"), FMath::Max(Displayed.AmmoReserve, 0)),
		Palette.Line.CopyWithNewOpacity(.75f), FMath::RoundToInt(14.f * Scale));
	return Layer + 3;
}

int32 USovHolographicHUDWidget::PaintEchoArc(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	if (Displayed.MaxEcho <= KINDA_SMALL_NUMBER) { return Layer; }
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const float Fraction = FMath::Clamp(Displayed.Echo / Displayed.MaxEcho, 0.f, 1.f);

	// A long shallow sweep across the bottom, leaving the left end where the radar sits and lifting
	// toward the right, as the references draw it. A quadratic curve is enough to describe it.
	const SovHolographicHUDLayout::FHUDGeometry Layout = SovHolographicHUDLayout::Compute(Geometry.GetLocalSize(), Scale);
	const auto PointAt = [&Layout](float T) { return Layout.ArcPoint(T); };

	// Segmented so charge is countable rather than estimated.
	const int32 Segments = 11;
	for (int32 Index = 0; Index < Segments; ++Index)
	{
		const float T0 = static_cast<float>(Index) / Segments;
		const float T1 = static_cast<float>(Index + 1) / Segments;
		const FVector2D A = PointAt(T0 + .005f);
		const FVector2D B = PointAt(T1 - .005f);
		// An unlit segment is drawn dark first and outlined second. At a fifth opacity it simply
		// disappeared against a white floor, leaving the lit end reading as a stray diagonal line
		// instead of the left end of a long bar.
		DrawSegment(Elements, Layer, Paint, A, B, FLinearColor(.02f, .03f, .05f, .55f), 16.f * Scale);
		DrawSegment(Elements, Layer + 1, Paint, A, B, Palette.Line.CopyWithNewOpacity(.55f), 15.f * Scale);
		if (Fraction > T0 + KINDA_SMALL_NUMBER)
		{
			// The final partial segment fills proportionally rather than snapping on.
			const float Fill = FMath::Clamp((Fraction - T0) * Segments, 0.f, 1.f);
			// A saturated body with a thinner bright core: the references light each segment from
			// inside rather than filling it flat, which is what kept this reading washed out.
			DrawSegment(Elements, Layer + 2, Paint, A, FMath::Lerp(A, B, Fill),
				Palette.Accent.CopyWithNewOpacity(.98f), 12.f * Scale);
			DrawSegment(Elements, Layer + 3, Paint, A, FMath::Lerp(A, B, Fill),
				Palette.ShieldTo.CopyWithNewOpacity(.85f), 5.f * Scale);
		}
	}

	// The centre marker, and the only word this element needs.
	const FVector2D Middle = PointAt(.5f);
	const float Mark = 7.f * Scale;
	DrawChevron(Elements, Layer + 3, Paint, Middle + FVector2D(-16.f * Scale, 0.f), Mark, -1.f,
		Palette.Accent.CopyWithNewOpacity(.9f), 2.f);
	DrawChevron(Elements, Layer + 3, Paint, Middle + FVector2D(16.f * Scale, 0.f), Mark, 1.f,
		Palette.Accent.CopyWithNewOpacity(.9f), 2.f);
	DrawLabel(Elements, Layer + 3, Geometry, FVector2D(Middle.X, Middle.Y + 16.f * Scale),
		LOCTEXT("Echo", "ECHO"), Palette.Line.CopyWithNewOpacity(.75f), FMath::RoundToInt(10.f * Scale),
		ETextAlign::Centre);
	return Layer + 5;
}

int32 USovHolographicHUDWidget::PaintRadar(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
	const FPalette& Palette, float Scale) const
{
	const FPaintGeometry Paint = Geometry.ToPaintGeometry();
	const SovHolographicHUDLayout::FHUDGeometry Layout = SovHolographicHUDLayout::Compute(Geometry.GetLocalSize(), Scale);
	const float Radius = Layout.RadarRadius;
	const FVector2D Centre = Layout.RadarCentre;

	// Its own colour rather than the shared veil. The backing alpha is tuned to sit behind text; a
	// disc that large at .55 washes to mid grey over the entry floor and reads as a blob rather than
	// an instrument. The references show a dark scope, and that only holds if it is genuinely dark.
	DrawDisc(Elements, Layer, Geometry, Centre, Radius,
		Palette.bHighContrast ? FLinearColor(0.f, 0.f, 0.f, 1.f) : FLinearColor(.006f, .012f, .022f, .78f));

	TArray<FVector2D> Ring;
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		BuildArc(Ring, Centre, Radius * (static_cast<float>(Step) / 3.f), 0.f, 360.f, 64);
		DrawPolyline(Elements, Layer + 1, Paint, Ring, Palette.Line.CopyWithNewOpacity(Step == 3 ? .95f : .45f),
			Step == 3 ? 2.f : 1.2f);
	}
	// Cardinal marks: which way is forward must be readable at a glance.
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		const float Angle = FMath::DegreesToRadians(90.f * Tick - 90.f);
		const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D Normal(-Direction.Y, Direction.X);
		DrawSegment(Elements, Layer + 1, Paint, Centre + Direction * Radius * 1.01f,
			Centre + Direction * Radius * 1.17f, Palette.Line.CopyWithNewOpacity(.8f), 2.f);
		DrawSegment(Elements, Layer + 1, Paint, Centre + Direction * Radius * 1.09f - Normal * 4.f * Scale,
			Centre + Direction * Radius * 1.09f + Normal * 4.f * Scale, Palette.Line.CopyWithNewOpacity(.55f), 1.4f);
	}
	// The sweep is a fading fan rather than a single line: Slate has no filled wedge, and a fan of
	// radial lines with falling opacity reads as the same cone.
	if (!Palette.bHighContrast)
	{
		const float Sweep = FMath::Fmod(SweepSeconds * 70.f, 360.f);
		const int32 Fan = 18;
		for (int32 Index = 0; Index < Fan; ++Index)
		{
			const float Angle = FMath::DegreesToRadians(Sweep - static_cast<float>(Index) * 2.4f);
			const FVector2D Direction(FMath::Cos(Angle), FMath::Sin(Angle));
			const float Opacity = (1.f - static_cast<float>(Index) / Fan) * .40f;
			DrawSegment(Elements, Layer + 2, Paint, Centre, Centre + Direction * Radius * .97f,
				Palette.Accent.CopyWithNewOpacity(Opacity), 2.4f);
		}
	}
	// The player's own facing, at the centre.
	TArray<FVector2D> Arrow;
	Arrow.Add(Centre + FVector2D(0.f, -12.f * Scale));
	Arrow.Add(Centre + FVector2D(8.f * Scale, 9.f * Scale));
	Arrow.Add(Centre + FVector2D(-8.f * Scale, 9.f * Scale));
	Arrow.Add(Centre + FVector2D(0.f, -12.f * Scale));
	DrawPolyline(Elements, Layer + 3, Paint, Arrow, Palette.Line.CopyWithNewOpacity(.95f), 2.f);
	Arrow.Reset();
	Arrow.Add(Centre + FVector2D(0.f, -7.f * Scale));
	Arrow.Add(Centre + FVector2D(4.f * Scale, 5.f * Scale));
	Arrow.Add(Centre + FVector2D(-4.f * Scale, 5.f * Scale));
	Arrow.Add(Centre + FVector2D(0.f, -7.f * Scale));
	DrawPolyline(Elements, Layer + 3, Paint, Arrow, Palette.Line.CopyWithNewOpacity(.8f), 2.f);

	for (const FSovProximityContact& Contact : Displayed.Contacts)
	{
		// Screen up is the player's facing, so bearing rotates clockwise from straight up.
		const float Angle = FMath::DegreesToRadians(Contact.BearingDegrees - 90.f);
		const FVector2D At = Centre + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Radius * Contact.NormalisedRange);
		const float Dot = (Contact.bLiveSighting ? 5.0f : 4.0f) * Scale;
		// A memory is drawn as an open mark, a live sighting as a filled one: certainty is visible.
		if (Contact.bLiveSighting)
		{
			DrawDisc(Elements, Layer + 4, Geometry, At, Dot, Palette.HealthTo.CopyWithNewOpacity(Contact.Alpha));
		}
		else
		{
			BuildArc(Ring, At, Dot, 0.f, 360.f, 12);
			DrawPolyline(Elements, Layer + 4, Paint, Ring, Palette.HealthTo.CopyWithNewOpacity(Contact.Alpha * .9f), 1.2f);
		}
	}
	return Layer + 5;
}

int32 USovHolographicHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	int32 Result = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
	// Counted before the validity gate, so a paint that ran but drew nothing is distinguishable
	// from a paint that never ran at all.
	++PaintCount;
	LastPaintSize = Geometry.GetLocalSize();
	LastCulling = FVector4(CullingRect.Left, CullingRect.Top, CullingRect.Right, CullingRect.Bottom);
	LastAbsolutePosition = Geometry.GetAbsolutePosition();
	if (CVarHolographicPaintProbe.GetValueOnGameThread() != 0)
	{
		const FVector2D ProbeSize(220.f, 90.f);
		const FVector2D At(60.f, 300.f);
		// Route one: exactly how the sibling overlay that does render submits a box.
		FSlateDrawElement::MakeBox(Elements, Result + 1,
			Geometry.ToPaintGeometry(ProbeSize, FSlateLayoutTransform(At)),
			FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(1.f, 0.f, 1.f, 1.f));
		// Route two: this file's own box helper, which uses a function-local static brush.
		DrawBlock(Elements, Result + 2, Geometry, At + FVector2D(240.f, 0.f), ProbeSize, FLinearColor(0.f, 1.f, 0.f, 1.f));
		// Route three: this file's own line helper.
		DrawSegment(Elements, Result + 3, Geometry.ToPaintGeometry(), At + FVector2D(480.f, 0.f),
			At + FVector2D(700.f, 90.f), FLinearColor(1.f, 1.f, 0.f, 1.f), 6.f);
	}
	if (!Displayed.bValid) { return Result; }
	if (const UWorld* const World = GetWorld()) { SweepSeconds = World->GetTimeSeconds(); }
	const FPalette Palette = BuildPalette();
	// Recorded rather than inferred: this is the last point before anything is submitted to draw.
	LastPalette = Palette;
	bLastPaintDrew = true;
	const float Scale = SovHolographicHUDLayout::ClampScale(Displayed.Settings.UIScale);
	// The edging frames the whole screen; every readout sits inside the title-safe area the subtitle surface
	// uses, so a TV that crops the frame never crops the HUD and both agree where the other is.
	Result = PaintEdging(Geometry, Elements, Result, Palette);
	FGeometry Safe = Geometry;
	FSlateRect SafeRect;
	if (SafeAreaSource.IsValid() && SafeAreaSource->GetSafeAreaAbsoluteRect(SafeRect))
	{
		const FVector2D Min = Geometry.AbsoluteToLocal(FVector2D(SafeRect.Left, SafeRect.Top));
		const FVector2D Max = Geometry.AbsoluteToLocal(FVector2D(SafeRect.Right, SafeRect.Bottom));
		if (!Min.ContainsNaN() && !Max.ContainsNaN() && Max.X - Min.X > 1. && Max.Y - Min.Y > 1.)
		{ Safe = Geometry.MakeChild(FVector2f(Max - Min), FSlateLayoutTransform(FVector2f(Min))); }
	}
	Result = PaintIdentityPlate(Safe, Elements, Result, Palette, Scale);
	Result = PaintAbilityPips(Safe, Elements, Result, Palette, Scale);
	Result = PaintAmmo(Safe, Elements, Result, Palette, Scale);
	Result = PaintEchoArc(Safe, Elements, Result, Palette, Scale);
	Result = PaintRadar(Safe, Elements, Result, Palette, Scale);
	return Result;
}

#undef LOCTEXT_NAMESPACE
