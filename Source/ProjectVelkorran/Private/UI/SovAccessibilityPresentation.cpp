// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovAccessibilityPolicy.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
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

#define LOCTEXT_NAMESPACE "SovAccessibilityPresentation"
USovAccessibilityPresentation::USovAccessibilityPresentation(const FObjectInitializer& Initializer) : Super(Initializer)
{ SetVisibility(ESlateVisibility::HitTestInvisible); }
TArray<FString> USovAccessibilityPresentation::PaginateText(const FString& Text, int32 CharactersPerLine, int32 MaximumLines)
{
	TArray<FString> Pages;
	if (Text.IsEmpty()) { return Pages; }
	CharactersPerLine = FMath::Clamp(CharactersPerLine, 1, 64); MaximumLines = FMath::Clamp(MaximumLines, 1, 4);
	// Unicode character boundaries preserve combining sequences/surrogates; UMG shapes the final localized lines.
	auto Iterator = FBreakIterator::CreateCharacterBoundaryIterator(); Iterator->SetString(Text);
	FString Page; int32 LineLength = 0, Lines = 1, Start = Iterator->MoveToBeginning();
	for (int32 End = Iterator->MoveToNext(); End != INDEX_NONE; Start = End, End = Iterator->MoveToNext())
	{
		const FString Character = Text.Mid(Start, End - Start);
		if (Character == TEXT("\r")) { continue; }
		const bool bNewLine = Character.Contains(TEXT("\n"));
		if (LineLength >= CharactersPerLine || bNewLine)
		{
			if (Lines >= MaximumLines) { Pages.Add(MoveTemp(Page)); Page.Reset(); Lines = 1; }
			else { Page += TEXT("\n"); ++Lines; }
			LineLength = 0;
		}
		if (!bNewLine) { Page += Character; ++LineLength; }
	}
	if (!Page.IsEmpty()) { Pages.Add(MoveTemp(Page)); }
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
		SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(); SubtitleText->SetJustification(ETextJustify::Center); SubtitleText->SetAutoWrapText(true);
		SubtitleBackground->AddChild(SubtitleText); SubtitleSlot = SafeTextCanvas->AddChildToCanvas(SubtitleBackground);
		SubtitleSlot->SetAnchors(FAnchors(.5f,.88f)); SubtitleSlot->SetAlignment(FVector2D(.5f,1)); SubtitleSlot->SetAutoSize(true);
		CaptionBackground = WidgetTree->ConstructWidget<UBorder>(); CaptionBackground->SetPadding(FMargin(14,8));
		CaptionText = WidgetTree->ConstructWidget<UTextBlock>(); CaptionText->SetJustification(ETextJustify::Center); CaptionText->SetAutoWrapText(true);
		CaptionBackground->AddChild(CaptionText); UCanvasPanelSlot* Slot = SafeTextCanvas->AddChildToCanvas(CaptionBackground);
		Slot->SetAnchors(FAnchors(.5f,.13f)); Slot->SetAlignment(FVector2D(.5f,0)); Slot->SetAutoSize(true);
	}
	RefreshText(); return Super::RebuildWidget();
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
	FocusedInteractable.Reset(); Interaction = nullptr; BoundSettings = nullptr; ClearSceneHistory(); Super::NativeDestruct();
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
	const float Width = GetSafeTextWidth();
	if (Width > 0) { Characters = FMath::Min(Characters,FMath::Max(1,FMath::FloorToInt(Width * .84f / (27.f * Settings.SubtitleScale)))); }
	SpeechPages = PaginateText(Entry.Text.ToString(), Characters, Settings.SubtitleMaximumLines); PageIndex = 0;
	PageRemaining = FMath::Max(2.f, Entry.Duration / FMath::Max(1,SpeechPages.Num())); RefreshText();
}
void USovAccessibilityPresentation::PresentCaption(const FText& Text, float Duration, const FVector& Location)
{
	if (Text.IsEmpty() || !FMath::IsFinite(Duration) || Location.ContainsNaN()) { return; }
	ActiveCaption.Text = Text; ActiveCaption.Location = Location; ActiveCaption.bCaption = true;
	const float Width=GetSafeTextWidth();
	const int32 Characters=Width>0 ? FMath::Min(Settings.SubtitleCharactersPerLine,FMath::Max(1,FMath::FloorToInt(Width*.8f/(27.f*Settings.SubtitleScale)))) : Settings.SubtitleCharactersPerLine;
	CaptionPages=PaginateText(Text.ToString(),Characters,Settings.SubtitleMaximumLines); CaptionPageIndex=0;
	CaptionPageDuration=FMath::Max(3.f,FMath::Clamp(Duration,3.f,30.f)/FMath::Max(1,CaptionPages.Num()));
	CaptionRemaining=CaptionPageDuration; History.Add(ActiveCaption); if (History.Num() > 64) { History.RemoveAt(0); } RefreshText();
	OnSceneHistoryChanged.Broadcast();
}
void USovAccessibilityPresentation::ClearSpeech()
{
	// The latest produced line owns the finish notification even when earlier pages are still readable.
	if (!PendingSpeech.IsEmpty()) { PendingSpeech.Last().bFinished = true; }
	else { ActiveSpeech.bFinished = true; }
}
void USovAccessibilityPresentation::ClearSceneHistory()
{ History.Reset(); PendingSpeech.Reset(); SpeechPages.Reset(); CaptionPages.Reset(); Markers.Reset(); ActiveSpeech = FSovSceneSubtitleEntry(); ActiveCaption = FSovSceneSubtitleEntry(); CaptionRemaining = 0; RefreshText(); OnSceneHistoryChanged.Broadcast(); }
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
	if (!SubtitleText || !CaptionText) { return; }
	const int32 Size = FMath::RoundToInt(26 * Settings.SubtitleScale);
	SubtitleText->SetWrapTextAt(FMath::Max(1.f,GetSafeTextWidth() * .84f));
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
		SubtitleSlot->SetAnchors(FAnchors(.5f,ActiveSpeech.bCinematic ? .9f : .8f));
	}
	CaptionText->SetWrapTextAt(FMath::Max(1.f,GetSafeTextWidth() * .8f));
	CaptionText->SetText(FText::Format(LOCTEXT("CaptionLayout","[sound] {0}\n{1}"),DirectionText(ActiveCaption.Location),CaptionPages.IsValidIndex(CaptionPageIndex) ? FText::FromString(CaptionPages[CaptionPageIndex]) : FText::GetEmpty()));
}
void USovAccessibilityPresentation::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry,DeltaSeconds);
	if(!FMath::IsNearlyEqual(LastLayoutWidth,GetSafeTextWidth(),1.f))
	{
		LastLayoutWidth=GetSafeTextWidth();
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
	RefreshText();
	MarkerRefreshRemaining -= DeltaSeconds; if (MarkerRefreshRemaining > 0) { return; } MarkerRefreshRemaining = .25f; Markers.Reset();
	APlayerController* PC = GetOwningPlayer(); if (!PC) { return; }
	if (Settings.bWeakPointOutlines)
	{
		int32 Inspected = 0;
		for (TActorIterator<ANarrativeCharacter> It(GetWorld()); It && Inspected++ < 256 && Markers.Num() < 32; ++It)
		{
			if (*It == PC->GetPawn() || FVector::DistSquared(It->GetActorLocation(),PC->GetFocalLocation()) > FMath::Square(5000.f) || !PC->LineOfSightTo(*It)) { continue; }
			if (const auto* Weak = It->FindComponentByClass<USovWeakPointComponent>())
			{ for (const FVector& Anchor : Weak->GetRevealedWeakPointAnchors()) { if (Markers.Num() >= 32) { break; } Markers.Add({Anchor,LOCTEXT("WeakPoint","Weak point"),true,false}); } }
		}
	}
	if (auto* Navigation = PC->FindComponentByClass<UNarrativeNavigationComponent>())
	{
		const FGameplayTag Domain = FNavigatorGameplayTags::Get().NavigatorTypes_Screenspace;
		for (UMapMarker* Marker : Navigation->Markers)
		{
			if (Markers.Num() >= 48) { break; } if (!IsValid(Marker) || !Marker->MarkerDomain.HasTagExact(Domain)) { continue; }
			FText Subtitle; const FText Title = Marker->GetMarkerDisplayText(Navigation,Domain,Subtitle);
			Markers.Add({Marker->GetMarkerTransform().GetLocation(),Title,false,true});
		}
	}
}
int32 USovAccessibilityPresentation::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	Layer = Super::NativePaint(Args,Geometry,CullingRect,Elements,Layer,Style,bParentEnabled);
	APlayerController* PC = GetOwningPlayer(); if (!PC) { return Layer; }
	const float DPI = UWidgetLayoutLibrary::GetViewportScale(this); if (DPI <= 0) { return Layer; }
	const FVector2D Size = Geometry.GetLocalSize(); const auto Font = FCoreStyle::GetDefaultFontStyle("Bold",FMath::RoundToInt(18 * Settings.UIScale));
	auto Project = [&](const FVector& Location,FVector2D& Point) { return PC->ProjectWorldLocationToScreen(Location,Point,true) && (Point /= DPI, true) && Point.X >= 16 && Point.Y >= 16 && Point.X < Size.X-16 && Point.Y < Size.Y-16; };
	auto DrawOutline = [&](const TArray<FVector2D>& Points,const FLinearColor& Tint)
	{
		TArray<FVector2f> SlatePoints; SlatePoints.Reserve(Points.Num()); for (const auto& Point : Points) { SlatePoints.Add(FVector2f(Point)); }
		FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),SlatePoints,ESlateDrawEffect::None,FLinearColor::Black,true,Settings.OutlineThickness+4);
		FSlateDrawElement::MakeLines(Elements,++Layer,Geometry.ToPaintGeometry(),MoveTemp(SlatePoints),ESlateDrawEffect::None,Tint,true,Settings.OutlineThickness);
	};
	auto DrawLabel = [&](const FVector2D& Point,const FText& Text,const FLinearColor& Tint)
	{
		if (!SafeTextCanvas || !FSlateApplication::IsInitialized()) { return; }
		const FGeometry& SafeGeometry=SafeTextCanvas->GetCachedGeometry();
		if (SafeGeometry.GetLocalSize().X <= 0 || SafeGeometry.GetLocalSize().Y <= 0) { return; }
		const FVector2D SafeMin=Geometry.AbsoluteToLocal(SafeGeometry.LocalToAbsolute(FVector2D::ZeroVector));
		const FVector2D SafeMax=Geometry.AbsoluteToLocal(SafeGeometry.LocalToAbsolute(SafeGeometry.GetLocalSize()));
		const FVector2D TextSize=FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text,Font);
		const FVector2D LabelPoint(FMath::Clamp(Point.X,SafeMin.X,FMath::Max(SafeMin.X,SafeMax.X-TextSize.X-2)),
			FMath::Clamp(Point.Y,SafeMin.Y,FMath::Max(SafeMin.Y,SafeMax.Y-TextSize.Y-2)));
		// Move only the label into the safe area; the weak point/interactable outline stays on its target.
		Elements.PushClip(FSlateClippingZone(SafeGeometry));
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(LabelPoint+FVector2D(2,2))),Text,Font,ESlateDrawEffect::None,FLinearColor::Black);
		FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(LabelPoint)),Text,Font,ESlateDrawEffect::None,Tint);
		Elements.PopClip();
	};
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
