// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionWorldPresentation.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovAurelionMissionDefinition.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Settings/SovGameUserSettings.h"
#include "UI/SovThreatCueLayout.h"
#include "UI/SovWidgetGeometry.h"
#include "UI/SovFrontendComponent.h"
#include "UI/SovCombatVitalsWidget.h"
#include "UI/SovAccessibilityPresentation.h"
#include "Components/Border.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SovAurelionWorld"

ASovAurelionJournalGate::ASovAurelionJournalGate()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("GateBody"));
    SetRootComponent(Body);
    Body->SetBoxExtent(FVector(70, 200, 200));
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateVisual"));
    Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("GateSign"));
    Sign->SetupAttachment(Body);
    Sign->SetRelativeLocation(FVector(-80, 0, 100));
    Sign->SetHorizontalAlignment(EHTA_Center);
    Sign->SetWorldSize(24);
}

void ASovAurelionJournalGate::BeginPlay() { Super::BeginPlay(); Refresh(); }
void ASovAurelionJournalGate::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); Refresh(); }
void ASovAurelionJournalGate::Refresh()
{
    const auto* PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    // Invalid or absent authority cannot open a gate. A valid incomplete journal may expose a pre-quarantine route.
    const USovCampaignDefinition* Contract = MissionId == TEXT("M12_FireAndFrost")
        ? static_cast<const USovCampaignDefinition*>(GetDefault<USovAurelionFireAndFrostMissionDefinition>())
        : MissionId == TEXT("M13_ContraryWitness") ? GetDefault<USovAurelionContraryWitnessMissionDefinition>() : nullptr;
    const bool bValid = State && State->IsStateValid() && Contract && Contract->FindBeat(BeatId);
    const bool bComplete = bValid && State->IsBeatComplete(MissionId, BeatId);
    const bool bNext = !bValid || (bBlockAfterCompletion ? bComplete : !bComplete);
    if (bApplied && bNext == bBlocking) { return; }
    bApplied = true;
    bBlocking = bNext;
    Body->SetCollisionEnabled(bUseGateBody && bBlocking ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    Visual->SetVisibility(bUseGateBody && bBlocking);
    Sign->SetText(bBlocking ? ClosedText : OpenText);
    for (AActor* Target : BoundVisualActors)
    {
        if (IsValid(Target) && Target != this && Target->GetWorld() == GetWorld())
        { Target->SetActorHiddenInGame(!bBlocking); Target->SetActorEnableCollision(bBlocking); }
    }
}

TSharedRef<SWidget> USovAurelionThreatWidget::RebuildWidget() { return SNew(SBox); }

void USovAurelionThreatWidget::QueueWarning(USovEncounterCoordinationComponent* Source, FGuid Id, AActor* Attacker, float LeadSeconds)
{
    if (!IsValid(Source) || !IsValid(Attacker) || !Id.IsValid() || !GetWorld()
        || Source->GetWorld() != GetWorld() || Attacker->GetWorld() != GetWorld()
        || !FMath::IsFinite(LeadSeconds) || LeadSeconds <= 0) { return; }
    if (Cues.ContainsByPredicate([Id](const FCue& Cue) { return Cue.Id == Id; })) { return; }
    FCue Cue; Cue.Coordinator = Source; Cue.Attacker = Attacker; Cue.Id = Id;
    Cue.ExpiresAt = GetWorld()->GetTimeSeconds() + FMath::Max(LeadSeconds + 1.f, 2.f);
    Cues.Add(Cue);
}

void USovAurelionThreatWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0;
    for (auto& Cue : Cues)
    {
        // A headless world, hidden widget, or queued-but-not-painted cue never authorizes an unseen shot.
        if (Cue.bPainted && !Cue.bAcknowledged && Cue.Coordinator.IsValid() && Cue.Attacker.IsValid() && Now < Cue.ExpiresAt)
        { Cue.bAcknowledged = Cue.Coordinator->AcknowledgeOffscreenWarning(Cue.Id); }
    }
    Cues.RemoveAll([Now](const FCue& Cue) { return !Cue.Coordinator.IsValid() || !Cue.Attacker.IsValid() || Now >= Cue.ExpiresAt; });
}

int32 USovAurelionThreatWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bParentEnabled);
    if (!GetOwningPlayer() || !GetOwningPlayer()->IsLocalController() || !GetWorld() || !bParentEnabled
        || Style.GetColorAndOpacityTint().A <= .01f || Geometry.GetLocalSize().X < 100 || Geometry.GetLocalSize().Y < 100) { return BaseLayer; }
    FVector CameraLocation; FRotator CameraRotation;
    GetOwningPlayer()->GetPlayerViewPoint(CameraLocation, CameraRotation);
    const auto* UserSettings = USovGameUserSettings::Get();
    const auto Settings = UserSettings ? UserSettings->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
    const FVector CameraForward = CameraRotation.Vector();
    const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
    TArray<const FCue*> Groups[4];
    for (const auto& Cue : Cues)
    {
        if (!Cue.Coordinator.IsValid() || !Cue.Attacker.IsValid() || GetWorld()->GetTimeSeconds() >= Cue.ExpiresAt) { continue; }
        ESovThreatCueSide Side;
        if (SovThreatCueLayout::Classify(CameraForward, CameraRight, Cue.Attacker->GetActorLocation() - CameraLocation, Side))
        { Groups[static_cast<uint8>(Side)].Add(&Cue); }
    }
    const FText Sides[] = { LOCTEXT("Ahead", "AHEAD"), LOCTEXT("Right", "RIGHT"), LOCTEXT("Behind", "BEHIND"), LOCTEXT("Left", "LEFT") };
    FLinearColor Accent = Settings.bHighContrastHUD ? FLinearColor::White : FLinearColor(1.f, .72f, .16f);
    if (!Settings.bHighContrastHUD && Settings.bOverrideThreatColor
        && FMath::IsFinite(Settings.ThreatColor.R) && FMath::IsFinite(Settings.ThreatColor.G) && FMath::IsFinite(Settings.ThreatColor.B))
    { Accent = Settings.ThreatColor.GetClamped(); Accent.A = 1.f; }
    TArray<FBox2D> OccupiedPanels;
    const auto AddPanel = [&](const UUserWidget* Owner, const UBorder* Panel)
    {
        if (!IsValid(Owner) || Owner->GetOwningPlayer() != GetOwningPlayer()
            || Owner->GetWorld() != GetWorld() || !Owner->IsInViewport() || !Owner->IsRendered()
            || !IsValid(Panel) || !Panel->IsRendered() || Panel->GetRenderOpacity() <= .01f) { return; }
        // Native UMG panels can render with empty geometry caches. Resolve their
        // arranged Slate path in that case, retaining DPI/SafeZone transforms.
        FSlateRect Bounds;
        if (!SovWidgetGeometry::FindRenderedBounds(Panel, Bounds)) { return; }
        const FVector2D Min = Geometry.AbsoluteToLocal(FVector2D(Bounds.Left, Bounds.Top));
        const FVector2D Max = Geometry.AbsoluteToLocal(FVector2D(Bounds.Right, Bounds.Bottom));
        if (!Min.ContainsNaN() && !Max.ContainsNaN() && Max.X > Min.X && Max.Y > Min.Y)
        { OccupiedPanels.Emplace(Min, Max); }
    };
    if (const auto* Frontend = GetOwningPlayer()->FindComponentByClass<USovFrontendComponent>())
    {
        if (const auto* Vitals = Frontend->GetCombatVitals())
        { AddPanel(Vitals, Vitals->GetSurvivalPanel()); AddPanel(Vitals, Vitals->GetEchoPanel()); }
        if (const auto* Presentation = Frontend->GetPresentation())
        {
            AddPanel(Presentation, Presentation->GetObjectivePanel());
            AddPanel(Presentation, Presentation->GetSubtitlePanel());
            AddPanel(Presentation, Presentation->GetCaptionPanel());
            // The holographic HUD paints its readouts rather than composing panels, so its areas come from
            // the shared layout instead of widget geometry. Without them a warning lands on the health plate.
            TArray<FSlateRect> HUDRegions;
            Presentation->GetHolographicHUDRegions(HUDRegions);
            for (const FSlateRect& Region : HUDRegions)
            {
                const FVector2D Min = Geometry.AbsoluteToLocal(FVector2D(Region.Left, Region.Top));
                const FVector2D Max = Geometry.AbsoluteToLocal(FVector2D(Region.Right, Region.Bottom));
                if (!Min.ContainsNaN() && !Max.ContainsNaN() && Max.X > Min.X && Max.Y > Min.Y) { OccupiedPanels.Emplace(Min, Max); }
            }
        }
    }
    for (uint8 Index = 0; Index < 4; ++Index)
    {
        if (Groups[Index].IsEmpty()) { continue; }
        FVector2D Position, Size; float Scale;
        if (!SovThreatCueLayout::Place(Geometry.GetLocalSize(), Settings.UIScale,
            static_cast<ESovThreatCueSide>(Index), Position, Size, Scale)) { continue; }
        // If an overcrowded viewport has no free rectangle, retain the original essential cue.
        // Never hide a warning or treat a layout result as attack authorization.
        SovThreatCueLayout::AvoidPanels(Geometry.GetLocalSize(), static_cast<ESovThreatCueSide>(Index), OccupiedPanels, Size, Position);
        const FSlateRect Bounds = Geometry.GetRenderBoundingRect(FSlateRect(FVector2f(Position), FVector2f(Position + Size)));
        if (!SovThreatCueLayout::FullyInside(FVector2D(Bounds.Left, Bounds.Top), FVector2D(Bounds.Right, Bounds.Bottom),
            FVector2D(CullingRect.Left, CullingRect.Top), FVector2D(CullingRect.Right, CullingRect.Bottom))) { continue; }
        const auto BoxGeometry = Geometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position)));
        FSlateDrawElement::MakeBox(Elements, BaseLayer + 1, BoxGeometry, FCoreStyle::Get().GetBrush("WhiteBrush"),
            ESlateDrawEffect::None, FLinearColor(.012f, .017f, .027f, Settings.bHighContrastHUD ? 1.f : .24f));
        // Static corner marks carry the holographic frame without a heavy slab
        // or animated flashing. High contrast retains its opaque backing.
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const float X = Corner & 1 ? -1.f : 1.f;
            const float Y = Corner & 2 ? -1.f : 1.f;
            const FVector2D Point = Position + FVector2D(Corner & 1 ? Size.X : 0., Corner & 2 ? Size.Y : 0.);
            TArray<FVector2f> Mark = { FVector2f(Point + FVector2D(0., Y * 10.f * Scale)),
                FVector2f(Point), FVector2f(Point + FVector2D(X * 10.f * Scale, 0.)) };
            FSlateDrawElement::MakeLines(Elements, BaseLayer + 2, Geometry.ToPaintGeometry(), Mark,
                ESlateDrawEffect::None, Accent, true, Scale);
        }
        // A static directional chevron and explicit words remain legible without flashes or color discrimination.
        const FVector2D Center = Position + FVector2D(23.f, 33.f) * Scale;
        const FVector2D Direction = Index == 0 ? FVector2D(0, -1) : Index == 1 ? FVector2D(1, 0)
            : Index == 2 ? FVector2D(0, 1) : FVector2D(-1, 0);
        const FVector2D Normal(-Direction.Y, Direction.X);
        TArray<FVector2f> Chevron;
        Chevron.Add(FVector2f(Center - Direction * 8.f * Scale + Normal * 8.f * Scale));
        Chevron.Add(FVector2f(Center + Direction * 8.f * Scale));
        Chevron.Add(FVector2f(Center - Direction * 8.f * Scale - Normal * 8.f * Scale));
        FSlateDrawElement::MakeLines(Elements, BaseLayer + 2, Geometry.ToPaintGeometry(), Chevron, ESlateDrawEffect::None, Accent, true, 2.f * Scale);
        const FText Label = FText::Format(LOCTEXT("IncomingGrouped", "INCOMING FIRE  {0}"), FText::AsNumber(Groups[Index].Num()));
        const auto DrawLabel = [&](const FText& Text, const float Y, const int32 FontSize, const FLinearColor Color)
        {
            FSlateDrawElement::MakeText(Elements, BaseLayer + 2,
                Geometry.ToPaintGeometry(FVector2f(Size.X - 48.f * Scale, 27.f * Scale),
                    FSlateLayoutTransform(FVector2f(Position + FVector2D(47.f, Y + 1.f) * Scale))),
                Text, FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::RoundToInt(FontSize * Scale)),
                ESlateDrawEffect::None, FLinearColor::Black);
            FSlateDrawElement::MakeText(Elements, BaseLayer + 3,
                Geometry.ToPaintGeometry(FVector2f(Size.X - 48.f * Scale, 27.f * Scale),
                    FSlateLayoutTransform(FVector2f(Position + FVector2D(46.f, Y) * Scale))),
                Text, FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FMath::RoundToInt(FontSize * Scale)), ESlateDrawEffect::None, Color);
        };
        DrawLabel(Label, 10.f, 17, FLinearColor::White);
        DrawLabel(Sides[Index], 35.f, 14, Accent);
        // Every source in this sector is represented by the displayed count; clipped groups authorize nothing.
        for (const FCue* Cue : Groups[Index]) { Cue->bPainted = true; }
        OccupiedPanels.Emplace(Position, Position + Size);
    }
    return BaseLayer + 3;
}

ASovAurelionPresentationDirector::ASovAurelionPresentationDirector()
{ PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .1f; }
void ASovAurelionPresentationDirector::BeginPlay()
{
    Super::BeginPlay();
    for (ASovEncounterDirector* Encounter : Encounters)
    {
        if (IsValid(Encounter) && Encounter->GetWorld() == GetWorld() && Encounter->GetCoordinationComponent())
        { Encounter->GetCoordinationComponent()->OnOffscreenAttackWarning.AddUniqueDynamic(this, &ThisClass::HandleWarning); }
    }
}
void ASovAurelionPresentationDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (Widget && (!PC || Widget->GetOwningPlayer() != PC)) { Widget->RemoveFromParent(); Widget = nullptr; }
    if (!Widget && PC && PC->IsLocalController() && PC->GetLocalPlayer() && !IsRunningDedicatedServer())
    {
        Widget = CreateWidget<USovAurelionThreatWidget>(PC);
        if (Widget) { Widget->SetVisibility(ESlateVisibility::HitTestInvisible); Widget->AddToPlayerScreen(60); }
    }
}
void ASovAurelionPresentationDirector::HandleWarning(FGuid Id, AActor* Source, float LeadSeconds)
{
    if (!Widget || !IsValid(Source)) { return; }
    ASovEncounterDirector* WarningEncounter = nullptr;
    for (ASovEncounterDirector* Encounter : Encounters)
    {
        if (IsValid(Encounter) && Encounter->GetEncounterState() == ESovEncounterState::Active && !Encounter->FindParticipantId(Source).IsNone())
        { if (WarningEncounter) { return; } WarningEncounter = Encounter; }
    }
    if (WarningEncounter) { Widget->QueueWarning(WarningEncounter->GetCoordinationComponent(), Id, Source, LeadSeconds); }
}
void ASovAurelionPresentationDirector::EndPlay(EEndPlayReason::Type Reason)
{
    for (ASovEncounterDirector* Encounter : Encounters)
    {
        if (IsValid(Encounter) && Encounter->GetCoordinationComponent())
        { Encounter->GetCoordinationComponent()->OnOffscreenAttackWarning.RemoveDynamic(this, &ThisClass::HandleWarning); }
    }
    if (Widget) { Widget->RemoveFromParent(); Widget = nullptr; }
    Super::EndPlay(Reason);
}
#undef LOCTEXT_NAMESPACE
