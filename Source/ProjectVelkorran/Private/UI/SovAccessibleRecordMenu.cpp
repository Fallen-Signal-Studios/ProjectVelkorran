// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovAccessibleRecordMenu.h"
#include "UI/SovAccessibilitySettingsMenu.h"
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovFrontendComponent.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEvidenceDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "SovAccessibleRecords"
USovAccessibleRecordMenu::USovAccessibleRecordMenu() { InputConfig=ENarrativeWidgetInputMode::Menu; }
void USovAccessibleRecordMenu::SetSceneHistoryMode(bool bValue) { if(bRetiring) { return; } ++ViewGeneration; bSceneHistory=bValue; Selection=0; RebuildRecords(); ShowRecord(true); }
FText USovAccessibleRecordMenu::DescribeEvidence(const USovEvidenceDefinition* Definition,ESovEvidenceStage Stage)
{
	if (!Definition || Stage==ESovEvidenceStage::Unknown) { return LOCTEXT("Unavailable","No acquired record available."); }
	static const FText Stages[]={LOCTEXT("Unknown","Unknown"),LOCTEXT("Observed","Observed"),LOCTEXT("Questioned","Questioned"),LOCTEXT("Corroborated","Corroborated"),LOCTEXT("Authenticated","Authenticated"),LOCTEXT("Distributed","Distributed")};
	return FText::Format(LOCTEXT("EvidenceSummary","Record summary: {0}\n\nProvenance stage: {1}\n\nObserved facts: {2}\n\nInterpretation: {3}"),Definition->Summary,
		Stages[FMath::Clamp(int32(Stage),0,5)],Definition->ObservedFacts.IsEmpty() ? LOCTEXT("UnclassifiedFacts","No separately classified fact summary is recorded.") : Definition->ObservedFacts,
		Definition->Interpretation.IsEmpty() ? LOCTEXT("UnclassifiedInterpretation","No separately classified interpretation is recorded. The record summary is not automatically an established fact.") : Definition->Interpretation);
}
TSharedRef<SWidget> USovAccessibleRecordMenu::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree")); }
	if (!Body)
	{
		USafeZone* Safe=WidgetTree->ConstructWidget<USafeZone>(); UBorder* Background=WidgetTree->ConstructWidget<UBorder>(); Background->SetBrushColor(FLinearColor(.015f,.02f,.025f,.99f)); Background->SetPadding(FMargin(30));
		UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(); UVerticalBox* Box=WidgetTree->ConstructWidget<UVerticalBox>();
		Heading=WidgetTree->ConstructWidget<UTextBlock>(); Heading->SetAutoWrapText(true); Box->AddChild(Heading);
		Body=WidgetTree->ConstructWidget<UTextBlock>(); Body->SetAutoWrapText(true); Body->SetMargin(FMargin(10,20)); Box->AddChild(Body);
		auto AddButton=[&](const FText& Text)
		{
			auto* Button=WidgetTree->ConstructWidget<USovAccessibilityNativeButton>(); auto* Label=WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Text); Label->SetAutoWrapText(true); Label->SetMargin(FMargin(10,12));
			Button->AddChild(Label); Button->SetAccessibleLabel(Text); Box->AddChild(Button); return Button;
		};
		PreviousButton=AddButton(LOCTEXT("Previous","Previous record")); NextButton=AddButton(LOCTEXT("Next","Next record"));
		ReadButton=AddButton(LOCTEXT("Read","Read current summary")); CloseButton=AddButton(LOCTEXT("Close","Close review"));
		PreviousButton->OnClicked.AddDynamic(this,&ThisClass::Previous); NextButton->OnClicked.AddDynamic(this,&ThisClass::Next); ReadButton->OnClicked.AddDynamic(this,&ThisClass::Read); CloseButton->OnClicked.AddDynamic(this,&ThisClass::Close);
		Scroll->AddChild(Box); Background->AddChild(Scroll); Safe->AddChild(Background); WidgetTree->RootWidget=Safe;
	}
	ShowRecord(false); return Super::RebuildWidget();
}
void USovAccessibleRecordMenu::NativeConstruct()
{
	bRetiring=false; const uint64 Expected=++ViewGeneration;
	BoundSettings=USovGameUserSettings::Get();
	if(BoundSettings) { BoundSettings->OnUserSettingsChanged.AddUniqueDynamic(this,&ThisClass::SettingsChanged); }
	if(auto* PC=GetOwningPlayer())
	{
		BoundCampaign=PC->FindComponentByClass<USovCampaignStateComponent>();
		if(BoundCampaign) { BoundCampaign->OnEvidenceRecorded.AddUniqueDynamic(this,&ThisClass::EvidenceChanged); BoundCampaign->OnCampaignStateRestored.AddUniqueDynamic(this,&ThisClass::CampaignRestored); BoundCampaign->OnMissionChanged.AddUniqueDynamic(this,&ThisClass::MissionChanged); }
		if(auto* Frontend=PC->FindComponentByClass<USovFrontendComponent>()) { BoundPresentation=Frontend->GetPresentation(); if(BoundPresentation) { BoundPresentation->OnSceneHistoryChanged.AddUniqueDynamic(this,&ThisClass::HistoryChanged); } }
	}
	Super::NativeConstruct(); if(bRetiring || Expected!=ViewGeneration) { return; }
	RebuildRecords(); ShowRecord(false);
}
void USovAccessibleRecordMenu::NativeDestruct()
{
	bRetiring=true; const uint64 Expected=++ViewGeneration;
	if(BoundSettings) { BoundSettings->OnUserSettingsChanged.RemoveDynamic(this,&ThisClass::SettingsChanged); }
	if(BoundPresentation) { BoundPresentation->OnSceneHistoryChanged.RemoveDynamic(this,&ThisClass::HistoryChanged); }
	if(BoundCampaign) { BoundCampaign->OnEvidenceRecorded.RemoveDynamic(this,&ThisClass::EvidenceChanged); BoundCampaign->OnCampaignStateRestored.RemoveDynamic(this,&ThisClass::CampaignRestored); BoundCampaign->OnMissionChanged.RemoveDynamic(this,&ThisClass::MissionChanged); }
	BoundPresentation=nullptr; BoundCampaign=nullptr;
	Records.Reset(); BoundSettings=nullptr; Super::NativeDestruct();
	if(Expected!=ViewGeneration || !bRetiring) { return; }
	if(GetOwningLocalPlayer()) { if(auto* Narrator=GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
}
void USovAccessibleRecordMenu::NativeOnActivated()
{
	const uint64 Expected=++ViewGeneration; Super::NativeOnActivated();
	if(bRetiring || Expected!=ViewGeneration || !IsActivated()) { return; }
	RebuildRecords(); ShowRecord(true);
}
void USovAccessibleRecordMenu::NativeOnDeactivated()
{
	const uint64 Expected=++ViewGeneration;
	Records.Reset(); if(Body) { Body->SetText(FText::GetEmpty()); } Super::NativeOnDeactivated();
	if(Expected!=ViewGeneration || IsActivated()) { return; }
	if(GetOwningLocalPlayer()) { if(auto* Narrator=GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
}
UWidget* USovAccessibleRecordMenu::NativeGetDesiredFocusTarget() const { return Records.Num()>1 ? NextButton.Get() : CloseButton.Get(); }
void USovAccessibleRecordMenu::RebuildRecords()
{
	Records.Reset(); APlayerController* PC=GetOwningPlayer(); if(!PC) { return; }
	if(bSceneHistory)
	{
		if(auto* Frontend=PC->FindComponentByClass<USovFrontendComponent>())
		{
			if(auto* Presentation=Frontend->GetPresentation())
			{ for(const auto& Entry:Presentation->GetSceneHistory()) { Records.Add(FText::Format(LOCTEXT("HistoryEntry","{0}: {1}"),Entry.bCaption ? LOCTEXT("Sound","Sound") : Entry.Speaker,Entry.Text)); } }
		}
	}
	else if(auto* Campaign=PC->FindComponentByClass<USovCampaignStateComponent>())
	{
		if(!Campaign->IsStateValid() || !Campaign->GetActiveProtagonist().IsValid()) { return; }
		TSet<FName> Seen; const FGameplayTag Protagonist=Campaign->GetActiveProtagonist();
		for(const FSovEvidenceAcquisition& Acquisition:Campaign->GetEvidence())
		{
			if(!IsValid(Acquisition.Definition) || Seen.Contains(Acquisition.EvidenceId) || !Campaign->KnowsEvidence(Acquisition.EvidenceId,Protagonist)) { continue; }
			Seen.Add(Acquisition.EvidenceId); Records.Add(DescribeEvidence(Acquisition.Definition,Campaign->GetEvidenceStage(Acquisition.EvidenceId,Protagonist)));
		}
	}
	Selection=FMath::Clamp(Selection,0,FMath::Max(0,Records.Num()-1));
}
void USovAccessibleRecordMenu::ShowRecord(bool bAnnounce)
{
	if(bRetiring || !Body || !Heading || !PreviousButton || !NextButton || !ReadButton || !CloseButton) { return; } const auto Settings=BoundSettings ? BoundSettings->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
	SetMenuNavigationWrap(Settings.bMenuNavigationWrap); const auto Font=FCoreStyle::GetDefaultFontStyle("Regular",FMath::RoundToInt(22*Settings.UIScale));
	Body->SetFont(Font); Heading->SetFont(Font); Body->SetColorAndOpacity(FSlateColor(FLinearColor::White)); Heading->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	for(auto* Button:{PreviousButton.Get(),NextButton.Get(),ReadButton.Get(),CloseButton.Get()})
	{ if(auto* Label=Cast<UTextBlock>(Button->GetContent())) { Label->SetFont(Font); Label->SetColorAndOpacity(FSlateColor(FLinearColor::White)); } Button->SetBackgroundColor(FLinearColor::Black); }
	Heading->SetText(FText::Format(LOCTEXT("Heading","{0} — {1} of {2}"),bSceneHistory ? LOCTEXT("SceneHistory","Current scene history") : LOCTEXT("Evidence","Acquired evidence"),FText::AsNumber(Records.Num() ? Selection+1 : 0),FText::AsNumber(Records.Num())));
	Body->SetText(Records.IsValidIndex(Selection) ? Records[Selection] : LOCTEXT("Empty","No records available to the current protagonist in this view."));
	PreviousButton->SetIsEnabled(Records.Num()>1); NextButton->SetIsEnabled(Records.Num()>1); ReadButton->SetIsEnabled(!Records.IsEmpty());
	if(bAnnounce && Settings.bMenuNarration) { Read(); }
}
void USovAccessibleRecordMenu::Previous() { if(bRetiring || !IsActivated()) { return; } ++ViewGeneration; RebuildRecords(); if(!Records.IsEmpty()) { Selection=(Selection+Records.Num()-1)%Records.Num(); } ShowRecord(true); }
void USovAccessibleRecordMenu::Next() { if(bRetiring || !IsActivated()) { return; } ++ViewGeneration; RebuildRecords(); if(!Records.IsEmpty()) { Selection=(Selection+1)%Records.Num(); } ShowRecord(true); }
void USovAccessibleRecordMenu::Read()
{
	// Recheck knowledge and scene ownership at the exact announcement boundary.
	if(bRetiring || !IsActivated() || !Heading || !Body) { return; } const uint64 Expected=ViewGeneration;
	RebuildRecords(); ShowRecord(false);
	if(!Records.IsValidIndex(Selection) || !BoundSettings || !BoundSettings->GetSettingsSnapshot().bMenuNarration || !GetOwningLocalPlayer()) { return; }
	if(auto* Narrator=GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>())
	{ FGuid Request; if(!Narrator->Announce(this,FText::Format(LOCTEXT("Readout","{0}. {1}"),Heading->GetText(),Records[Selection]),Request) && Expected==ViewGeneration && !bRetiring && IsActivated() && Heading) { Heading->SetText(Narrator->GetUnavailableReason()); } }
}
void USovAccessibleRecordMenu::Close() { DeactivateWidget(); }
void USovAccessibleRecordMenu::SettingsChanged(const FSovUserSettingsSnapshot& Value)
{
	if(bRetiring) { return; } const uint64 Expected=++ViewGeneration;
	if(!Value.bMenuNarration && GetOwningLocalPlayer()) { if(auto* Narrator=GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
	if(Expected!=ViewGeneration || bRetiring) { return; }
	ShowRecord(false);
}
void USovAccessibleRecordMenu::HistoryChanged()
{
	if(bSceneHistory && IsActivated())
	{
		const uint64 Expected=++ViewGeneration;
		if(GetOwningLocalPlayer()) { if(auto* Narrator=GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
		if(Expected!=ViewGeneration || bRetiring || !IsActivated()) { return; }
		RebuildRecords(); ShowRecord(false);
	}
}
void USovAccessibleRecordMenu::EvidenceChanged(const FSovEvidenceAcquisition& Value)
{
	if(!bSceneHistory && IsActivated())
	{
		const uint64 Expected=++ViewGeneration;
		if(GetOwningLocalPlayer()) { if(auto* Narrator=GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
		if(Expected!=ViewGeneration || bRetiring || !IsActivated()) { return; }
		RebuildRecords(); ShowRecord(false);
	}
}
void USovAccessibleRecordMenu::CampaignRestored(bool bValid) { DeactivateWidget(); }
void USovAccessibleRecordMenu::MissionChanged(FName Mission,bool bSucceeded) { DeactivateWidget(); }
#undef LOCTEXT_NAMESPACE
