// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovWorldLabelLayout.h"
#include "UI/SovCombatVitalsWidget.h"
#include "UI/SovHUDStyle.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerController.h"
#include "Components/SceneComponent.h"
#include "UI/SovAccessibilityPolicy.h"
#include "UI/SovPlayerInformationPolicy.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/SovHolographicHUDLayout.h"
#include "Components/SovWeakPointComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Internationalization/BreakIterator.h"
#include "Internationalization/IBreakIterator.h"
#include "Misc/Crc.h"
#include "Navigation/MapMarker.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include "Navigation/NavigatorGameplayTags.h"
#include "Rendering/DrawElementTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Layout/Clipping.h"
#include "Styling/CoreStyle.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"

namespace
{
bool IsCinematicControlled(const APlayerController* PC)
{
    const auto* Player = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    const auto* ASC = Player ? Player->GetNarrativeAbilitySystemComponent() : nullptr;
    return ASC && ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
}
}

#define LOCTEXT_NAMESPACE "SovAccessibilityPresentation"
USovAccessibilityPresentation::USovAccessibilityPresentation(const FObjectInitializer& Initializer) : Super(Initializer)
{ SetVisibility(ESlateVisibility::HitTestInvisible); }
TArray<FString> USovAccessibilityPresentation::PaginateText(const FString& Text, int32 CharactersPerLine, int32 MaximumLines)
{
	TArray<FString> Pages;
	if (Text.IsEmpty()) { return Pages; }
	CharactersPerLine = FMath::Clamp(CharactersPerLine, 1, 64); MaximumLines = FMath::Clamp(MaximumLines, 1, 4);
	const FString Normalized = Text.Replace(TEXT("\r"), TEXT(""));
	// Prefer Unicode soft-wrap opportunities so ordinary words survive intact.
	// Oversized tokens still fall back to grapheme boundaries, never UTF-16 units.
	auto Characters = FBreakIterator::CreateCharacterBoundaryIterator(); Characters->SetString(Normalized);
	TArray<int32> Offsets; Offsets.Add(Characters->ResetToBeginning());
	for (int32 End = Characters->MoveToNext(); End != INDEX_NONE; End = Characters->MoveToNext()) { Offsets.Add(End); }
	auto Breaks = FBreakIterator::CreateLineBreakIterator(); Breaks->SetString(Normalized);
	Breaks->ResetToBeginning();
	TSet<int32> SoftBreaks;
	for (int32 End = Breaks->MoveToNext(); End != INDEX_NONE; End = Breaks->MoveToNext()) { SoftBreaks.Add(End); }
	FString Page; int32 Lines = 0, Start = 0;
	const int32 Count = Offsets.Num() - 1;
	while (Start < Count)
	{
		int32 End = Start;
		while (End < Count && End - Start < CharactersPerLine && Normalized[Offsets[End]] != TEXT('\n')) { ++End; }
		const bool bHardBreak = End < Count && Normalized[Offsets[End]] == TEXT('\n');
		if (!bHardBreak && End < Count)
		{
			for (int32 Candidate = End; Candidate > Start; --Candidate)
			{
				if (SoftBreaks.Contains(Offsets[Candidate])) { End = Candidate; break; }
			}
		}
		if (Lines > 0) { Page += TEXT("\n"); }
		Page += Normalized.Mid(Offsets[Start], Offsets[End] - Offsets[Start]);
		Start = End + (bHardBreak ? 1 : 0);
		if (++Lines == MaximumLines) { Pages.Add(MoveTemp(Page)); Page.Reset(); Lines = 0; }
	}
	if (Lines > 0) { Pages.Add(MoveTemp(Page)); }
	return Pages;
}
FLinearColor USovAccessibilityPresentation::TeamTint(const FSovUserSettingsSnapshot& Value)
{
	if (Value.bOverrideTeamColor) { return Value.TeamColor; }
	switch (Value.ColorVisionPreset)
	{ case ESovColorVisionPreset::Deuteranopia: return FLinearColor(.1f,.55f,1); case ESovColorVisionPreset::Protanopia: return FLinearColor(.05f,.7f,1); case ESovColorVisionPreset::Tritanopia: return FLinearColor(.1f,1,.6f); default: return FLinearColor(.1f,.65f,1); }
}
FLinearColor USovAccessibilityPresentation::ThreatTint(const FSovUserSettingsSnapshot& Value)
{
	if (Value.bOverrideThreatColor) { return Value.ThreatColor; }
	switch (Value.ColorVisionPreset)
	{ case ESovColorVisionPreset::Deuteranopia: return FLinearColor(1,.8f,.05f); case ESovColorVisionPreset::Protanopia: return FLinearColor(1,.75f,.1f); case ESovColorVisionPreset::Tritanopia: return FLinearColor(1,.2f,.4f); default: return FLinearColor(1,.3f,.1f); }
}
TSharedRef<SWidget> USovAccessibilityPresentation::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this,TEXT("WidgetTree")); }
	if (!SubtitleText)
	{
		USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>();
		SafeTextCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(); Safe->AddChild(SafeTextCanvas); WidgetTree->RootWidget = Safe;
		SubtitleBackground = WidgetTree->ConstructWidget<UBorder>(); SubtitleBackground->SetPadding(FMargin(18,10));
		// RefreshText supplies the safe-area wrap width. Auto-wrap also clamps to
		// the last painted width, trapping auto-sized dialogue panels at short lines.
		SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(); SubtitleText->SetJustification(ETextJustify::Center); SubtitleText->SetAutoWrapText(false);
		SubtitleBackground->AddChild(SubtitleText); SubtitleSlot = SafeTextCanvas->AddChildToCanvas(SubtitleBackground);
		SubtitleSlot->SetAnchors(FAnchors(.5f,.88f)); SubtitleSlot->SetAlignment(FVector2D(.5f,1)); SubtitleSlot->SetAutoSize(true);
		CaptionBackground = WidgetTree->ConstructWidget<UBorder>(); CaptionBackground->SetPadding(FMargin(14,8));
		CaptionText = WidgetTree->ConstructWidget<UTextBlock>(); CaptionText->SetJustification(ETextJustify::Center); CaptionText->SetAutoWrapText(false);
		CaptionBackground->AddChild(CaptionText); UCanvasPanelSlot* CanvasSlot = SafeTextCanvas->AddChildToCanvas(CaptionBackground);
		CanvasSlot->SetAnchors(FAnchors(.5f,.13f)); CanvasSlot->SetAlignment(FVector2D(.5f,0)); CanvasSlot->SetAutoSize(true);
		ObjectiveBackground = WidgetTree->ConstructWidget<UBorder>(); ObjectiveBackground->SetPadding(FMargin(12, 8));
		ObjectiveBackground->SetClipping(EWidgetClipping::ClipToBounds);
		ObjectiveSize = WidgetTree->ConstructWidget<USizeBox>(); ObjectiveBackground->AddChild(ObjectiveSize);
		auto* Rows = WidgetTree->ConstructWidget<UVerticalBox>(); ObjectiveSize->AddChild(Rows);
		for (int32 Index = 0; Index < MaximumObjectiveRows; ++Index)
		{
			auto* Row = WidgetTree->ConstructWidget<UTextBlock>(); Row->SetAutoWrapText(false); Row->SetJustification(ETextJustify::Left);
			Row->SetShadowOffset(FVector2D(1.f)); Row->SetShadowColorAndOpacity(FLinearColor::Black);
			Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 0, 0, 8)); ObjectiveRows.Add(Row);
		}
		ObjectiveText = ObjectiveRows[0];
		ObjectiveOverflow = WidgetTree->ConstructWidget<UTextBlock>(); ObjectiveOverflow->SetAutoWrapText(false); Rows->AddChild(ObjectiveOverflow);
		UCanvasPanelSlot* ObjectiveSlot = SafeTextCanvas->AddChildToCanvas(ObjectiveBackground);
		ObjectiveSlot->SetAnchors(FAnchors(0.f, 0.f)); ObjectiveSlot->SetAlignment(FVector2D::ZeroVector);
		ObjectiveSlot->SetPosition(FVector2D(12.f, 12.f)); ObjectiveSlot->SetAutoSize(true);
	}
	RefreshText(); return Super::RebuildWidget();
}
FVector2D USovAccessibilityPresentation::GetSafeCanvasSize() const
{
	const FVector2D Size = SafeTextCanvas ? FVector2D(SafeTextCanvas->GetCachedGeometry().GetLocalSize()) : FVector2D::ZeroVector;
	return Size.X > 0. && Size.Y > 0. ? Size : FVector2D(GetCachedGeometry().GetLocalSize());
}
bool USovAccessibilityPresentation::GetSafeAreaAbsoluteRect(FSlateRect& Out) const
{
	if (!SafeTextCanvas) { return false; }
	const FGeometry& Geometry = SafeTextCanvas->GetCachedGeometry();
	if (Geometry.GetLocalSize().X <= 1. || Geometry.GetLocalSize().Y <= 1.) { return false; }
	Out = Geometry.GetLayoutBoundingRect();
	return true;
}
void USovAccessibilityPresentation::SetHolographicHUDClearance(bool bHUDShown, bool bAmmoShown)
{
	if (bHUDClearance == bHUDShown && bHUDAmmo == bAmmoShown) { return; }
	bHUDClearance = bHUDShown; bHUDAmmo = bAmmoShown;
	if (!ActiveSpeech.Text.IsEmpty() && SpeechPages.IsValidIndex(PageIndex)) { BeginEntry(ActiveSpeech); }
	RefreshText();
}
void USovAccessibilityPresentation::GetHolographicHUDRegions(TArray<FSlateRect>& OutAbsolute) const
{
	OutAbsolute.Reset();
	if (!bHUDClearance || !SafeTextCanvas) { return; }
	const FGeometry& Geometry = SafeTextCanvas->GetCachedGeometry();
	if (Geometry.GetLocalSize().X <= 1. || Geometry.GetLocalSize().Y <= 1.) { return; }
	const auto Layout = SovHolographicHUDLayout::Compute(FVector2D(Geometry.GetLocalSize()), Settings.UIScale);
	for (const FBox2D& Region : Layout.Regions(bHUDAmmo))
	{
		const FVector2D Min = Geometry.LocalToAbsolute(Region.Min), Max = Geometry.LocalToAbsolute(Region.Max);
		OutAbsolute.Emplace(float(Min.X), float(Min.Y), float(Max.X), float(Max.Y));
	}
}
float USovAccessibilityPresentation::GetSubtitleTextWidth() const
{
	const float Width = GetSafeTextWidth() * .84f;
	if (!bHUDClearance) { return Width; }
	const FVector2D Size = GetSafeCanvasSize();
	if (Size.X <= 1. || Size.Y <= 1.) { return Width; }
	const auto Layout = SovHolographicHUDLayout::Compute(Size, Settings.UIScale);
	const float HeightBudget = SovHolographicHUDLayout::TextHeightBudget(Settings.SubtitleMaximumLines, Settings.SubtitleScale);
	return FMath::Min(Width, SovHolographicHUDLayout::PlaceSubtitle(Layout, float(Size.Y) * .9f, Width, HeightBudget).MaximumWidth);
}
float USovAccessibilityPresentation::GetSafeTextWidth() const
{
	// Text uses the console safe area. NativePaint retains the full player viewport for world projections.
	const float Width = SafeTextCanvas ? float(SafeTextCanvas->GetCachedGeometry().GetLocalSize().X) : 0.f;
	return Width > 0.f ? Width : float(GetCachedGeometry().GetLocalSize().X);
}
void USovAccessibilityPresentation::NativeConstruct()
{
	Super::NativeConstruct(); BoundSettings = USovGameUserSettings::Get();
	if (BoundSettings) { Settings = BoundSettings->GetSettingsSnapshot(); BoundSettings->OnUserSettingsChanged.AddUniqueDynamic(this,&ThisClass::SettingsChanged); }
	if (GetOwningPlayer())
	{
		Interaction = GetOwningPlayer()->FindComponentByClass<UPlayerInteractionComponent>();
		if (Interaction) { Interaction->OnFoundInteractable.AddUniqueDynamic(this,&ThisClass::FoundInteractable); Interaction->OnLostInteractable.AddUniqueDynamic(this,&ThisClass::LostInteractable); }
	}
	RefreshText();
}
void USovAccessibilityPresentation::NativeDestruct()
{
	if (BoundSettings) { BoundSettings->OnUserSettingsChanged.RemoveDynamic(this,&ThisClass::SettingsChanged); }
	if (Interaction) { Interaction->OnFoundInteractable.RemoveDynamic(this,&ThisClass::FoundInteractable); Interaction->OnLostInteractable.RemoveDynamic(this,&ThisClass::LostInteractable); }
	FocusedInteractable.Reset(); Interaction = nullptr; BoundSettings = nullptr; ResetMarkerRegistry(); ResetWaypointRegistry(); ++ObjectiveViewGeneration; ClearObjectives(); ClearSceneHistory(); Super::NativeDestruct();
}
void USovAccessibilityPresentation::FoundInteractable(UNarrativeInteractableComponent* Value) { FocusedInteractable = Value; }
void USovAccessibilityPresentation::LostInteractable(UNarrativeInteractableComponent* Value) { if (FocusedInteractable == Value) { FocusedInteractable.Reset(); } }
void USovAccessibilityPresentation::SettingsChanged(const FSovUserSettingsSnapshot& Value)
{
	Settings = Value;
	if (!ActiveSpeech.Text.IsEmpty()) { BeginEntry(ActiveSpeech); }
	RefreshText();
}
void USovAccessibilityPresentation::PresentSpeech(const FText& Speaker, const FText& Text, float Duration, const FVector& Location, bool bCinematic)
{
	if (Text.IsEmpty() || !FMath::IsFinite(Duration) || Location.ContainsNaN()) { return; }
	FSovSceneSubtitleEntry Entry; Entry.Speaker = Speaker; Entry.Text = Text; Entry.Location = Location; Entry.Duration = Duration < 0.f ? -1.f : FMath::Clamp(Duration,2.f,120.f); Entry.bCinematic = bCinematic;
	History.Add(Entry); if (History.Num() > 64) { History.RemoveAt(0); }
	if (!Settings.bSubtitles) { OnSceneHistoryChanged.Broadcast(); return; }
	if (!SpeechPages.IsEmpty()) { if (PendingSpeech.Num() >= 16) { PendingSpeech.RemoveAt(0); } PendingSpeech.Add(Entry); }
	else { BeginEntry(Entry); }
	OnSceneHistoryChanged.Broadcast();
}
void USovAccessibilityPresentation::BeginEntry(const FSovSceneSubtitleEntry& Entry)
{
	ActiveSpeech = Entry;
	int32 Characters = Settings.SubtitleCharactersPerLine;
	const float Width = GetSubtitleTextWidth();
	if (Width > 0) { Characters = FMath::Min(Characters,FMath::Max(1,FMath::FloorToInt(Width / (27.f * Settings.SubtitleScale)))); }
	SpeechPages = PaginateText(Entry.Text.ToString(), Characters, Settings.SubtitleMaximumLines); PageIndex = 0;
	PageRemaining = FMath::Max(2.f, Entry.Duration / FMath::Max(1,SpeechPages.Num())); RefreshText();
}
void USovAccessibilityPresentation::PresentCaption(const FText& Text, float Duration, const FVector& Location, ESovCaptionPriority CaptionPriority)
{
	if (Text.IsEmpty() || !FMath::IsFinite(Duration) || Location.ContainsNaN() || CaptionPriority > ESovCaptionPriority::Critical) { return; }
	// Repeated damage updates direction, never restarts the reading clock or floods history.
	if (CaptionRemaining > 0.f && ActiveCaption.Text.EqualTo(Text))
	{ ActiveCaption.Location = Location; ActiveCaption.CaptionPriority = FMath::Max(ActiveCaption.CaptionPriority, CaptionPriority); return; }
	for (auto& Pending : PendingCaptions)
	{
		if (Pending.Text.EqualTo(Text))
		{ Pending.Location = Location; Pending.CaptionPriority = FMath::Max(Pending.CaptionPriority, CaptionPriority); return; }
	}
	FSovSceneSubtitleEntry Entry; Entry.Text = Text; Entry.Location = Location; Entry.bCaption = true;
	Entry.Duration = FMath::Clamp(Duration, 3.f, 30.f); Entry.CaptionPriority = CaptionPriority;
	if (SovPlayerInformationPolicy::CanPreempt(int(ActiveCaption.CaptionPriority), int(CaptionPriority), CaptionRemaining > 0.f))
	{
		if (CaptionRemaining > 0.f) { QueueCaption(ActiveCaption); }
		BeginCaption(Entry);
	}
	else { QueueCaption(Entry); }
	History.Add(Entry); if (History.Num() > 64) { History.RemoveAt(0); }
	RefreshText(); OnSceneHistoryChanged.Broadcast();
}
void USovAccessibilityPresentation::QueueCaption(const FSovSceneSubtitleEntry& Entry)
{
	if (PendingCaptions.Num() >= 8)
	{
		int32 Lowest = 0;
		for (int32 Index = 1; Index < PendingCaptions.Num(); ++Index)
		{ if (PendingCaptions[Index].CaptionPriority < PendingCaptions[Lowest].CaptionPriority) { Lowest = Index; } }
		// A routine event cannot evict a critical warning, even under a sustained burst.
		if (Entry.CaptionPriority <= PendingCaptions[Lowest].CaptionPriority) { return; }
		PendingCaptions.RemoveAt(Lowest);
	}
	PendingCaptions.Add(Entry);
}
void USovAccessibilityPresentation::BeginCaption(const FSovSceneSubtitleEntry& Entry)
{
	ActiveCaption = Entry;
	const float Width=GetSafeTextWidth();
	const int32 Characters=Width>0 ? FMath::Min(Settings.SubtitleCharactersPerLine,FMath::Max(1,FMath::FloorToInt(Width*.8f/(27.f*Settings.SubtitleScale)))) : Settings.SubtitleCharactersPerLine;
	CaptionPages=PaginateText(Entry.Text.ToString(),Characters,Settings.SubtitleMaximumLines); CaptionPageIndex=0;
	CaptionPageDuration=FMath::Max(3.f,Entry.Duration/FMath::Max(1,CaptionPages.Num()));
	CaptionRemaining=CaptionPageDuration;
}
void USovAccessibilityPresentation::ClearSpeech()
{
	// The latest produced line owns the finish notification even when earlier pages are still readable.
	if (!PendingSpeech.IsEmpty()) { PendingSpeech.Last().bFinished = true; }
	else { ActiveSpeech.bFinished = true; }
}
void USovAccessibilityPresentation::ClearSceneHistory()
{ History.Reset(); PendingSpeech.Reset(); PendingCaptions.Reset(); SpeechPages.Reset(); CaptionPages.Reset(); Markers.Reset(); ActiveSpeech = FSovSceneSubtitleEntry(); ActiveCaption = FSovSceneSubtitleEntry(); CaptionRemaining = 0; RefreshText(); OnSceneHistoryChanged.Broadcast(); }
void USovAccessibilityPresentation::RetireSpeechPresentation()
{ PendingSpeech.Reset(); SpeechPages.Reset(); ActiveSpeech = FSovSceneSubtitleEntry(); PageRemaining = 0.f; RefreshText(); }
void USovAccessibilityPresentation::PresentObjectives(const TArray<FSovObjectivePresentationEntry>& Entries, int32 AdditionalCount)
{
	const auto Previous = ObjectiveReviewEntries;
    ObjectiveWaypoint = {}; MarkerRefreshRemaining = 0.f;
	Objectives.Reset(); ObjectiveReviewEntries.Reset(); VisibleObjectiveRows = 0; AdditionalObjectiveCount = FMath::Max(0, AdditionalCount);
	TSet<FName> Seen;
	for (const FSovObjectivePresentationEntry& Entry : Entries)
	{
		if (Entry.BeatId.IsNone() || Entry.Text.IsEmpty() || Seen.Contains(Entry.BeatId)
			|| (Entry.State != ESovObjectiveState::Available && Entry.State != ESovObjectiveState::Active)) { continue; }
		Seen.Add(Entry.BeatId);
		ObjectiveReviewEntries.Add(Entry);
		if (Objectives.Num() < MaximumObjectiveRows) { Objectives.Add(Entry); }
		else { ++AdditionalObjectiveCount; }
		if (ObjectiveReviewEntries.Num() >= 512) { break; }
	}
	bool bChanged = Previous.Num() != ObjectiveReviewEntries.Num();
	for (int32 Index = 0; !bChanged && Index < Previous.Num(); ++Index)
	{
		const auto& A = Previous[Index]; const auto& B = ObjectiveReviewEntries[Index];
		bChanged = A.BeatId != B.BeatId || A.State != B.State || A.bOptional != B.bOptional || A.bCanonGate != B.bCanonGate
			|| !A.Text.EqualTo(B.Text) || !A.FailureRule.EqualTo(B.FailureRule);
	}
	RefreshObjectiveText();
	if (bChanged) { OnObjectiveViewChanged.Broadcast(); }
}
void USovAccessibilityPresentation::ClearObjectives()
{
	const bool bChanged = !ObjectiveReviewEntries.IsEmpty();
    ObjectiveWaypoint = {};
	Objectives.Reset(); ObjectiveReviewEntries.Reset(); AdditionalObjectiveCount = 0; VisibleObjectiveRows = 0; RefreshObjectiveText();
	if (bChanged) { OnObjectiveViewChanged.Broadcast(); }
}
void USovAccessibilityPresentation::RefreshObjectiveText()
{
	if (!ObjectiveText || !ObjectiveBackground || !ObjectiveSize) { return; }
	if (Objectives.IsEmpty())
	{
		for (const auto& Row : ObjectiveRows) { Row->SetText(FText::GetEmpty()); Row->SetVisibility(ESlateVisibility::Collapsed); }
		ObjectiveOverflow->SetText(FText::GetEmpty()); ObjectiveBackground->SetVisibility(ESlateVisibility::Collapsed); return;
	}
	for (int32 Index = 0; Index < ObjectiveRows.Num(); ++Index)
	{
		auto* Label = ObjectiveRows[Index].Get();
		Label->SetVisibility(Objectives.IsValidIndex(Index) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Label->SetText(FText::GetEmpty());
		if (!Objectives.IsValidIndex(Index)) { continue; }
		const FSovObjectivePresentationEntry& Entry = Objectives[Index];
		const FText Kind = Entry.bOptional ? LOCTEXT("OptionalObjective", "Optional") : LOCTEXT("MainObjective", "Main objective");
		const FText State = Entry.State == ESovObjectiveState::Active ? LOCTEXT("ActiveObjective", "Active") : LOCTEXT("AvailableObjective", "Available");
		FText Row = FText::Format(LOCTEXT("ObjectiveRow", "{0} · {1}\n{2}"), Kind, State, Entry.Text);
		if (!Entry.FailureRule.IsEmpty()) { Row = FText::Format(LOCTEXT("ObjectiveRule", "{0}\n{1}"), Row, Entry.FailureRule); }
		Label->SetText(Row);
	}
    const auto* Player = GetOwningPlayer() ? Cast<ASovPlayerCharacterBase>(GetOwningPlayer()->GetPawn()) : nullptr;
    const auto Theme = SovHUDStyle::ForProtagonist(Player ? Player->GetProtagonistIdentityTag() : FGameplayTag(), Settings.bHighContrastHUD);
    FLinearColor ObjectiveFill = Theme.Background;
    ObjectiveFill.A = Settings.bHighContrastHUD ? 1.f : .20f;
    ObjectiveBackground->SetBrushColor(ObjectiveFill);
	const FVector2D Size = SafeTextCanvas ? SafeTextCanvas->GetCachedGeometry().GetLocalSize() : GetCachedGeometry().GetLocalSize();
	LayoutObjectives(Size.X > 0.f ? float(Size.X) : 1280.f, Size.Y > 0.f ? float(Size.Y) : 720.f);
}
void USovAccessibilityPresentation::LayoutObjectives(float SafeWidth, float SafeHeight)
{
	if (!ObjectiveBackground || !ObjectiveSize || !ObjectiveOverflow) { return; }
	constexpr float CornerInset = 12.f, RowGap = 8.f, PriorityGap = 12.f;
	const FMargin ObjectivePadding = ObjectiveBackground->GetPadding();
	float Width = FMath::Max(1.f, FMath::Min(440.f * Settings.UIScale, SafeWidth * .38f) - 24.f);
	TArray<FBox2D> PriorityPanels;
	if (bHUDClearance)
	{
		// The top-left panel stops short of the identity plate and sits above the radar.
		const auto Layout = SovHolographicHUDLayout::Compute(FVector2D(SafeWidth, SafeHeight), Settings.UIScale);
		Width = FMath::Max(1.f, FMath::Min(Width, SovHolographicHUDLayout::TopLeftPanelMaximumWidth(Layout, CornerInset) - 24.f));
		PriorityPanels.Add(Layout.Radar); PriorityPanels.Add(Layout.Arc);
	}
	const auto Font = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(20.f * Settings.UIScale));
	ObjectiveOverflow->SetFont(Font); ObjectiveOverflow->SetWrapTextAt(Width);
	ObjectiveOverflow->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	// Explicit wrapping uses this safe width immediately, not the previous frame's
	// arranged width. Measure rows even when their parent was collapsed last frame.
	for (int32 Index = 0; Index < ObjectiveRows.Num(); ++Index)
	{
		auto* Label = ObjectiveRows[Index].Get(); Label->SetFont(Font); Label->SetWrapTextAt(Width);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Label->SetVisibility(Objectives.IsValidIndex(Index) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (Objectives.IsValidIndex(Index)) { Label->ForceLayoutPrepass(); }
	}
	const auto MeasurePriorityPanel = [&](UBorder* Panel, UTextBlock* Text, const float WrapWidth)
	{
		if (!Panel || !Text) { return; }
		Text->SetWrapTextAt(FMath::Max(1.f, WrapWidth)); Panel->ForceLayoutPrepass();
		const auto* PrioritySlot = Cast<UCanvasPanelSlot>(Panel->Slot);
		if (!PrioritySlot) { return; }
		const FVector2D PanelSize = Panel->GetDesiredSize();
		const FVector2D Anchor = PrioritySlot->GetAnchors().Minimum * FVector2D(SafeWidth, SafeHeight);
		const FVector2D Min = Anchor + PrioritySlot->GetPosition() - PrioritySlot->GetAlignment() * PanelSize;
		PriorityPanels.Emplace(Min, Min + PanelSize);
	};
	if (Settings.bClosedCaptions && CaptionRemaining > 0.f)
	{ MeasurePriorityPanel(CaptionBackground, CaptionText, SafeWidth * .8f); }
	if (Settings.bSubtitles && SpeechPages.IsValidIndex(PageIndex))
	{ MeasurePriorityPanel(SubtitleBackground, SubtitleText, SafeWidth * .84f); }
	const auto AvailableHeight = [&](const float ContentWidth)
	{
		float Bottom = FMath::Min(SafeHeight * .65f, SafeHeight - CornerInset);
		const float Right = CornerInset + ObjectivePadding.Left + ContentWidth + ObjectivePadding.Right;
		for (const FBox2D& Panel : PriorityPanels)
		{
			if (Panel.Max.Y > CornerInset && CornerInset < Panel.Max.X + PriorityGap && Right > Panel.Min.X - PriorityGap)
			{ Bottom = FMath::Min(Bottom, float(Panel.Min.Y) - PriorityGap); }
		}
		return FMath::Max(0.f, Bottom - CornerInset - ObjectivePadding.Top - ObjectivePadding.Bottom);
	};
	// Captions never move the corner. Try at most four complete row counts against
	// the actual competing rectangles, including the exact remaining-count label.
	VisibleObjectiveRows = 0; float ContentHeight = 0.f, Budget = 0.f; bool bFitsContent = false;
	for (int32 Count = Objectives.Num(); Count >= 0; --Count)
	{
		const int32 Remaining = AdditionalObjectiveCount + Objectives.Num() - Count;
		ObjectiveOverflow->SetText(FText::Format(
			LOCTEXT("AdditionalObjectives", "{0} more {0}|plural(one=objective,other=objectives) in Accessibility > Review current objectives"),
			Remaining));
		ObjectiveOverflow->SetVisibility(Remaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		float Height = 0.f, ContentWidth = 0.f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D RowSize = ObjectiveRows[Index]->GetDesiredSize();
			Height += float(RowSize.Y); ContentWidth = FMath::Max(ContentWidth, float(RowSize.X));
			if (Index + 1 < Count || Remaining > 0) { Height += RowGap; }
		}
		if (Remaining > 0)
		{
			ObjectiveOverflow->ForceLayoutPrepass();
			const FVector2D CounterSize = ObjectiveOverflow->GetDesiredSize();
			Height += float(CounterSize.Y); ContentWidth = FMath::Max(ContentWidth, float(CounterSize.X));
		}
		Budget = AvailableHeight(ContentWidth);
		if (Height > 0.f && Height <= Budget)
		{ VisibleObjectiveRows = Count; ContentHeight = Height; bFitsContent = true; break; }
	}
	const int32 Remaining = GetAdditionalObjectiveCount();
	for (int32 Index = 0; Index < ObjectiveRows.Num(); ++Index)
	{
		ObjectiveRows[Index]->SetVisibility(Index < VisibleObjectiveRows ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (auto* RowSlot = Cast<UVerticalBoxSlot>(ObjectiveRows[Index]->Slot))
		{ RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, Index + 1 < VisibleObjectiveRows || (Index < VisibleObjectiveRows && Remaining > 0) ? RowGap : 0.f)); }
	}
	ObjectiveOverflow->SetVisibility(bFitsContent && Remaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ObjectiveSize->SetMaxDesiredHeight(Budget);
	ObjectiveSize->SetHeightOverride(ContentHeight);
	ObjectiveBackground->SetVisibility(Settings.bShowObjectiveText && !IsCinematicControlled(GetOwningPlayer()) && !Objectives.IsEmpty() && Budget > 0.f && bFitsContent
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* CanvasSlot = Cast<UCanvasPanelSlot>(ObjectiveBackground->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f)); CanvasSlot->SetAlignment(FVector2D::ZeroVector);
		CanvasSlot->SetPosition(FVector2D(CornerInset, CornerInset));
	}
	// The temporary overflow measurement must not survive as empty panel space.
	// Recompute the final desired width/height after every visibility and padding decision.
	ObjectiveBackground->ForceLayoutPrepass();
}
FText USovAccessibilityPresentation::DirectionText(const FVector& Location) const
{
	const APlayerController* PC = GetOwningPlayer(); if (!PC || !Settings.bSubtitleDirections) { return FText::GetEmpty(); }
	FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View,Rotation);
	const FVector Delta = Rotation.UnrotateVector(Location - View);
	if (Delta.SizeSquared() < 1.) { return FText::GetEmpty(); }
	if (FMath::Abs(Delta.Y) > FMath::Abs(Delta.X)) { return Delta.Y > 0 ? LOCTEXT("Right","[right]") : LOCTEXT("Left","[left]"); }
	return Delta.X >= 0 ? LOCTEXT("Ahead","[ahead]") : LOCTEXT("Behind","[behind]");
}
void USovAccessibilityPresentation::RefreshText()
{
	if (!SubtitleText || !CaptionText) { RefreshObjectiveText(); return; }
	const int32 Size = FMath::RoundToInt(26 * Settings.SubtitleScale);
	SubtitleText->SetWrapTextAt(FMath::Max(1.f,GetSubtitleTextWidth()));
	SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular",Size)); CaptionText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold",Size));
	SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White)); CaptionText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	const FLinearColor Background(0,0,0,Settings.bHighContrastHUD ? 1.f : Settings.SubtitleBackgroundOpacity);
	SubtitleBackground->SetBrushColor(Background); CaptionBackground->SetBrushColor(Background);
	SubtitleBackground->SetVisibility(Settings.bSubtitles && SpeechPages.IsValidIndex(PageIndex) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CaptionBackground->SetVisibility(Settings.bClosedCaptions && CaptionRemaining > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (SpeechPages.IsValidIndex(PageIndex))
	{
		// Speaker names are a non-color identity cue; page text never relies on the palette alone.
		const TCHAR Patterns[]={TCHAR(0x25CF),TCHAR(0x25C6),TCHAR(0x25A0),TCHAR(0x25B2)};
		const FText Pattern=ActiveSpeech.Speaker.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Chr(Patterns[FCrc::StrCrc32(*ActiveSpeech.Speaker.ToString())%4]));
		const FText Speaker = FText::Format(LOCTEXT("SpeakerPattern","{0} {1}"),Pattern,Settings.bSubtitleSpeakerNames ? ActiveSpeech.Speaker : FText::GetEmpty());
		SubtitleText->SetText(FText::Format(LOCTEXT("SpeechLayout","{0} {1}\n{2}"),Speaker,DirectionText(ActiveSpeech.Location),FText::FromString(SpeechPages[PageIndex])));
		float Anchor = ActiveSpeech.bCinematic ? .9f : .8f;
		const FVector2D Safe = GetSafeCanvasSize();
		if (bHUDClearance && Safe.X > 1. && Safe.Y > 1.)
		{
			// Above the Echo arc, and clear of the radar: text is never drawn through a HUD readout.
			const auto Layout = SovHolographicHUDLayout::Compute(Safe, Settings.UIScale);
			const float HeightBudget = SovHolographicHUDLayout::TextHeightBudget(Settings.SubtitleMaximumLines, Settings.SubtitleScale);
			const auto Placement = SovHolographicHUDLayout::PlaceSubtitle(Layout, float(Safe.Y) * Anchor, GetSafeTextWidth() * .84f, HeightBudget);
			Anchor = FMath::Clamp(Placement.Bottom / float(Safe.Y), .2f, 1.f);
		}
		SubtitleSlot->SetAnchors(FAnchors(.5f,Anchor));
	}
	if (auto* CaptionSlot = Cast<UCanvasPanelSlot>(CaptionBackground->Slot))
	{
		float Anchor = .13f;
		const FVector2D Safe = GetSafeCanvasSize();
		if (bHUDClearance && Safe.X > 1. && Safe.Y > 1.)
		{
			// Below the identity plate and the ammo readout, which would otherwise cover a raised-scale caption.
			const auto Layout = SovHolographicHUDLayout::Compute(Safe, Settings.UIScale);
			Anchor = FMath::Clamp(SovHolographicHUDLayout::PlaceCaptionTop(Layout, float(Safe.Y) * Anchor, GetSafeTextWidth() * .8f, bHUDAmmo) / float(Safe.Y), 0.f, .8f);
		}
		CaptionSlot->SetAnchors(FAnchors(.5f,Anchor));
	}
	CaptionText->SetWrapTextAt(FMath::Max(1.f,GetSafeTextWidth() * .8f));
	CaptionText->SetText(FText::Format(LOCTEXT("CaptionLayout","[sound] {0}\n{1}"),DirectionText(ActiveCaption.Location),CaptionPages.IsValidIndex(CaptionPageIndex) ? FText::FromString(CaptionPages[CaptionPageIndex]) : FText::GetEmpty()));
	// Objective fitting must see this frame's caption/speech text and visibility.
	RefreshObjectiveText();
}
void USovAccessibilityPresentation::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry,DeltaSeconds);
	const float LayoutHeight = SafeTextCanvas ? float(SafeTextCanvas->GetCachedGeometry().GetLocalSize().Y) : float(Geometry.GetLocalSize().Y);
	if(!FMath::IsNearlyEqual(LastLayoutWidth,GetSafeTextWidth(),1.f) || !FMath::IsNearlyEqual(LastLayoutHeight,LayoutHeight,1.f))
	{
		LastLayoutWidth=GetSafeTextWidth();
		LastLayoutHeight=LayoutHeight;
		if(!ActiveSpeech.Text.IsEmpty()) { BeginEntry(ActiveSpeech); }
		// Relayout a live caption without producing a duplicate scene-history record.
		if(CaptionRemaining > 0.f)
		{
			const int32 Characters=FMath::Min(Settings.SubtitleCharactersPerLine,FMath::Max(1,FMath::FloorToInt(LastLayoutWidth*.8f/(27.f*Settings.SubtitleScale))));
			CaptionPages=PaginateText(ActiveCaption.Text.ToString(),Characters,Settings.SubtitleMaximumLines); CaptionPageIndex=0;
			CaptionRemaining=FMath::Max(3.f,CaptionRemaining);
		}
		RefreshText();
	}
	if (!GetWorld() || GetWorld()->IsPaused()) { return; }
	if (SpeechPages.IsValidIndex(PageIndex))
	{
		PageRemaining -= DeltaSeconds;
		if (PageRemaining <= 0 && (PageIndex + 1 < SpeechPages.Num() || ActiveSpeech.Duration >= 0.f || ActiveSpeech.bFinished))
		{
			++PageIndex; PageRemaining = FMath::Max(2.f,ActiveSpeech.Duration / FMath::Max(1,SpeechPages.Num()));
			if (!SpeechPages.IsValidIndex(PageIndex)) { SpeechPages.Reset(); ActiveSpeech = FSovSceneSubtitleEntry(); if (!PendingSpeech.IsEmpty()) { const auto Next = PendingSpeech[0]; PendingSpeech.RemoveAt(0); BeginEntry(Next); } }
		}
	}
	if (CaptionRemaining>0.f)
	{
		CaptionRemaining=FMath::Max(0.f,CaptionRemaining-DeltaSeconds);
		if (CaptionRemaining<=0.f && CaptionPages.IsValidIndex(CaptionPageIndex+1)) { ++CaptionPageIndex; CaptionRemaining=CaptionPageDuration; }
	}
	if (CaptionRemaining <= 0.f && !PendingCaptions.IsEmpty())
	{
		int32 Next = 0;
		for (int32 Index = 1; Index < PendingCaptions.Num(); ++Index)
		{ if (PendingCaptions[Index].CaptionPriority > PendingCaptions[Next].CaptionPriority) { Next = Index; } }
		const FSovSceneSubtitleEntry Entry = PendingCaptions[Next]; PendingCaptions.RemoveAt(Next); BeginCaption(Entry);
	}
	RefreshText();
	MarkerRefreshRemaining -= DeltaSeconds; if (MarkerRefreshRemaining > 0) { return; } MarkerRefreshRemaining = .25f; Markers.Reset();
	APlayerController* PC = GetOwningPlayer(); if (!PC) { ObjectiveWaypoint = {}; return; }
    RefreshObjectiveWaypoint();
	if (Settings.bWeakPointOutlines && !Settings.bModifierBlackout)
	{
		RefreshWeakPointMarkers(PC);
	}
	if (auto* NarrativeNavigation = PC->FindComponentByClass<UNarrativeNavigationComponent>())
	{
		const FGameplayTag Domain = FNavigatorGameplayTags::Get().NavigatorTypes_Screenspace;
		for (UMapMarker* Marker : NarrativeNavigation->Markers)
		{
			if (Markers.Num() >= 48) { break; } if (!IsValid(Marker) || !Marker->HasDomain(Domain)) { continue; }
			FText Subtitle; const FText Title = Marker->GetMarkerDisplayText(NarrativeNavigation,Domain,Subtitle);
			Markers.Add({Marker->GetMarkerTransform().GetLocation(),Title,false,true});
		}
	}
}
void USovAccessibilityPresentation::RegisterWaypointSource(AActor* Actor)
{
    if (SovObjectiveWaypoint::IsSupportedSource(Actor)) { WaypointSources.AddUnique(Actor); }
}
void USovAccessibilityPresentation::ResetWaypointRegistry()
{
    if (WaypointWorld.IsValid() && WaypointSpawnedHandle.IsValid())
    { WaypointWorld->RemoveOnActorSpawnedHandler(WaypointSpawnedHandle); }
    WaypointSpawnedHandle.Reset(); WaypointWorld.Reset(); WaypointSources.Reset(); ObjectiveWaypoint = {};
}
void USovAccessibilityPresentation::RefreshObjectiveWaypoint()
{
    ObjectiveWaypoint = {};
    auto* PC = Cast<ASovPlayerController>(GetOwningPlayer());
    FSovCombatVitalsSnapshot Current;
    if (!Settings.bShowObjectiveText || IsCinematicControlled(PC) || Objectives.IsEmpty() || !GetWorld()
        || !USovCombatVitalsWidget::ReadCurrentVitals(PC, Current) || Current.Values[0].Current <= 0.f) { return; }
    if (WaypointWorld.Get() != GetWorld())
    {
        ResetWaypointRegistry(); WaypointWorld = GetWorld();
        for (TActorIterator<AActor> It(GetWorld()); It; ++It) { RegisterWaypointSource(*It); }
        WaypointSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
            FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::RegisterWaypointSource));
    }
    WaypointSources.RemoveAll([](const auto& Actor) { return !Actor.IsValid() || Actor->IsActorBeingDestroyed(); });
    SovObjectiveWaypoint::Resolve(PC, Objectives, WaypointSources, ObjectiveWaypoint);
}
void USovAccessibilityPresentation::RegisterMarkerCharacter(AActor* Actor)
{
	if (auto* Character = Cast<ANarrativeCharacter>(Actor)) { MarkerCharacters.AddUnique(Character); }
}
void USovAccessibilityPresentation::ResetMarkerRegistry()
{
	if (MarkerWorld.IsValid() && ActorSpawnedHandle.IsValid()) { MarkerWorld->RemoveOnActorSpawnedHandler(ActorSpawnedHandle); }
	ActorSpawnedHandle.Reset(); MarkerWorld.Reset(); MarkerCharacters.Reset(); RetainedMarkerCharacters.Reset(); MarkerCursor = 0;
}
void USovAccessibilityPresentation::RefreshWeakPointMarkers(APlayerController* PC)
{
	UWorld* World = GetWorld(); if (!World || !PC) { return; }
	if (MarkerWorld.Get() != World)
	{
		ResetMarkerRegistry(); MarkerWorld = World;
		// One roster build per world; subsequent spawns join without rescanning the world's actor list.
		for (TActorIterator<ANarrativeCharacter> It(World); It; ++It) { RegisterMarkerCharacter(*It); }
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::RegisterMarkerCharacter));
	}
	TArray<TWeakObjectPtr<ANarrativeCharacter>> Candidates = RetainedMarkerCharacters;
	const int32 Count = FMath::Min(256, MarkerCharacters.Num());
	for (int32 Inspected = 0; Inspected < Count; ++Inspected)
	{ Candidates.AddUnique(MarkerCharacters[int32(SovPlayerInformationPolicy::TakeNext(size_t(MarkerCharacters.Num()), MarkerCursor))]); }
	if (MarkerCursor == 0)
	{ MarkerCharacters.RemoveAll([](const auto& Character) { return !Character.IsValid() || Character->IsActorBeingDestroyed(); }); }

	FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View, Rotation);
	struct FVisibleAnchor { FVector Location; TWeakObjectPtr<ANarrativeCharacter> Character; double Rank; };
	TArray<FVisibleAnchor> Visible;
	for (const auto& Candidate : Candidates)
	{
		ANarrativeCharacter* Character = Candidate.Get();
		if (!Character || Character->IsActorBeingDestroyed() || Character == PC->GetPawn()
			|| FVector::DistSquared(Character->GetActorLocation(), View) > FMath::Square(5000.f)) { continue; }
		const auto* Weak = Character->FindComponentByClass<USovWeakPointComponent>();
		if (!Weak) { continue; }
		const TArray<FVector> Anchors = Weak->GetRevealedWeakPointAnchors();
		if (Anchors.IsEmpty() || !PC->LineOfSightTo(Character)) { continue; }
		for (const FVector& Anchor : Anchors)
		{
			const FVector Delta = Anchor - View;
			const double Facing = FVector::DotProduct(Delta.GetSafeNormal(), Rotation.Vector());
			if (Anchor.ContainsNaN() || Facing <= 0.) { continue; }
			// Center-of-view relevance wins, then distance; only admitted, revealed anchors participate.
			Visible.Add({Anchor, Character, (1. - Facing) * 25000000. + Delta.SizeSquared()});
		}
	}
	Visible.StableSort([](const FVisibleAnchor& A, const FVisibleAnchor& B) { return A.Rank < B.Rank; });
	RetainedMarkerCharacters.Reset();
	for (int32 Index = 0; Index < FMath::Min(32, Visible.Num()); ++Index)
	{
		Markers.Add({Visible[Index].Location, LOCTEXT("WeakPoint", "Weak point"), true, false});
		RetainedMarkerCharacters.AddUnique(Visible[Index].Character);
	}
}
int32 USovAccessibilityPresentation::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args,Geometry,CullingRect,Elements,Layer,Style,bParentEnabled);
	APlayerController* PC = GetOwningPlayer(); if (!PC) { return Layer; }
	const float DPI = UWidgetLayoutLibrary::GetViewportScale(this); if (DPI <= 0) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize(); const auto Font = FCoreStyle::GetDefaultFontStyle("Bold",FMath::RoundToInt(18 * Settings.UIScale));
    // Child paint geometry shares this window origin; tick geometry includes the desktop offset.
    // A restrained faction accent leaves the actual objective text in the corner.
    // No surrounding frame or lower corners expand its apparent footprint.
    if (ObjectiveBackground && ObjectiveBackground->IsRendered())
    {
        const auto* Player = Cast<ASovPlayerCharacterBase>(PC->GetPawn());
        const auto Theme = SovHUDStyle::ForProtagonist(Player ? Player->GetProtagonistIdentityTag() : FGameplayTag(), Settings.bHighContrastHUD);
        const float FrameScale = FMath::IsFinite(Settings.UIScale) ? FMath::Clamp(Settings.UIScale, .75f, 2.f) : 1.f;
        const auto& ObjectiveGeometry = ObjectiveBackground->GetPaintSpaceGeometry();
        const FVector2D PanelSize = ObjectiveGeometry.GetLocalSize();
        if (PanelSize.X > 8.f * FrameScale && PanelSize.Y > 8.f * FrameScale)
        {
            const FVector2D Inset(2.f * FrameScale);
            const FVector2D Min = Geometry.AbsoluteToLocal(ObjectiveGeometry.LocalToAbsolute(FVector2D::ZeroVector)) + Inset;
            const FVector2D Max = Geometry.AbsoluteToLocal(ObjectiveGeometry.LocalToAbsolute(PanelSize)) - Inset;
            const FLinearColor Tint = Theme.Accent * Style.GetColorAndOpacityTint()
                * FLinearColor(1.f, 1.f, 1.f, ObjectiveBackground->GetRenderOpacity());
            const auto FrameLine = [&](const TArray<FVector2D>& Points, const float Width)
            {
                TArray<FVector2f> SlatePoints;
                SlatePoints.Reserve(Points.Num());
                for (const auto& Point : Points) { SlatePoints.Add(FVector2f(Point)); }
                FSlateDrawElement::MakeLines(Elements, ++Layer, Geometry.ToPaintGeometry(), SlatePoints,
                    ESlateDrawEffect::None, Tint, true, Width * FrameScale);
            };
            Elements.PushClip(FSlateClippingZone(ObjectiveGeometry));
            const float Stroke = Settings.bHighContrastHUD ? 2.f : 1.f;
            FrameLine({Min, FVector2D(Min.X, FMath::Min(Max.Y, Min.Y + 56. * FrameScale))}, Stroke);
            if (Max.X - Min.X > 8. * FrameScale)
            {
                FrameLine({FVector2D(Min.X + 8. * FrameScale, Min.Y),
                    FVector2D(FMath::Min(Max.X, Min.X + 128. * FrameScale), Min.Y)}, Stroke);
            }
            Elements.PopClip();
        }
    }
	auto Project = [&](const FVector& Location,FVector2D& Point) { return PC->ProjectWorldLocationToScreen(Location,Point,true) && (Point /= DPI, true) && Point.X >= 16 && Point.Y >= 16 && Point.X < Size.X-16 && Point.Y < Size.Y-16; };
	auto DrawOutline = [&](const TArray<FVector2D>& Points,const FLinearColor& Tint)
	{
		TArray<FVector2f> SlatePoints; SlatePoints.Reserve(Points.Num()); for (const auto& Point : Points) { SlatePoints.Add(FVector2f(Point)); }
		FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),SlatePoints,ESlateDrawEffect::None,FLinearColor::Black,true,Settings.OutlineThickness+4);
		FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),MoveTemp(SlatePoints),ESlateDrawEffect::None,Tint,true,Settings.OutlineThickness);
	};
	// Use arranged paint geometry, including padding, wrapping, DPI and the safe-area offset.
	// Hidden panels reserve no space; labels return to their projected location when speech ends.
	TArray<FBox2D> TextPanels;
	for (const UBorder* Panel : {SubtitleBackground.Get(), CaptionBackground.Get(), ObjectiveBackground.Get()})
	{
		if (!Panel || !Panel->IsRendered()) { continue; }
		const FGeometry& PanelGeometry = Panel->GetPaintSpaceGeometry();
		if (PanelGeometry.GetLocalSize().IsNearlyZero()) { continue; }
		TextPanels.Emplace(Geometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector)),
			Geometry.AbsoluteToLocal(PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize())));
	}
	auto DrawLabel = [&](const FVector2D& Point,const FText& Text,const FLinearColor& Tint, bool bHolographic = false)
	{
		if (!SafeTextCanvas || !FSlateApplication::IsInitialized()) { return; }
		const FGeometry& SafeGeometry=SafeTextCanvas->GetPaintSpaceGeometry();
		if (SafeGeometry.GetLocalSize().X <= 0 || SafeGeometry.GetLocalSize().Y <= 0) { return; }
		const FVector2D SafeMin=Geometry.AbsoluteToLocal(SafeGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D SafeMax=Geometry.AbsoluteToLocal(SafeGeometry.LocalToAbsolute(SafeGeometry.GetLocalSize()));
		const FVector2D TextSize=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text,Font);
		const FVector2D Padding = bHolographic ? FVector2D(8,5) * Settings.UIScale : FVector2D::ZeroVector;
		const FVector2D PanelSize = TextSize + Padding * 2. + FVector2D(2,2);
		FVector2D LabelPoint;
		if (!SovWorldLabelLayout::Place(Point, PanelSize, FBox2D(SafeMin,SafeMax), TextPanels, LabelPoint))
		{ return; }
		// Move only the label into the safe area; the weak point/interactable outline stays on its target.
		Elements.PushClip(FSlateClippingZone(SafeGeometry));
		if (bHolographic)
		{
			static const FSlateColorBrush Glass(FLinearColor::White);
			FSlateDrawElement::MakeBox(Elements,++Layer,Geometry.ToPaintGeometry(PanelSize,FSlateLayoutTransform(LabelPoint)),
				&Glass,ESlateDrawEffect::None,Settings.bHighContrastHUD ? FLinearColor::Black : FLinearColor(.008f,.016f,.025f,.78f));
			TArray<FVector2f> Lip = {FVector2f(LabelPoint+FVector2D(0,PanelSize.Y)),FVector2f(LabelPoint),
				FVector2f(LabelPoint+FVector2D(PanelSize.X*.65,0))};
			FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),MoveTemp(Lip),ESlateDrawEffect::None,Tint,true,1.f);
			LabelPoint += Padding;
		}
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(LabelPoint+FVector2D(2,2))),Text,Font,ESlateDrawEffect::None,FLinearColor::Black);
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(LabelPoint)),Text,Font,ESlateDrawEffect::None,Tint);
		Elements.PopClip();
	};
    FSovCombatVitalsSnapshot CurrentVitals;
    const auto* SovPC = Cast<ASovPlayerController>(PC);
    if (!Settings.bModifierBlackout && Settings.bShowObjectiveText && !IsCinematicControlled(SovPC) && SafeTextCanvas && USovCombatVitalsWidget::ReadCurrentVitals(SovPC, CurrentVitals)
        && CurrentVitals.Values[0].Current > 0.f && SovObjectiveWaypoint::IsCurrent(SovPC, ObjectiveWaypoint))
    {
        const auto& SafeGeometry = SafeTextCanvas->GetPaintSpaceGeometry();
        const FVector2D SafeSize = SafeGeometry.GetLocalSize();
        if (SafeSize.X > 0. && SafeSize.Y > 0.)
        {
            const FVector2D Min = Geometry.AbsoluteToLocal(SafeGeometry.LocalToAbsolute(FVector2D::ZeroVector));
            const FVector2D Max = Geometry.AbsoluteToLocal(SafeGeometry.LocalToAbsolute(SafeSize));
            // Keep the outer safe-edge ring available for directional threat cues.
            const double WaypointInset = 72. * Settings.UIScale;
            const FVector Location = ObjectiveWaypoint.Anchor->GetComponentLocation();
            FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View, Rotation);
            const FVector Delta = Location - View;
            const double Forward = FVector::DotProduct(Delta, Rotation.Vector());
            FVector2D Screen = FVector2D::ZeroVector;
            const bool bFront = Forward > 0. && PC->ProjectWorldLocationToScreen(Location, Screen, true);
            if (bFront) { Screen /= DPI; }
            const FVector2D Bearing(FVector::DotProduct(Delta, Rotation.RotateVector(FVector::RightVector)), -Forward);
            FVector2D Point; bool bAtEdge = false;
            if (SovObjectiveWaypoint::FitToSafeRect(Screen, bFront, Bearing,
                Min + FVector2D(WaypointInset, WaypointInset), Max - FVector2D(WaypointInset, WaypointInset), Point, bAtEdge))
            {
                const auto Theme = SovHUDStyle::ForProtagonist(CurrentVitals.Protagonist, Settings.bHighContrastHUD);
                FLinearColor Tint = Settings.bNavigationContrast ? FLinearColor::White : Theme.Accent;
                Tint.A = SovAccessibilityPolicy::Pulse(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f, Settings.bNavigationPulse);
                const double Radius = 17. * Settings.UIScale;
                auto Hologram = [&](const TArray<FVector2D>& Points)
                {
                    TArray<FVector2f> Line; for (const auto& P : Points) { Line.Add(FVector2f(P)); }
                    if (!Settings.bHighContrastHUD)
                    {
                        FLinearColor Halo = Tint; Halo.A *= .12f;
                        FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),Line,ESlateDrawEffect::None,Halo,true,8.f*Settings.UIScale);
                    }
                    FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),Line,ESlateDrawEffect::None,FLinearColor(.005f,.012f,.018f,.88f),true,Settings.OutlineThickness+3.f);
                    FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),MoveTemp(Line),ESlateDrawEffect::None,Tint,true,Settings.OutlineThickness);
                };
                if (bAtEdge)
                {
                    const FVector2D Direction = (Point - (Min + Max) * .5).GetSafeNormal();
                    const FVector2D Side(-Direction.Y, Direction.X);
                    Hologram({Point + Direction*Radius, Point - Direction*Radius + Side*Radius*.7,
                        Point - Direction*Radius - Side*Radius*.7, Point + Direction*Radius});
                    Hologram({Point-Direction*Radius*1.5+Side*Radius*.45,Point-Direction*Radius*.8,
                        Point-Direction*Radius*1.5-Side*Radius*.45});
                }
                else if (Theme.Frame == SovHUDStyle::EFrame::Shield)
                {
                    Hologram({Point+FVector2D(-Radius,-Radius), Point+FVector2D(Radius,-Radius),
                        Point+FVector2D(Radius,Radius*.45), Point+FVector2D(0,Radius),
                        Point+FVector2D(-Radius,Radius*.45), Point+FVector2D(-Radius,-Radius)});
                    Hologram({Point+FVector2D(-Radius*.55,-Radius*.35),Point+FVector2D(0,Radius*.35),
                        Point+FVector2D(Radius*.55,-Radius*.35)});
                }
                else
                {
                    Hologram({Point+FVector2D(-Radius*.2,-Radius*.8),Point+FVector2D(-Radius,0),Point+FVector2D(-Radius*.2,Radius*.8)});
                    Hologram({Point+FVector2D(Radius*.2,-Radius*.8),Point+FVector2D(Radius,0),Point+FVector2D(Radius*.2,Radius*.8)});
                    Hologram({Point+FVector2D(0,-Radius*.25),Point+FVector2D(Radius*.25,0),
                        Point+FVector2D(0,Radius*.25),Point+FVector2D(-Radius*.25,0),Point+FVector2D(0,-Radius*.25)});
                }
                FNumberFormattingOptions DistanceFormat; DistanceFormat.SetMaximumFractionalDigits(0);
                const FText Distance = FText::AsNumber(FVector::Dist(Location, PC->GetPawn()->GetActorLocation()) / 100., &DistanceFormat);
                const TCHAR* Key = ObjectiveWaypoint.Kind == FSovObjectiveWaypoint::EKind::Retry ? TEXT("Waypoint.Retry")
                    : ObjectiveWaypoint.Kind == FSovObjectiveWaypoint::EKind::Receiver ? TEXT("Waypoint.Receiver")
                    : ObjectiveWaypoint.Kind == FSovObjectiveWaypoint::EKind::Encounter ? TEXT("Waypoint.Encounter") : TEXT("Waypoint.Interaction");
                const FText Label = FText::Format(SovHUDStyle::Text(Key), Distance);
                if (FSlateApplication::IsInitialized())
                {
                    const FVector2D LabelSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font)
                        + FVector2D(16,10)*Settings.UIScale + FVector2D(2,2);
                    const FVector2D Center = (Min + Max) * .5;
                    // Text grows inward, rather than covering the outer threat-indicator band.
                    const FVector2D LabelPoint(Point.X > Center.X ? Point.X-Radius-6.-LabelSize.X : Point.X+Radius+6.,
                        Point.Y > Center.Y ? Point.Y-Radius-6.-LabelSize.Y : Point.Y+Radius+6.);
                    DrawLabel(LabelPoint, Label, Tint, true);
                }
            }
        }
    }
	if (Settings.bInteractableOutlines && FocusedInteractable.IsValid() && Interaction && Interaction->IsInteractableInReach(FocusedInteractable.Get()))
	{
		const FBox Bounds = FocusedInteractable->GetInteractableBounds(); FVector2D Min(FLT_MAX,FLT_MAX),Max(-FLT_MAX,-FLT_MAX); bool bValid = Bounds.IsValid != 0;
		for (int32 Index=0; Index<8 && bValid; ++Index)
		{
			FVector2D Point; const FVector Corner(Index&1 ? Bounds.Max.X : Bounds.Min.X,Index&2 ? Bounds.Max.Y : Bounds.Min.Y,Index&4 ? Bounds.Max.Z : Bounds.Min.Z);
			bValid = Project(Corner,Point); if (bValid) { Min.X=FMath::Min(Min.X,Point.X); Min.Y=FMath::Min(Min.Y,Point.Y); Max.X=FMath::Max(Max.X,Point.X); Max.Y=FMath::Max(Max.Y,Point.Y); }
		}
		if (bValid) { DrawOutline({Min,FVector2D(Max.X,Min.Y),Max,FVector2D(Min.X,Max.Y),Min},TeamTint(Settings)); DrawLabel(FVector2D(Min.X,Max.Y+4),FText::Format(LOCTEXT("InteractLabel","Interact: {0}"),FocusedInteractable->GetInteractableNameText(PC->GetPawn(),Interaction)),FLinearColor::White); }
	}
	for (const FMarker& Marker : Markers)
	{
        if (Settings.bModifierBlackout && (Marker.bThreat || Marker.bNavigation)) { continue; }
		FVector2D Point; if (!Project(Marker.Location,Point)) { continue; }
		const float Radius = (Marker.bThreat ? 13.f : 9.f) * Settings.UIScale;
		FLinearColor Tint = Marker.bThreat ? ThreatTint(Settings) : TeamTint(Settings);
		if (Marker.bNavigation && Settings.bNavigationContrast) { Tint = FLinearColor::White; }
		if (Marker.bNavigation) { Tint.A = SovAccessibilityPolicy::Pulse(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f,Settings.bNavigationPulse); }
		if (Marker.bThreat) { DrawOutline({Point+FVector2D(0,-Radius),Point+FVector2D(Radius,0),Point+FVector2D(0,Radius),Point+FVector2D(-Radius,0),Point+FVector2D(0,-Radius)},Tint); }
		else { DrawOutline({Point+FVector2D(-Radius,Radius),Point+FVector2D(0,-Radius),Point+FVector2D(Radius,Radius),Point+FVector2D(-Radius,Radius)},Tint); }
		DrawLabel(Point+FVector2D(Radius+5,0),Marker.Text,Settings.bHighContrastHUD ? FLinearColor::White : Tint);
	}
	return Layer;
}
#undef LOCTEXT_NAMESPACE
