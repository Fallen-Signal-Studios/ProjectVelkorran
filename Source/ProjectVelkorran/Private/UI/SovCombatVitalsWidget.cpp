// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatVitalsWidget.h"
#include "UI/SovHUDStyle.h"
#include "UI/SovCombatReadinessWidget.h"
#include "Brushes/SlateColorBrush.h"
#include "Rendering/DrawElementTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Exertion/SovExertionComponent.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Settings/SovGameUserSettings.h"
#include "Styling/CoreStyle.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "SovCombatVitals"
USovCombatVitalsWidget::USovCombatVitalsWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

bool USovCombatVitalsWidget::ReadCurrentVitals(const ASovPlayerController* Controller, FSovCombatVitalsSnapshot& Out)
{
    Out = {};
    if (!IsValid(Controller) || Controller->IsActorBeingDestroyed()
        || Controller->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return false; }
    auto* Pawn = Cast<ASovPlayerCharacterBase>(Controller->GetPawn());
    auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Controller->GetAbilitySystemComponent());
    if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !Pawn->IsCharacterReady()
        || Pawn->GetController() != Controller || !IsValid(ASC) || ASC->GetAvatarActor() != Pawn
        || Pawn->GetAbilitySystemComponent() != ASC) { return false; }
    // This nonessential overlay follows the same owner-counted hide contract as Narrative's gameplay HUD.
    // Parent matching also handles WantsHideHUD.All without inventing separate menu/pause rules.
    if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Player_WantsHideHUD)) { return false; }
    const auto* Attributes = ASC->GetSet<UNarrativeAttributeSetBase>();
    const auto* Shield = Pawn->GetShieldComponent();
    const auto* Poise = Pawn->GetPoiseComponent();
    const auto* Echo = Pawn->GetEchoComponent();
    const auto* Exertion = Pawn->GetExertionComponent();
    if (!IsValid(Attributes) || Attributes != Pawn->GetAttributeSetBase()
        || !IsValid(Shield) || !Shield->IsInitialized() || !IsValid(Poise) || !Poise->IsInitialized()
        || !IsValid(Echo) || !Echo->IsInitialized() || !IsValid(Exertion) || !Exertion->IsInitialized()) { return false; }
    FSovCombatVitalsSnapshot Current;
    Current.Values[0] = {Pawn->GetHealth(), Pawn->GetMaxHealth()};
    Current.Values[1] = {Shield->GetShield(), Shield->GetMaxShield()};
    Current.Values[2] = {Exertion->GetStamina(), Pawn->GetMaxStamina()};
    Current.Values[3] = {Poise->GetPoise(), Poise->GetMaxPoise()};
    Current.Values[4] = {Echo->GetEcho(), Echo->GetMaxEcho()};
    for (const auto& Value : Current.Values)
    {
        if (!FMath::IsFinite(Value.Current) || !FMath::IsFinite(Value.Maximum)
            || Value.Current < 0.f || Value.Maximum < 0.f) { return false; }
    }
    Current.Pawn = Pawn;
    Current.Protagonist = Pawn->GetProtagonistIdentityTag();
    Out = Current;
    return true;
}

TSharedRef<SWidget> USovCombatVitalsWidget::RebuildWidget()
{
    RetireQuietSources();
    ValueLabels.Reset(); Bars.Reset(); VitalRows.Reset(); Displayed = {}; DisplayedScale = -1.f;
    Panel = nullptr; EchoPanel = nullptr; IdentityLabel = nullptr; EchoLabel = nullptr;
    AbilityReadiness = nullptr; AbilityReadinessSize = nullptr; CompanionStatus = nullptr;
    if (WidgetTree)
    {
        auto* SafeZone = WidgetTree->ConstructWidget<USafeZone>();
        WidgetTree->RootWidget = SafeZone;
        auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); SafeZone->SetContent(Canvas);
        const auto MakePanel = [&](const FName Name, const bool bEcho)
        {
            auto* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
            Border->SetPadding(bEcho ? FMargin(0.f) : FMargin(12.f, 8.f));
            const FVector2D Corner(bEcho ? 1.f : 0.f, 1.f);
            Border->SetRenderTransformPivot(Corner);
            auto* CanvasSlot = Canvas->AddChildToCanvas(Border);
            CanvasSlot->SetAnchors(FAnchors(bEcho ? 1.f : 0.f, 1.f)); CanvasSlot->SetAlignment(Corner);
            CanvasSlot->SetAutoSize(true);
            // The authored weapon/ammo group occupies the bottom-right 24px margin.
            // Reserve a separate band above it; RefreshVitals also scales that clearance.
            CanvasSlot->SetPosition(bEcho ? FVector2D(-24.f, -168.f) : FVector2D(24.f, -24.f));
            auto* Width = WidgetTree->ConstructWidget<USizeBox>(); Width->SetWidthOverride(300.f); Border->SetContent(Width);
            auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); Width->SetContent(Rows);
            Border->SetVisibility(ESlateVisibility::Collapsed);
            return Border;
        };
        Panel = MakePanel(TEXT("SurvivalVitalsPanel"), false);
        EchoPanel = MakePanel(TEXT("EchoVitalsPanel"), true);
        auto* SurvivalRows = CastChecked<UVerticalBox>(CastChecked<USizeBox>(Panel->GetContent())->GetContent());
        auto* EchoRows = CastChecked<UVerticalBox>(CastChecked<USizeBox>(EchoPanel->GetContent())->GetContent());
        const auto AddHeader = [&](UVerticalBox* Rows, FName Name)
        {
            auto* Header = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
            Header->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
            Header->SetAutoWrapText(true);
            Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
            return Header;
        };
        IdentityLabel = AddHeader(SurvivalRows, TEXT("ProtagonistIdentity"));
        EchoLabel = AddHeader(EchoRows, TEXT("EchoIdentity"));
        CompanionStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CompanionReadiness"));
        CompanionStatus->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 13));
        CompanionStatus->SetAutoWrapText(true);
        CompanionStatus->SetVisibility(ESlateVisibility::Collapsed);
        SurvivalRows->AddChildToVerticalBox(CompanionStatus)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        EchoLabel->SetText(SovHUDStyle::Text(TEXT("Echo")));
        // The actual resource row already names Echo; leave no redundant boxed-strip heading.
        EchoLabel->SetVisibility(ESlateVisibility::Collapsed);
        const FName RowNames[] = {TEXT("HealthVitalRow"), TEXT("ShieldVitalRow"), TEXT("StaminaVitalRow"),
                                 TEXT("PoiseVitalRow"), TEXT("EchoVitalRow")};
        for (int32 Index = 0; Index < 5; ++Index)
        {
            auto* Row = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), RowNames[Index]);
            auto* Rows = Index == 4 ? EchoRows : SurvivalRows;
            Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, Index == 0 || Index == 4 ? 0.f : 5.f, 0.f, 0.f));
            auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Index == 4 ? 13 : 16));
            if (Index == 4) { Label->SetShadowOffset(FVector2D(1.f)); Label->SetShadowColorAndOpacity(FLinearColor::Black); }
            Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            Row->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
            auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(Index == 0 ? 10.f : Index == 4 ? 3.f : 6.f);
            auto* Bar = WidgetTree->ConstructWidget<UProgressBar>(); Height->SetContent(Bar);
            FProgressBarStyle BarStyle;
            BarStyle.SetBackgroundImage(FSlateColorBrush(FLinearColor(.12f, .15f, .19f, .9f)));
            BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
            Bar->SetWidgetStyle(BarStyle);
            Row->AddChildToVerticalBox(Height);
            VitalRows.Add(Row); ValueLabels.Add(Label); Bars.Add(Bar);
        }
        AbilityReadiness = WidgetTree->ConstructWidget<USovCombatReadinessWidget>(USovCombatReadinessWidget::StaticClass(), TEXT("AbilityReadiness"));
        AbilityReadiness->SetOwningPlayer(GetOwningPlayer());
        AbilityReadinessSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("AbilityReadinessSize"));
        AbilityReadinessSize->SetHeightOverride(112.f); AbilityReadinessSize->SetContent(AbilityReadiness);
        EchoRows->AddChildToVerticalBox(AbilityReadinessSize)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
    }
    return Super::RebuildWidget();
}

void USovCombatVitalsWidget::RefreshVitals()
{
    if (!Panel || !EchoPanel || VitalRows.Num() != 5 || ValueLabels.Num() != 5 || Bars.Num() != 5) { return; }
    FSovCombatVitalsSnapshot Current;
    if (!ReadCurrentVitals(Cast<ASovPlayerController>(GetOwningPlayer()), Current))
    {
        Panel->SetVisibility(ESlateVisibility::Collapsed); EchoPanel->SetVisibility(ESlateVisibility::Collapsed);
        if (AbilityReadiness) { AbilityReadiness->Present({}); }
        if (CompanionStatus) { CompanionStatus->SetText(FText::GetEmpty()); CompanionStatus->SetVisibility(ESlateVisibility::Collapsed); }
        RetireQuietSources(); Displayed = {}; return;
    }
    auto* PC = CastChecked<ASovPlayerController>(GetOwningPlayer());
    BindQuietSources(PC);
    auto* Pawn = CastChecked<ASovPlayerCharacterBase>(Current.Pawn.Get());
    auto* ASC = CastChecked<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent());
    const double Now = GetWorld()->GetTimeSeconds();
    if (Displayed.Pawn != Current.Pawn || QuietActorInfoEpoch != ASC->GetCombatActorInfoEpoch())
    { QuietState.Reset(); NextThreatRead=-1.; bCachedThreat=false; QuietActorInfoEpoch=ASC->GetCombatActorInfoEpoch(); }
    if (NextThreatRead<0. || Now>=NextThreatRead || Now+0.25<NextThreatRead)
    { bCachedThreat=SovCombatHUDQuiet::HasNativeThreat(Pawn); NextThreatRead=Now+0.25; }
    const auto* UserSettings = USovGameUserSettings::Get();
    const auto Settings = UserSettings ? UserSettings->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
    const float Scale = FMath::IsFinite(Settings.UIScale) ? FMath::Clamp(Settings.UIScale, .75f, 2.f) : 1.f;
    const auto Theme = SovHUDStyle::ForProtagonist(Current.Protagonist, Settings.bHighContrastHUD);
    const bool bRestyle = DisplayedScale != Scale || bDisplayedHighContrast != Settings.bHighContrastHUD
        || Displayed.Protagonist != Current.Protagonist || Displayed.Pawn != Current.Pawn;
    if (bRestyle)
    {
        Panel->SetRenderScale(FVector2D(Scale));
        EchoPanel->SetRenderScale(FVector2D(Scale));
        Panel->SetBrushColor(Theme.Background); EchoPanel->SetBrushColor(FLinearColor::Transparent);
        IdentityLabel->SetText(Theme.Identity);
        IdentityLabel->SetColorAndOpacity(FSlateColor(Theme.Accent));
        EchoLabel->SetColorAndOpacity(FSlateColor(Theme.Accent));
        if (auto* EchoSlot = Cast<UCanvasPanelSlot>(EchoPanel->Slot))
        { EchoSlot->SetPosition(FVector2D(-24.f, -24.f - 144.f * FMath::Max(1.f, Scale))); }
        DisplayedScale = Scale; bDisplayedHighContrast = Settings.bHighContrastHUD;
    }
    const FText Names[] = {LOCTEXT("Health", "Health"), LOCTEXT("Shield", "Shield"), LOCTEXT("Stamina", "Stamina"),
                          LOCTEXT("Poise", "Poise"), LOCTEXT("Echo", "Echo")};
    // Shape, stable placement and explicit labels identify the protagonist even without color.
    const FLinearColor Colors[] = {FLinearColor(.92f, .96f, 1.f), Theme.Accent,
        FLinearColor(.6f, .82f, .78f), Theme.Accent, Theme.Accent};
    FNumberFormattingOptions NumberFormat; NumberFormat.SetMaximumFractionalDigits(0);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const auto& Value = Current.Values[Index];
        if (bRestyle || Displayed.Pawn != Current.Pawn || Value.Current != Displayed.Values[Index].Current
            || Value.Maximum != Displayed.Values[Index].Maximum)
        {
            ValueLabels[Index]->SetText(FText::Format(LOCTEXT("Value", "{0}   {1} / {2}"), Names[Index],
                FText::AsNumber(Value.Current, &NumberFormat), FText::AsNumber(Value.Maximum, &NumberFormat)));
            Bars[Index]->SetPercent(Value.Maximum > KINDA_SMALL_NUMBER ? FMath::Clamp(Value.Current / Value.Maximum, 0.f, 1.f) : 0.f);
            Bars[Index]->SetFillColorAndOpacity(Settings.bHighContrastHUD ? FLinearColor::White : Colors[Index]);
        }
    }
    const auto& Stamina = Current.Values[2];
    const bool bStaminaChanging = Displayed.Pawn == Current.Pawn &&
        (!FMath::IsNearlyEqual(Stamina.Current, Displayed.Values[2].Current)
            || !FMath::IsNearlyEqual(Stamina.Maximum, Displayed.Values[2].Maximum));
    const bool bShowStamina = Stamina.Current < Stamina.Maximum - KINDA_SMALL_NUMBER || bStaminaChanging;
    VitalRows[2]->SetVisibility(bShowStamina ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    // Keep the requested UI scale. Narrow only this three-image strip when the actual
    // safe canvas cannot fit it beside the survival panel, retaining measured key/status wraps.
    const auto* SafeCanvas = EchoPanel->GetParent();
    const float SafeWidth = SafeCanvas ? SafeCanvas->GetCachedGeometry().GetLocalSize().X : 0.f;
    const float AbilityWidth = SafeWidth > 0.f
        ? FMath::Clamp((SafeWidth - 72.f - 324.f * Scale) / Scale, 90.f, 300.f) : 300.f;
    if (auto* Width = Cast<USizeBox>(EchoPanel->GetContent())) { Width->SetWidthOverride(AbilityWidth); }
    FSovCombatReadinessSnapshot Readiness;
    SovCombatReadiness::Read(Cast<ASovPlayerController>(GetOwningPlayer()), Readiness);
    if (AbilityReadiness)
    {
        AbilityReadiness->Present(Readiness);
        // Measure the actual remapped key and status text at the current strip width.
        if (AbilityReadinessSize) { AbilityReadinessSize->SetHeightOverride(AbilityReadiness->GetPresentationHeight(AbilityWidth)); }
    }
    if (AbilityReadinessSize) { AbilityReadinessSize->SetVisibility(Readiness.Abilities.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (CompanionStatus)
    {
        CompanionStatus->SetText(Readiness.CompanionText);
        CompanionStatus->SetColorAndOpacity(FSlateColor(Theme.Accent));
        CompanionStatus->SetVisibility(Readiness.CompanionText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    bool bResourcesChanged=false, bSurvivalNeedsAttention=false;
    for (int32 Index=0; Index<5; ++Index)
    {
        const auto& Value=Current.Values[Index];
        bResourcesChanged |= Displayed.Pawn==Current.Pawn &&
            (!FMath::IsNearlyEqual(Value.Current,Displayed.Values[Index].Current)
                || !FMath::IsNearlyEqual(Value.Maximum,Displayed.Values[Index].Maximum));
        if (Index<4) { bSurvivalNeedsAttention |= Value.Maximum>0.f && Value.Current<Value.Maximum-KINDA_SMALL_NUMBER; }
    }
    if (bRestyle || bResourcesChanged) { QuietState.Wake(Now); }
    const bool bAction=Readiness.Abilities.ContainsByPredicate([](const FSovAbilityHUDEntry& Entry)
        { return Entry.State==ESovAbilityHUDState::Active; }) || PC->IsGameplayAbilityInputSuppressed() || GetWorld()->IsPaused();
    const float Opacity=QuietState.Update(Pawn,QuietActorInfoEpoch,Now,
        Pawn->GetEchoComponent()->GetSecondsSinceCombatActivity(),bCachedThreat,bAction);
    // Damage, depleted survival resources and committed companion status remain legible.
    const float SurvivalOpacity=(bSurvivalNeedsAttention || !Readiness.CompanionText.IsEmpty()) ? 1.f : Opacity;
    Panel->SetRenderOpacity(SurvivalOpacity); EchoPanel->SetRenderOpacity(Opacity);
    Displayed = Current;
    Panel->SetVisibility(SurvivalOpacity>0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    EchoPanel->SetVisibility(Opacity>0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
int32 USovCombatVitalsWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer,
    const FWidgetStyle& Style, bool bParentEnabled) const
{
    Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
    if (!Displayed.Pawn.IsValid()) { return Layer; }
    const auto Theme = SovHUDStyle::ForProtagonist(Displayed.Protagonist, bDisplayedHighContrast);
    const float Scale = FMath::Max(1.f, DisplayedScale);
    float FrameOpacity=1.f;
    const auto Lines = [&](const TArray<FVector2D>& Points, float Width)
    {
        TArray<FVector2f> SlatePoints;
        for (const auto& Point : Points) { SlatePoints.Add(FVector2f(Point)); }
        FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), SlatePoints,
            ESlateDrawEffect::None, Theme.Accent.CopyWithNewOpacity(Theme.Accent.A*FrameOpacity), true, Width * Scale);
    };
    // Child paint geometry shares this window origin; tick geometry includes the desktop offset.
    // Only survival retains a faction plate. Echo is an invisible layout/fade/avoidance owner.
    for (const auto* Border : {Panel.Get()})
    {
        if (!Border || Border->GetVisibility() == ESlateVisibility::Collapsed) { continue; }
        FrameOpacity=Border->GetRenderOpacity();
        const auto& PanelGeometry = Border->GetPaintSpaceGeometry();
        const FVector2D Min = Geometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector));
        const FVector2D Max = Geometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize()));
        if (Max.X <= Min.X || Max.Y <= Min.Y) { continue; }
        const double Cut = 12. * Scale;
        if (Theme.Frame == SovHUDStyle::EFrame::Shield)
        {
            // Broad, closed shield plate with clipped lower shoulders.
            Lines({Min, FVector2D(Max.X, Min.Y), FVector2D(Max.X, Max.Y-Cut), FVector2D(Max.X-Cut, Max.Y),
                FVector2D(Min.X+Cut, Max.Y), FVector2D(Min.X, Max.Y-Cut), Min}, 2.f);
            Lines({Min+FVector2D(8, 5)*Scale, FVector2D(Min.X+54*Scale, Min.Y+5*Scale)}, 3.f);
        }
        else
        {
            // Fine separated brackets retain their gaps in high contrast and with motion disabled.
            const double Arm = 34. * Scale;
            Lines({FVector2D(Min.X, Min.Y+Arm), Min, FVector2D(Min.X+Arm, Min.Y)}, 1.f);
            Lines({FVector2D(Max.X-Arm, Min.Y), FVector2D(Max.X, Min.Y), FVector2D(Max.X, Min.Y+Arm)}, 1.f);
            Lines({FVector2D(Min.X, Max.Y-Arm), FVector2D(Min.X, Max.Y), FVector2D(Min.X+Arm, Max.Y)}, 1.f);
            Lines({FVector2D(Max.X-Arm, Max.Y), Max, FVector2D(Max.X, Max.Y-Arm)}, 1.f);
        }
    }
    return Layer;
}
void USovCombatVitalsWidget::RetireQuietSources()
{
    if (QuietController.IsValid()) { QuietController->OnSemanticInputChanged.RemoveDynamic(this,&ThisClass::HandleQuietInput); }
    if (QuietInteraction.IsValid())
    {
        QuietInteraction->OnInteractPressed.RemoveDynamic(this,&ThisClass::HandleQuietInteraction);
        QuietInteraction->OnInteractReleased.RemoveDynamic(this,&ThisClass::HandleQuietInteraction);
    }
    if (QuietSettings.IsValid()) { QuietSettings->OnUserSettingsChanged.RemoveDynamic(this,&ThisClass::HandleQuietSettings); }
    QuietController.Reset(); QuietInteraction.Reset(); QuietSettings.Reset();
    QuietState.Reset(); NextThreatRead=-1.; bCachedThreat=false; QuietActorInfoEpoch=0;
}
void USovCombatVitalsWidget::BindQuietSources(ASovPlayerController* Controller)
{
    auto* Interaction=Controller->GetInteractionComponent();
    auto* Settings=USovGameUserSettings::Get();
    if (QuietController.Get()==Controller && QuietInteraction.Get()==Interaction && QuietSettings.Get()==Settings) { return; }
    RetireQuietSources(); QuietController=Controller; QuietInteraction=Interaction; QuietSettings=Settings;
    Controller->OnSemanticInputChanged.AddUniqueDynamic(this,&ThisClass::HandleQuietInput);
    if (Interaction)
    {
        Interaction->OnInteractPressed.AddUniqueDynamic(this,&ThisClass::HandleQuietInteraction);
        Interaction->OnInteractReleased.AddUniqueDynamic(this,&ThisClass::HandleQuietInteraction);
    }
    if (Settings) { Settings->OnUserSettingsChanged.AddUniqueDynamic(this,&ThisClass::HandleQuietSettings); }
}
void USovCombatVitalsWidget::WakeQuietHUD()
{
    if (QuietController.Get()!=GetOwningPlayer() || !GetWorld()) { return; }
    FSovCombatVitalsSnapshot Current;
    if (ReadCurrentVitals(QuietController.Get(),Current)) { QuietState.Wake(GetWorld()->GetTimeSeconds()); }
}
void USovCombatVitalsWidget::HandleQuietInput(FGameplayTag InputTag, bool bPressed)
{
    const auto& Tags=FNarrativeGameplayTags::Get();
    // Ordinary exploration movement should not defeat a combat-only fade.
    if (InputTag==Tags.Narrative_Input_None || InputTag==Tags.Narrative_Input_Jump
        || InputTag==Tags.Narrative_Input_Crouch || InputTag==Tags.Narrative_Input_Sprint) { return; }
    WakeQuietHUD(); // A real press or release/toggle cancellation restores the readout.
}
void USovCombatVitalsWidget::HandleQuietInteraction(UNarrativeInteractionComponent* Interaction)
{ if (QuietInteraction.Get()==Interaction) { WakeQuietHUD(); } }
void USovCombatVitalsWidget::HandleQuietSettings(const FSovUserSettingsSnapshot& Settings)
{ WakeQuietHUD(); }
void USovCombatVitalsWidget::NativeDestruct()
{ RetireQuietSources(); Displayed={}; Super::NativeDestruct(); }
#undef LOCTEXT_NAMESPACE
