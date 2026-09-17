// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovHolographicHUDSurface.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "SovHolographicHUD"

FVector2D USovHolographicHUDSurface::ArcPoint(float T) const
{
	if (!FMath::IsFinite(T)) { return View.ArcStart; }
	const float Clamped = FMath::Clamp(T, 0.f, 1.f);
	const float U = 1.f - Clamped;
	return View.ArcStart * (U * U) + View.ArcControl * (2.f * U * Clamped) + View.ArcEnd * (Clamped * Clamped);
}

void USovHolographicHUDSurface::NativeConstruct()
{
	Super::NativeConstruct();
	// Constructed after the widget tree exists, so a frame that arrived first is applied now.
	ApplyBoundWidgets();
}

void USovHolographicHUDSurface::ApplyHolographicHUDView(const FSovHolographicHUDView& InView)
{
	View = InView;
	// A surface that is not constructed has no widget tree to update yet; the next refresh publishes again.
	if (!IsConstructed()) { return; }
	ApplyBoundWidgets();
	OnHolographicHUDUpdated(View);
}

void USovHolographicHUDSurface::PlaceRegion(UPanelWidget* Region, const FBox2D& Box) const
{
	if (!IsValid(Region) || !Box.bIsValid) { return; }
	auto* CanvasSlot = Cast<UCanvasPanelSlot>(Region->Slot);
	if (!CanvasSlot) { return; }
	const FVector2D Size = Box.Max - Box.Min;
	if (Size.X <= 0. || Size.Y <= 0. || Box.Min.ContainsNaN() || Size.ContainsNaN()) { return; }
	// Anchored to the top-left of the safe area, which is the space the layout rectangles are in.
	CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	CanvasSlot->SetAlignment(FVector2D::ZeroVector);
	CanvasSlot->SetAutoSize(false);
	CanvasSlot->SetOffsets(FMargin(static_cast<float>(Box.Min.X), static_cast<float>(Box.Min.Y),
		static_cast<float>(Size.X), static_cast<float>(Size.Y)));
}

void USovHolographicHUDSurface::ApplyBoundWidgets()
{
	const auto Show = [](UWidget* Widget, bool bVisible)
	{
		if (IsValid(Widget))
		{ Widget->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	};
	const auto Fill = [](UProgressBar* Bar, const FSovHolographicHUDBar& Value)
	{
		if (IsValid(Bar)) { Bar->SetPercent(FMath::Clamp(Value.Fraction, 0.f, 1.f)); }
	};

	if (bPlaceRegionsFromLayout)
	{
		PlaceRegion(PlateRegion, View.Plate);
		PlaceRegion(AmmoRegion, View.Ammo);
		PlaceRegion(ArcRegion, View.Arc);
		PlaceRegion(RadarRegion, View.Radar);
	}

	Fill(HealthBar, View.Health);
	Fill(ShieldBar, View.Shield);
	Fill(StaminaBar, View.Stamina);
	Fill(EchoBar, View.Echo);

	// A weapon without a magazine hides the readout rather than reporting zero rounds.
	Show(AmmoRegion, View.bHasAmmo);
	if (IsValid(AmmoText))
	{
		AmmoText->SetText(View.bHasAmmo
			? FText::Format(LOCTEXT("AmmoReadout", "{0} / {1}"),
				FText::AsNumber(View.AmmoInClip), FText::AsNumber(View.AmmoReserve))
			: FText::GetEmpty());
	}

	if (IsValid(ArcFill) && !ArcFillParameter.IsNone())
	{
		if (!ArcMaterial) { ArcMaterial = ArcFill->GetDynamicMaterial(); }
		// Absent art is normal while a HUD is being authored; the readout simply is not driven yet.
		if (ArcMaterial) { ArcMaterial->SetScalarParameterValue(ArcFillParameter, FMath::Clamp(View.Echo.Fraction, 0.f, 1.f)); }
	}
}

#undef LOCTEXT_NAMESPACE
