// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatVitalsWidget.h"
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
    Out = Current;
    return true;
}

TSharedRef<SWidget> USovCombatVitalsWidget::RebuildWidget()
{
    ValueLabels.Reset(); Bars.Reset(); VitalRows.Reset(); Displayed = {}; DisplayedScale = -1.f;
    Panel = nullptr; EchoPanel = nullptr;
    if (WidgetTree)
    {
        auto* SafeZone = WidgetTree->ConstructWidget<USafeZone>();
        WidgetTree->RootWidget = SafeZone;
        auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); SafeZone->SetContent(Canvas);
        const auto MakePanel = [&](const FName Name, const bool bEcho)
        {
            auto* Border = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
            Border->SetPadding(FMargin(12.f, 8.f));
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
        const FName RowNames[] = {TEXT("HealthVitalRow"), TEXT("ShieldVitalRow"), TEXT("StaminaVitalRow"),
                                 TEXT("PoiseVitalRow"), TEXT("EchoVitalRow")};
        for (int32 Index = 0; Index < 5; ++Index)
        {
            auto* Row = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), RowNames[Index]);
            auto* Rows = Index == 4 ? EchoRows : SurvivalRows;
            Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, Index == 0 || Index == 4 ? 0.f : 5.f, 0.f, 0.f));
            auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
            Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 16));
            Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            Row->AddChildToVerticalBox(Label)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
            auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(6.f);
            auto* Bar = WidgetTree->ConstructWidget<UProgressBar>(); Height->SetContent(Bar);
            Row->AddChildToVerticalBox(Height);
            VitalRows.Add(Row); ValueLabels.Add(Label); Bars.Add(Bar);
        }
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
        Displayed = {}; return;
    }
    const auto* UserSettings = USovGameUserSettings::Get();
    const auto Settings = UserSettings ? UserSettings->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
    const float Scale = FMath::IsFinite(Settings.UIScale) ? FMath::Clamp(Settings.UIScale, .75f, 2.f) : 1.f;
    const bool bRestyle = DisplayedScale != Scale || bDisplayedHighContrast != Settings.bHighContrastHUD;
    if (bRestyle)
    {
        Panel->SetRenderScale(FVector2D(Scale));
        EchoPanel->SetRenderScale(FVector2D(Scale));
        const FLinearColor Background(.025f, .035f, .055f, Settings.bHighContrastHUD ? 1.f : .9f);
        Panel->SetBrushColor(Background); EchoPanel->SetBrushColor(Background);
        if (auto* EchoSlot = Cast<UCanvasPanelSlot>(EchoPanel->Slot))
        { EchoSlot->SetPosition(FVector2D(-24.f, -24.f - 144.f * FMath::Max(1.f, Scale))); }
        DisplayedScale = Scale; bDisplayedHighContrast = Settings.bHighContrastHUD;
    }
    const FText Names[] = {LOCTEXT("Health", "Health"), LOCTEXT("Shield", "Shield"), LOCTEXT("Stamina", "Stamina"),
                          LOCTEXT("Poise", "Poise"), LOCTEXT("Echo", "Echo")};
    const FLinearColor Colors[] = {FLinearColor(.95f, .24f, .3f), FLinearColor(.15f, .72f, 1.f),
        FLinearColor(.3f, .9f, .45f), FLinearColor(1.f, .7f, .2f), FLinearColor(.73f, .43f, 1.f)};
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
    // Keep health (including damage outside combat) visible. A complete six-second fade must
    // consume a real participation/threat owner; weapon draw or damage alone cannot prove safety.
    Displayed = Current;
    Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
    EchoPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
}
#undef LOCTEXT_NAMESPACE
