// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovWeaponWheelGlass.h"
#include "Blueprint/WidgetTree.h"
#include "CommonLazyImage.h"
#include "CommonActivatableWidget.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Engine/Texture.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/SovPlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Settings/SovGameUserSettings.h"
#include "Styling/CoreStyle.h"
#include "UI/SovCombatVitalsWidget.h"
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovFrontendComponent.h"
#include "UI/SovWeaponWheelLayout.h"
#include "Widgets/Layout/SBox.h"

USovWeaponWheelGlass::USovWeaponWheelGlass(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> USovWeaponWheelGlass::RebuildWidget()
{
    return SNew(SBox).WidthOverride(600.f).HeightOverride(600.f);
}

void USovWeaponWheelGlass::ResetLayout() { PlacedWheel = FBox2D(ForceInit); }
void USovWeaponWheelGlass::NativeConstruct()
{
    Super::NativeConstruct(); ResetLayout();
    if (auto* Menu = GetTypedOuter<UCommonActivatableWidget>())
    {
        Menu->OnActivated().RemoveAll(this);
        Menu->OnActivated().AddUObject(this, &USovWeaponWheelGlass::ResetLayout);
    }
}
void USovWeaponWheelGlass::NativeDestruct()
{
    if (auto* Menu = GetTypedOuter<UCommonActivatableWidget>()) { Menu->OnActivated().RemoveAll(this); }
    Super::NativeDestruct();
}

void USovWeaponWheelGlass::UpdatePlacement(UUserWidget* Menu)
{
    auto* Frame = Cast<UScaleBox>(Menu->WidgetTree->FindWidget(TEXT("WheelSafeFrame")));
    auto* FrameSlot = Frame ? Cast<UCanvasPanelSlot>(Frame->Slot) : nullptr;
    auto* PC = Cast<ASovPlayerController>(GetOwningPlayer());
    auto* Presentation = PC && PC->GetFrontend() ? PC->GetFrontend()->GetPresentation() : nullptr;
    if (!FrameSlot || !Frame->GetParent() || !Presentation) { return; }
    const FGeometry& Canvas = Frame->GetParent()->GetCachedGeometry();
    if (Canvas.GetLocalSize().IsNearlyZero()) { return; }
    FSlateRect AbsoluteSafe;
    if (!Presentation->GetSafeAreaAbsoluteRect(AbsoluteSafe)) { return; }
    const auto LocalRect = [&](const FSlateRect& R)
    {
        return FBox2D(Canvas.AbsoluteToLocal(FVector2D(R.Left,R.Top)),Canvas.AbsoluteToLocal(FVector2D(R.Right,R.Bottom)));
    };
    FBox2D Safe = LocalRect(AbsoluteSafe);
    Safe.Min += FVector2D(12,12); Safe.Max -= FVector2D(12,12);
    TArray<FSlateRect> HUD;
    Presentation->GetHolographicHUDRegions(HUD);
    TArray<FBox2D> Panels;
    for (const auto& R : HUD) { Panels.Add(LocalRect(R)); }
    for (const UBorder* Panel : {Presentation->GetSubtitlePanel(),Presentation->GetCaptionPanel(),Presentation->GetObjectivePanel()})
    {
        if (!Panel || !Panel->IsRendered()) { continue; }
        const FGeometry& G = Panel->GetPaintSpaceGeometry();
        if (G.GetLocalSize().IsNearlyZero()) { continue; }
        Panels.Emplace(Canvas.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector)),Canvas.AbsoluteToLocal(G.LocalToAbsolute(G.GetLocalSize())));
    }
    FBox2D Result;
    if (SovWeaponWheelLayout::Place(Safe,Panels,600.,PlacedWheel,Result))
    {
        PlacedWheel=Result;
        FrameSlot->SetPosition(Result.Min); FrameSlot->SetSize(Result.GetSize());
    }
    Presentation->SetWeaponWheelSurface(Frame,Cast<UCommonActivatableWidget>(Menu));
}

void USovWeaponWheelGlass::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    FSovCombatVitalsSnapshot Vitals;
    USovCombatVitalsWidget::ReadCurrentVitals(Cast<ASovPlayerController>(GetOwningPlayer()), Vitals);
    const auto* Settings = USovGameUserSettings::Get();
    const bool bHighContrast = Settings && Settings->GetSettingsSnapshot().bHighContrastHUD;
    Theme = SovHUDStyle::ForProtagonist(Vitals.Protagonist, bHighContrast);
    // This child lives in the menu's tree, including when that tree is inherited by the weapon wheel.
    // Resolve the live brush each frame: Blueprint owns its MID and may rebuild it on activation.
    auto* Menu = GetTypedOuter<UUserWidget>();
    if (!Menu || !Menu->WidgetTree) { return; }
    UpdatePlacement(Menu);
    if (auto* Image = Cast<UImage>(Menu->WidgetTree->FindWidget(TEXT("Image_WeaponWheel"))))
    {
        if (auto* Material = Image->GetDynamicMaterial())
        {
            Material->SetVectorParameterValue(TEXT("ProtagonistAccent"), Theme.Accent);
            Material->SetScalarParameterValue(TEXT("HighContrast"), bHighContrast ? 1.f : 0.f);
            Material->SetScalarParameterValue(TEXT("DominionFrame"), Theme.Frame == SovHUDStyle::EFrame::Shield ? 1.f : 0.f);
        }
    }
    if (auto* Title = Cast<UTextBlock>(Menu->WidgetTree->FindWidget(TEXT("CommonTextBlock_RadialTitle"))))
    {
        Title->SetColorAndOpacity(FSlateColor(Theme.Accent));
    }
    auto* Items = Cast<UPanelWidget>(Menu->WidgetTree->FindWidget(TEXT("Overlay_RadialItems")));
    if (!WeaponGlyphMaterial || !Items) { return; }
    for (UWidget* Child : Items->GetAllChildren())
    {
        auto* Item = Cast<UUserWidget>(Child);
        if (!Item || !Item->WidgetTree) { continue; }
        auto* Icon = Cast<UImage>(Item->WidgetTree->FindWidget(TEXT("CommonLazyImage_RadialIcon")));
        if (!Icon) { continue; }
        // Setting a brush cancels UCommonLazyImage's pending stream. Do not capture its placeholder.
        if (const auto* Lazy = Cast<UCommonLazyImage>(Icon); Lazy && Lazy->IsLoading()) { continue; }
        UObject* Resource = Icon->GetBrush().GetResourceObject();
        auto* Glyph = Cast<UMaterialInstanceDynamic>(Resource);
        if (auto* Texture = Cast<UTexture>(Resource))
        {
            Glyph = UMaterialInstanceDynamic::Create(WeaponGlyphMaterial, this);
            Glyph->SetTextureParameterValue(TEXT("WeaponTexture"), Texture);
            Icon->SetBrushFromMaterial(Glyph);
        }
        // A delayed CommonLazyImage load can replace the brush; consume its new texture next tick.
        if (Glyph && Glyph->Parent == WeaponGlyphMaterial)
        {
            Glyph->SetVectorParameterValue(TEXT("ProtagonistAccent"), FMath::Lerp(Theme.Accent, FLinearColor::White, .65f));
        }
    }
}

int32 USovWeaponWheelGlass::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
    const FVector2D Size = Geometry.GetLocalSize();
    // The inscription is outside the selection annulus and never intercepts pointer input.
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", 14);
    if (!FSlateApplication::IsInitialized()) { return Layer; }
    const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Theme.Identity, Font);
    FSlateDrawElement::MakeText(Elements, ++Layer,
        Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(FVector2D((Size.X-TextSize.X)*.5,Size.Y-18))),
        Theme.Identity, Font, ESlateDrawEffect::None, Theme.Accent);
    return Layer;
}
