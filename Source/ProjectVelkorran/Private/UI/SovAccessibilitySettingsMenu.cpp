// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovAccessibilitySettingsMenu.h"
#include "UI/SovAccessibleRecordMenu.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetNavigation.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SlateWrapperTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Styling/CoreStyle.h"
#include "UObject/UnrealType.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Widgets/SWidget.h"
#include "NarrativeGameplayTags.h"

#define LOCTEXT_NAMESPACE "SovAccessibilitySettings"
namespace
{
	double ReadNumber(FProperty* Property, void* Container)
	{
		if (!Property) { return 0.; }
		void* Address = Property->ContainerPtrToValuePtr<void>(Container);
		if (FBoolProperty* Bool = CastField<FBoolProperty>(Property)) { return Bool->GetPropertyValue(Address) ? 1. : 0.; }
		if (FEnumProperty* Enum = CastField<FEnumProperty>(Property)) { return Enum->GetUnderlyingProperty()->GetSignedIntPropertyValue(Address); }
		if (FNumericProperty* Number = CastField<FNumericProperty>(Property))
		{ return Number->IsFloatingPoint() ? Number->GetFloatingPointPropertyValue(Address) : Number->GetSignedIntPropertyValue(Address); }
		return 0.;
	}
	void WriteNumber(FProperty* Property, void* Container, double Value)
	{
		if (!Property) { return; }
		void* Address = Property->ContainerPtrToValuePtr<void>(Container);
		if (FBoolProperty* Bool = CastField<FBoolProperty>(Property)) { Bool->SetPropertyValue(Address, Value != 0.); }
		else if (FEnumProperty* Enum = CastField<FEnumProperty>(Property)) { Enum->GetUnderlyingProperty()->SetIntPropertyValue(Address, static_cast<int64>(Value)); }
		else if (FNumericProperty* Number = CastField<FNumericProperty>(Property))
		{ if (Number->IsFloatingPoint()) { Number->SetFloatingPointPropertyValue(Address, Value); } else { Number->SetIntPropertyValue(Address, static_cast<int64>(Value)); } }
	}
	float* ColorChannel(FSovUserSettingsSnapshot& Value, FName Key)
	{
		if (Key == "TeamRed") { return &Value.TeamColor.R; }
		if (Key == "TeamGreen") { return &Value.TeamColor.G; }
		if (Key == "TeamBlue") { return &Value.TeamColor.B; }
		if (Key == "ThreatRed") { return &Value.ThreatColor.R; }
		if (Key == "ThreatGreen") { return &Value.ThreatColor.G; }
		if (Key == "ThreatBlue") { return &Value.ThreatColor.B; }
		return nullptr;
	}
	double ChangedValue(double Value, const USovAccessibilitySettingRow* Row, int32 Direction)
	{
		const double New = Value + Row->Increment * Direction;
		return New > Row->Maximum + .001 ? Row->Minimum : New < Row->Minimum - .001 ? Row->Maximum : FMath::Clamp(New, double(Row->Minimum), double(Row->Maximum));
	}
}
void USovAccessibilitySettingRow::Configure(USovAccessibilitySettingsMenu* Owner, FName Key, const FText& Label, float Min, float Max, float Step)
{ Menu = Owner; SettingKey = Key; DisplayLabel = Label; Minimum = Min; Maximum = Max; Increment = Step; }
void USovAccessibilityNativeButton::SetAccessibleLabel(const FText& Label)
{
	NativeAccessibleLabel = Label;
	SynchronizeProperties();
}
void USovAccessibilityNativeButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
#if WITH_ACCESSIBILITY
	if (const TSharedPtr<SWidget> AccessibleWidget = GetAccessibleWidget())
	{
		AccessibleWidget->SetAccessibleBehavior(EAccessibleBehavior::Custom, NativeAccessibleLabel, EAccessibleType::Main);
		AccessibleWidget->SetAccessibleBehavior(EAccessibleBehavior::Custom, NativeAccessibleLabel, EAccessibleType::Summary);
		AccessibleWidget->SetCanChildrenBeAccessible(false);
	}
#endif
}
TSharedRef<SWidget> USovAccessibilitySettingRow::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this,TEXT("WidgetTree")); }
	if (!Button)
	{
		Button = WidgetTree->ConstructWidget<USovAccessibilityNativeButton>();
		Text = WidgetTree->ConstructWidget<UTextBlock>();
		Text->SetAutoWrapText(true);
		Text->SetMargin(FMargin(12.f, 8.f));
		Button->AddChild(Text);
		Button->OnClicked.AddDynamic(this, &ThisClass::Clicked);
		FCustomWidgetNavigationDelegate ChangeValue;
		ChangeValue.BindDynamic(this, &ThisClass::NavigateValue);
		// Slate's user/platform navigation configuration supplies both D-pad and analog directions.
		Button->SetNavigationRuleCustom(EUINavigation::Left, ChangeValue);
		Button->SetNavigationRuleCustom(EUINavigation::Right, ChangeValue);
		WidgetTree->RootWidget = Button;
	}
	Refresh();
	return Super::RebuildWidget();
}
void USovAccessibilitySettingRow::Refresh()
{
	if (!Menu || !Text || !Button) { return; }
	const FSovUserSettingsSnapshot Value = Menu->CurrentSettings();
	const FText Label = FText::Format(LOCTEXT("SettingValue", "{0}: {1}"), DisplayLabel, Menu->ValueText(this));
	Text->SetText(Label);
	Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(20.f * Value.UIScale)));
	Text->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Button->SetBackgroundColor(Value.bHighContrastHUD ? FLinearColor::Black : FLinearColor(.06f, .075f, .10f));
	Button->SetAccessibleLabel(Label);
	Button->SetIsEnabled(Menu->IsRowEnabled(this));
}
UWidget* USovAccessibilitySettingRow::GetFocusTarget() const { return Button; }
FText USovAccessibilitySettingRow::GetLabel() const { return Text ? Text->GetText() : DisplayLabel; }
void USovAccessibilitySettingRow::Clicked() { if (Menu) { Menu->Adjust(this, 1); } }
void USovAccessibilitySettingRow::NativeOnAddedToFocusPath(const FFocusEvent& Event)
{
	Super::NativeOnAddedToFocusPath(Event);
	if (Menu && Text) { Menu->FocusRow(this, Text->GetText()); }
}
UWidget* USovAccessibilitySettingRow::NavigateValue(EUINavigation Direction)
{
	if (Menu && Menu->IsActivated() && Menu->CanAdjustValue(this) &&
		(Direction == EUINavigation::Left || Direction == EUINavigation::Right))
	{ Menu->Adjust(this, Direction == EUINavigation::Left ? -1 : 1); }
	// Commands (Continue, cloud import, HDR confirm) require activation, never a sideways nudge.
	// Null consumes this custom navigation without proposing a focus target. A settings
	// callback may have opened a modal, and must retain CommonUI's resulting focus.
	return nullptr;
}
USovAccessibilitySettingsMenu::USovAccessibilitySettingsMenu() { InputConfig = ENarrativeWidgetInputMode::Menu; bIsBackHandler = true; }
void USovAccessibilitySettingsMenu::SetFirstBoot(bool bValue) { bFirstBoot = bValue; bDeactivateOnBack = !bValue; RefreshRows(); }
bool USovAccessibilitySettingsMenu::NativeOnHandleBackAction()
{
	if (bFirstBoot) { return true; } // Setup completion is an explicit Continue transaction.
	return Super::NativeOnHandleBackAction();
}
FSovUserSettingsSnapshot USovAccessibilitySettingsMenu::CurrentSettings() const
{ const USovGameUserSettings* Settings = BoundSettings ? BoundSettings.Get() : USovGameUserSettings::Get(); return Settings ? Settings->GetSettingsSnapshot() : FSovUserSettingsSnapshot(); }
void USovAccessibilitySettingsMenu::AddRow(FName Key, const FText& Label, float Min, float Max, float Step)
{
	USovAccessibilitySettingRow* Row = WidgetTree->ConstructWidget<USovAccessibilitySettingRow>();
	Row->Initialize();
	Row->Configure(this, Key, Label, Min, Max, Step);
	RowsBox->AddChild(Row); Rows.Add(Row);
}
TSharedRef<SWidget> USovAccessibilitySettingsMenu::RebuildWidget()
{
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this,TEXT("WidgetTree")); }
	if (!RowsBox)
	{
		USafeZone* Safe = WidgetTree->ConstructWidget<USafeZone>();
		UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(); Backdrop->SetBrushColor(FLinearColor(.015f, .02f, .025f, .98f)); Backdrop->SetPadding(FMargin(30.f));
		Scroll = WidgetTree->ConstructWidget<UScrollBox>();
		RowsBox = WidgetTree->ConstructWidget<UVerticalBox>();
		Status = WidgetTree->ConstructWidget<UTextBlock>(); Status->SetAutoWrapText(true); Status->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RowsBox->AddChild(Status); Scroll->AddChild(RowsBox); Backdrop->AddChild(Scroll); Safe->AddChild(Backdrop); WidgetTree->RootWidget = Safe;
		AddRow("bMenuNarration", LOCTEXT("Narration", "Menu and dialogue narration"));
		AddRow("UIScale", LOCTEXT("UIScale", "UI text scale"), 1.f, 2.f, .25f);
		AddRow("SubtitleScale", LOCTEXT("SubtitleScale", "Subtitle text scale"), 1.f, 2.5f, .25f);
		AddRow("bSubtitles", LOCTEXT("Subtitles", "Speech subtitles"));
		AddRow("bClosedCaptions", LOCTEXT("Captions", "Important sound captions"));
		AddRow("ControllerAudioVolume", LOCTEXT("ControllerAudio", "Controller audio channel volume"),0,1,.1f);
		AddRow("Audio.Overall",LOCTEXT("OverallAudio","Overall audio volume"),0,1,.1f);
		AddRow("Audio.Dialogue",LOCTEXT("DialogueAudio","Dialogue audio volume"),0,1,.1f);
		AddRow("Audio.Music",LOCTEXT("MusicAudio","Music volume"),0,1,.1f);
		AddRow("Audio.SFX",LOCTEXT("SFXAudio","Effects volume"),0,1,.1f);
		AddRow("Audio.Ambience",LOCTEXT("AmbienceAudio","Ambience volume"),0,1,.1f);
		AddRow("Audio.Tinnitus",LOCTEXT("TinnitusAudio","Tinnitus-like tones volume"),0,1,.1f);
		AddRow("Audio.DynamicRange",LOCTEXT("AudioRange","Audio dynamic range"),0,2,1);
		AddRow("Review.Evidence", LOCTEXT("ReviewEvidence", "Review acquired evidence summaries"));
		AddRow("Review.History", LOCTEXT("ReviewHistory", "Review current scene dialogue history"));
		AddRow("SubtitleBackgroundOpacity", LOCTEXT("SubtitleBackground", "Subtitle background opacity"), 0.f, 1.f, .1f);
		AddRow("bSubtitleSpeakerNames", LOCTEXT("SpeakerNames", "Subtitle speaker names"));
		AddRow("bSubtitleDirections", LOCTEXT("Directions", "Subtitle and caption directions"));
		AddRow("SubtitleCharactersPerLine", LOCTEXT("Characters", "Subtitle characters per line"), 20, 64, 2);
		AddRow("SubtitleMaximumLines", LOCTEXT("Lines", "Subtitle maximum lines per page"), 1, 4, 1);
		AddRow("bHighContrastHUD", LOCTEXT("HighContrast", "High contrast HUD"));
		AddRow("ColorVisionPreset", LOCTEXT("ColorVision", "Color vision palette"), 0, 3, 1);
		AddRow("bOverrideTeamColor", LOCTEXT("TeamOverride", "Independent team color"));
		AddRow("TeamRed", LOCTEXT("TeamR", "Team color red"), 0, 1, .1f);
		AddRow("TeamGreen", LOCTEXT("TeamG", "Team color green"), 0, 1, .1f);
		AddRow("TeamBlue", LOCTEXT("TeamB", "Team color blue"), 0, 1, .1f);
		AddRow("bOverrideThreatColor", LOCTEXT("ThreatOverride", "Independent threat color"));
		AddRow("ThreatRed", LOCTEXT("ThreatR", "Threat color red"), 0, 1, .1f);
		AddRow("ThreatGreen", LOCTEXT("ThreatG", "Threat color green"), 0, 1, .1f);
		AddRow("ThreatBlue", LOCTEXT("ThreatB", "Threat color blue"), 0, 1, .1f);
		AddRow("bInteractableOutlines", LOCTEXT("InteractableOutline", "Interactable bounds outlines"));
		AddRow("bWeakPointOutlines", LOCTEXT("WeakPointOutline", "Revealed weak point outlines"));
		AddRow("OutlineThickness", LOCTEXT("OutlineThickness", "Outline thickness"), 1, 6, 1);
		AddRow("bNavigationContrast", LOCTEXT("NavigationContrast", "High contrast navigation markers"));
		AddRow("bNavigationPulse", LOCTEXT("NavigationPulse", "Slow navigation pulse"));
		AddRow("bMenuNavigationWrap", LOCTEXT("Wrap", "Menu navigation wrap"));
		AddRow("DialoguePressureMode", LOCTEXT("Pressure", "Dialogue pressure timer"), 0, 2, 1);
		AddRow("DialogueMinimumReadSeconds", LOCTEXT("ReadTime", "Minimum dialogue reading seconds"), 2, 30, 1);
		AddRow("DialoguePressureExtension", LOCTEXT("PressureExtension", "Extended dialogue timer multiplier"), 1, 5, .5f);
		AddRow("Difficulty",LOCTEXT("Difficulty","Difficulty preset"),0,4,1);
		AddRow("IncomingDamageScale",LOCTEXT("IncomingDamage","Incoming damage multiplier"),.1f,2,.1f);
		AddRow("EnemyRecoveryScale",LOCTEXT("EnemyRecovery","Enemy recovery multiplier"),.5f,2,.1f);
		AddRow("bAllowCompanionRescue",LOCTEXT("CompanionRescue","Allow companion rescue"));
		AddRow("bTapInteractions", LOCTEXT("Tap", "Tap instead of hold interactions"));
		AddRow("InteractionHoldScale", LOCTEXT("Hold", "Interaction hold duration multiplier"), .1f, 1, .1f);
		AddRow("bToggleAim", LOCTEXT("AimToggle", "Toggle aim")); AddRow("bToggleGuard", LOCTEXT("GuardToggle", "Toggle guard"));
		AddRow("bToggleSprint", LOCTEXT("SprintToggle", "Toggle sprint")); AddRow("bToggleAbilityModifier", LOCTEXT("ModifierToggle", "Toggle ability modifier"));
		AddRow("bAutomaticSprint", LOCTEXT("AutoSprint", "Automatic sprint")); AddRow("bAimSnap", LOCTEXT("AimSnap", "Aim snap")); AddRow("bProjectileLead", LOCTEXT("Lead", "Projectile lead assistance"));
		AddRow("AutoCameraStrength", LOCTEXT("AutoCamera", "Automatic camera strength"), 0, 1, .1f);
		AddRow("DefenseWindowScale", LOCTEXT("DefenseWindow", "Defense window assistance"), 1, 2, .1f);
		AddRow("ExertionCostScale", LOCTEXT("Exertion", "Exertion cost multiplier"), .25f, 1, .25f);
		AddRow("InputBufferAssistanceSeconds", LOCTEXT("Buffer", "Combat input buffer seconds"), 0, .2f, .05f);
		AddRow("MeleeAimAssistStrength", LOCTEXT("MeleeAssist", "Melee aim assistance"), 0, 1, .1f);
		AddRow("RangedAimAssistStrength", LOCTEXT("RangedAssist", "Ranged aim friction"), 0, 1, .1f);
		AddRow("bDisableCameraShake", LOCTEXT("Shake", "Disable camera shake")); AddRow("bReduceLensEffects", LOCTEXT("Lens", "Reduce lens effects")); AddRow("bReduceCorruptionEffects", LOCTEXT("Corruption", "Reduce corruption distortion"));
		AddRow("Haptic.Master", LOCTEXT("HapticMaster", "Vibration master"), 0, 1, .1f);
		AddRow("Haptic.Combat", LOCTEXT("HapticCombat", "Combat vibration"), 0, 1, .1f);
		AddRow("Haptic.Interaction", LOCTEXT("HapticInteraction", "Interaction vibration"), 0, 1, .1f);
		AddRow("Haptic.Cinematic", LOCTEXT("HapticCinematic", "Cinematic vibration"), 0, 1, .1f);
		AddRow("Haptic.Ambience", LOCTEXT("HapticAmbience", "Ambient vibration"), 0, 1, .1f);
		AddRow("Haptic.UI", LOCTEXT("HapticUI", "UI vibration"), 0, 1, .1f);
		AddRow("HDR.Enabled", LOCTEXT("HDREnabled", "HDR output preview enabled"));
		AddRow("HDR.Peak", LOCTEXT("HDRPeak", "HDR peak nits"), 400, 2000, 100);
		AddRow("HDR.Black", LOCTEXT("HDRBlack", "HDR black floor nits"), .000001f, 1, .01f);
		AddRow("HDR.Paper", LOCTEXT("HDRPaper", "HDR paper white nits"), 80, 500, 10);
		AddRow("HDR.UI", LOCTEXT("HDRUI", "HDR UI white nits"), 80, 500, 10);
		AddRow("HDR.Preview", LOCTEXT("HDRPreview", "Preview HDR for 15 seconds"));
		AddRow("HDR.Confirm", LOCTEXT("HDRConfirm", "Confirm HDR preview")); AddRow("HDR.Revert", LOCTEXT("HDRRevert", "Revert HDR preview"));
		AddRow("Cloud.Enabled", LOCTEXT("CloudEnable", "Optional cloud saves for this signed-in session"));
		AddRow("Cloud.Slot", LOCTEXT("CloudSlot", "Cloud review manual slot"), 0, 9, 1);
		AddRow("Cloud.Inspect", LOCTEXT("CloudInspect", "Review local and cloud copies"));
		AddRow("Cloud.KeepLocal", LOCTEXT("CloudKeep", "Publish reviewed local copy to cloud"));
		AddRow("Cloud.UseCloud", LOCTEXT("CloudUse", "Import reviewed cloud copy locally"));
		AddRow("Cloud.Cancel", LOCTEXT("CloudCancel", "Cancel cloud review"));
		AddRow("Continue", LOCTEXT("Continue", "Continue / close settings"));
	}
	RefreshRows(); return Super::RebuildWidget();
}
void USovAccessibilitySettingsMenu::NativeConstruct()
{
	const uint64 Expected=++MenuGeneration; BoundSettings = USovGameUserSettings::Get();
	if (BoundSettings) { BoundSettings->OnUserSettingsChanged.AddUniqueDynamic(this, &ThisClass::SettingsChanged); HDRDraft = BoundSettings->GetHDRCalibration(); }
	if (GetGameInstance()) { PlatformServices = GetGameInstance()->GetSubsystem<USovPlatformServicesSubsystem>(); if (PlatformServices) { PlatformServices->OnCloudReviewChanged.AddUniqueDynamic(this,&ThisClass::CloudChanged); } }
	Super::NativeConstruct(); if(Expected!=MenuGeneration) { return; }
	RefreshRows();
}
void USovAccessibilitySettingsMenu::NativeDestruct()
{
	const uint64 Expected=++MenuGeneration;
	USovGameUserSettings* OldSettings=BoundSettings; USovPlatformServicesSubsystem* OldPlatform=PlatformServices;
	const FGuid OldHDR=HDRReceipt,OldCloud=CloudRequest;
	if(OldSettings) { OldSettings->OnUserSettingsChanged.RemoveDynamic(this,&ThisClass::SettingsChanged); }
	if(OldPlatform) { OldPlatform->OnCloudReviewChanged.RemoveDynamic(this,&ThisClass::CloudChanged); }
	BoundSettings=nullptr; PlatformServices=nullptr; HDRReceipt.Invalidate(); CloudRequest.Invalidate();
	Super::NativeDestruct();
	if(IsValid(OldSettings) && OldHDR.IsValid()) { OldSettings->RevertHDRCalibration(OldHDR); }
	if(IsValid(OldPlatform) && OldCloud.IsValid() && OldPlatform->GetCloudReview().RequestId==OldCloud) { OldPlatform->CancelCloudOperation(); }
	if(Expected!=MenuGeneration || IsActivated()) { return; }
	if (GetOwningLocalPlayer()) { if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
}
void USovAccessibilitySettingsMenu::NativeOnDeactivated()
{
	const uint64 Expected=++MenuGeneration;
	Super::NativeOnDeactivated(); if(Expected!=MenuGeneration || IsActivated()) { return; }
	if (BoundSettings && HDRReceipt.IsValid()) { const FGuid Receipt=HDRReceipt; HDRReceipt.Invalidate(); BoundSettings->RevertHDRCalibration(Receipt); }
	if(Expected!=MenuGeneration || IsActivated()) { return; }
	if (PlatformServices && CloudRequest.IsValid() && PlatformServices->GetCloudReview().RequestId == CloudRequest) { CloudRequest.Invalidate(); PlatformServices->CancelCloudOperation(); }
	if(Expected!=MenuGeneration || IsActivated()) { return; }
	if (GetOwningLocalPlayer()) { if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
}
void USovAccessibilitySettingsMenu::NativeOnActivated()
{
	const uint64 Expected=++MenuGeneration; Super::NativeOnActivated();
	if(Expected!=MenuGeneration || !IsActivated() || !BoundSettings || HDRReceipt.IsValid()) { return; }
	const FSovHDROutputStatus Output=BoundSettings->GetHDROutputStatus();
	if(Expected!=MenuGeneration || !IsActivated() || !BoundSettings) { return; }
	bHDREnabled=Output.bEnabled; HDRPeakNits=Output.PeakNits>0 ? FMath::Clamp(Output.PeakNits,400,2000) : 1000;
	HDRDraft=BoundSettings->GetHDRCalibration(); RefreshRows();
}
void USovAccessibilitySettingsMenu::SettingsChanged(const FSovUserSettingsSnapshot& Value)
{
	const uint64 Expected=MenuGeneration;
	if (!Value.bMenuNarration && GetOwningLocalPlayer()) { if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>()) { Narrator->Cancel(this); } }
	if(Expected!=MenuGeneration) { return; }
	RefreshRows();
}
void USovAccessibilitySettingsMenu::CloudChanged(const FSovCloudReview& Review)
{
	RefreshRows(); if (!Status) { return; }
	const FText Missing = LOCTEXT("NoCopy","No copy");
	auto Describe = [&](bool bHas,const FSovSaveSlotHeader& Header)
	{ return bHas ? FText::Format(LOCTEXT("CopyMetadata","{0}; {1}; {2} minutes; generation {3}"),Header.MissionLabel,FText::AsDateTime(Header.TimestampUtc),FText::AsNumber(FMath::RoundToInt(Header.PlaySeconds/60.)),FText::AsNumber(Header.Generation)) : Missing; };
	Status->SetText(FText::Format(LOCTEXT("CloudReview","Cloud review: {0}\nLocal: {1}\nCloud: {2}\nSelect publish local, import cloud, or cancel. Neither copy is changed by review."),FText::FromString(Review.Message),Describe(Review.bHasLocal,Review.Local),Describe(Review.bHasCloud,Review.Cloud)));
}
void USovAccessibilitySettingsMenu::RefreshRows()
{
	const auto Value = CurrentSettings(); SetMenuNavigationWrap(Value.bMenuNavigationWrap);
	if (Status)
	{
		Status->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(22 * Value.UIScale)));
		Status->SetText(bFirstBoot ? LOCTEXT("FirstBoot", "Accessibility setup — settings save immediately. Activate or use left/right to change a value. Continue when ready.") : LOCTEXT("Instructions", "Accessibility — settings save immediately. Activate or use left/right to change a value."));
	}
	for (USovAccessibilitySettingRow* Row : Rows) { if (Row) { Row->Refresh(); } }
}
UWidget* USovAccessibilitySettingsMenu::NativeGetDesiredFocusTarget() const
{
	for (const USovAccessibilitySettingRow* Row : Rows) { if (Row && IsRowEnabled(Row)) { return Row->GetFocusTarget(); } }
	return nullptr;
}
void USovAccessibilitySettingsMenu::FocusRow(USovAccessibilitySettingRow* Row, const FText& Label)
{
	if (Scroll) { Scroll->ScrollWidgetIntoView(Row, false); }
	const uint64 Expected=MenuGeneration;
	if (IsActivated() && CurrentSettings().bMenuNarration && GetOwningLocalPlayer())
	{
		if (auto* Narrator = GetOwningLocalPlayer()->GetSubsystem<USovAccessibleNarrationSubsystem>())
		{ FGuid Request; if (!Narrator->Announce(this, Label, Request) && Status && Expected==MenuGeneration && IsActivated()) { Status->SetText(Narrator->GetUnavailableReason()); } }
	}
}
FText USovAccessibilitySettingsMenu::ValueText(const USovAccessibilitySettingRow* Row) const
{
	if (!Row) { return FText::GetEmpty(); }
	const FName Key = Row->SettingKey; const FString Name = Key.ToString();
	if (Name.StartsWith(TEXT("HDR.")) && BoundSettings && BoundSettings->IsDisplayOutputSystemManaged())
	{ return LOCTEXT("HDRSystemManaged", "Managed by console display settings"); }
	if(Name.StartsWith(TEXT("Audio.")) && BoundSettings)
	{
		if(Key=="Audio.DynamicRange") { static const FText Names[]={LOCTEXT("FullRange","Full"),LOCTEXT("ReducedRange","Reduced"),LOCTEXT("NightRange","Night")}; return Names[FMath::Clamp(int32(BoundSettings->GetAudioDynamicRange()),0,2)]; }
		const float Value=Key=="Audio.Overall" ? BoundSettings->GetOverallAudioVolume() : Key=="Audio.Dialogue" ? BoundSettings->GetDialogueAudioVolume() : Key=="Audio.Music" ? BoundSettings->GetMusicAudioVolume() : Key=="Audio.SFX" ? BoundSettings->GetSFXAudioVolume() : Key=="Audio.Ambience" ? BoundSettings->GetAmbienceAudioVolume() : BoundSettings->GetTinnitusAudioVolume();
		return FText::AsNumber(Value);
	}
	if (Key == "HDR.Preview" && BoundSettings && bHDREnabled && !BoundSettings->GetHDROutputStatus().bSupported) { return LOCTEXT("HDRUnavailable","Unavailable on current display"); }
	if (Name.StartsWith(TEXT("Cloud.")))
	{
		if (PlatformServices && PlatformServices->IsCloudManagedByPlatform()) { return LOCTEXT("CloudSystemManaged","Managed by platform save system"); }
		if (!PlatformServices || !PlatformServices->IsCloudAvailable()) { return LOCTEXT("CloudUnavailable","Unavailable: signed-in cloud provider and frontend required"); }
		if (Key == "Cloud.Enabled") { return PlatformServices->IsCloudEnabled() ? LOCTEXT("On","On") : LOCTEXT("Off","Off"); }
		if (Key == "Cloud.Slot") { return FText::AsNumber(CloudManualSlot + 1); }
		return LOCTEXT("Activate","activate");
	}
	if (Key == "Continue" || Name.StartsWith(TEXT("Review.")) || Key == "HDR.Preview" || Key == "HDR.Confirm" || Key == "HDR.Revert") { return LOCTEXT("Activate", "activate"); }
	FSovUserSettingsSnapshot Value = CurrentSettings();
	if(Key=="Difficulty") { static const FText Names[]={LOCTEXT("Story","Story"),LOCTEXT("Standard","Standard"),LOCTEXT("Veteran","Veteran"),LOCTEXT("Sovereign","Sovereign"),LOCTEXT("Custom","Custom")}; return Names[FMath::Clamp(int32(Value.Preset),0,4)]; }
	if (Key == "ColorVisionPreset") { static const FText Names[] = { LOCTEXT("DefaultPalette", "Default"), LOCTEXT("Deuteranopia", "Deuteranopia"), LOCTEXT("Protanopia", "Protanopia"), LOCTEXT("Tritanopia", "Tritanopia") }; return Names[FMath::Clamp(int32(Value.ColorVisionPreset), 0, 3)]; }
	if (Key == "DialoguePressureMode") { static const FText Names[] = { LOCTEXT("StandardPressure", "Standard"), LOCTEXT("ExtendedPressure", "Extended"), LOCTEXT("DisabledPressure", "Disabled") }; return Names[FMath::Clamp(int32(Value.DialoguePressureMode), 0, 2)]; }
	if (float* Channel = ColorChannel(Value, Key)) { return FText::AsNumber(*Channel); }
	if (Name.StartsWith(TEXT("Haptic."))) { FSovHapticSettings Haptic = BoundSettings ? BoundSettings->GetHapticSettings() : FSovHapticSettings(); return FText::AsNumber(ReadNumber(FSovHapticSettings::StaticStruct()->FindPropertyByName(FName(*Name.RightChop(7))), &Haptic)); }
	if (Key == "HDR.Enabled") { return bHDREnabled ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off"); }
	if (Key == "HDR.Peak") { return FText::AsNumber(HDRPeakNits); }
	if (Key == "HDR.Black") { FNumberFormattingOptions Format; Format.MaximumFractionalDigits=6; return FText::AsNumber(HDRDraft.BlackFloorNits,&Format); }
	if (Key == "HDR.Paper") { return FText::AsNumber(HDRDraft.PaperWhiteNits); } if (Key == "HDR.UI") { return FText::AsNumber(HDRDraft.UIWhiteNits); }
	FProperty* Property = FSovUserSettingsSnapshot::StaticStruct()->FindPropertyByName(Key);
	const double Number = ReadNumber(Property, &Value);
	return CastField<FBoolProperty>(Property) ? (Number != 0 ? LOCTEXT("On", "On") : LOCTEXT("Off", "Off")) : FText::AsNumber(Number);
}
bool USovAccessibilitySettingsMenu::IsRowEnabled(const USovAccessibilitySettingRow* Row) const
{
	if (!Row) { return false; } const FName Key = Row->SettingKey;
	if (Key.ToString().StartsWith(TEXT("HDR.")))
	{
		if (!BoundSettings || BoundSettings->IsDisplayOutputSystemManaged()) { return false; }
		if (Key == "HDR.Preview")
		{
			const auto Output = BoundSettings->GetHDROutputStatus();
			return Output.bCanPreviewInGame && (!bHDREnabled || Output.bCanCalibrateInGame);
		}
		if (Key == "HDR.Confirm" || Key == "HDR.Revert") { return HDRReceipt.IsValid(); }
	}
	if (!Key.ToString().StartsWith(TEXT("Cloud."))) { return true; }
	if (!PlatformServices || !PlatformServices->IsCloudAvailable()) { return false; }
	if (Key == "Cloud.Enabled") { return true; }
	if (!PlatformServices->IsCloudEnabled()) { return false; }
	const auto Review = PlatformServices->GetCloudReview();
	if (Key == "Cloud.KeepLocal" || Key == "Cloud.UseCloud")
	{ return CloudRequest.IsValid() && Review.RequestId == CloudRequest && Review.Phase == ESovCloudPhase::AwaitingChoice && (Key == "Cloud.KeepLocal" ? Review.bHasLocal : Review.bHasCloud); }
	return true;
}
bool USovAccessibilitySettingsMenu::CanAdjustValue(const USovAccessibilitySettingRow* Row) const
{
	if (!Row || !IsRowEnabled(Row)) { return false; }
	const FName Key = Row->SettingKey; const FString Name = Key.ToString();
	return Key != "Continue" && !Name.StartsWith(TEXT("Review.")) &&
		Key != "HDR.Preview" && Key != "HDR.Confirm" && Key != "HDR.Revert" &&
		(!Name.StartsWith(TEXT("Cloud.")) || Key == "Cloud.Enabled" || Key == "Cloud.Slot");
}
void USovAccessibilitySettingsMenu::Adjust(USovAccessibilitySettingRow* Row, int32 Direction)
{
	if (!Row || !Rows.Contains(Row) || !BoundSettings || !IsRowEnabled(Row)) { return; }
	const uint64 Expected=MenuGeneration; const bool bWasActive=IsActivated();
	const auto IsCurrent=[&]() { return Expected==MenuGeneration && (!bWasActive || IsActivated()); };
	const FName Key = Row->SettingKey; const FString Name = Key.ToString(); FString Error;
	if (Key == "Continue") { if ((!bFirstBoot || BoundSettings->CompleteAccessibilitySetup()) && IsCurrent()) { DeactivateWidget(); } return; }
	if(Key=="Difficulty")
	{
		BoundSettings->ApplyDifficultyPreset(static_cast<ESovDifficultyPreset>(int32(ChangedValue(int32(BoundSettings->GetSettingsSnapshot().Preset),Row,Direction))),Error);
		if(!IsCurrent()) { return; } RefreshRows(); if(!Error.IsEmpty() && Status) { Status->SetText(FText::FromString(Error)); } FocusRow(Row,Row->GetLabel()); return;
	}
	if(Name.StartsWith(TEXT("Review.")))
	{
		if(auto* PC=Cast<ANarrativePlayerController>(GetOwningPlayer()))
		{ if(auto* HUD=PC->GetNarrativeGameplayHUD()) { if(auto* Review=Cast<USovAccessibleRecordMenu>(HUD->OpenMenu(USovAccessibleRecordMenu::StaticClass(),FNarrativeGameplayTags::Get().UI_Layer_Modal))) { Review->SetSceneHistoryMode(Key=="Review.History"); } } }
		return;
	}
	if(Name.StartsWith(TEXT("Audio.")))
	{
		if(Key=="Audio.DynamicRange")
		{
			const auto Value=static_cast<ENarrativeAudioDynamicRange>(int32(ChangedValue(int32(BoundSettings->GetAudioDynamicRange()),Row,Direction)));
			if(BoundSettings->IsAudioDynamicRangeAvailable(Value)) { BoundSettings->SetAudioDynamicRange(Value); }
			else { Error=TEXT("This dynamic-range mix has not been supplied for the current audio configuration."); }
		}
		else
		{
			const float Current=Key=="Audio.Overall" ? BoundSettings->GetOverallAudioVolume() : Key=="Audio.Dialogue" ? BoundSettings->GetDialogueAudioVolume() : Key=="Audio.Music" ? BoundSettings->GetMusicAudioVolume() : Key=="Audio.SFX" ? BoundSettings->GetSFXAudioVolume() : Key=="Audio.Ambience" ? BoundSettings->GetAmbienceAudioVolume() : BoundSettings->GetTinnitusAudioVolume();
			const float Value=ChangedValue(Current,Row,Direction);
			if(Key=="Audio.Overall") { BoundSettings->SetOverallAudioVolume(Value); } else if(Key=="Audio.Dialogue") { BoundSettings->SetDialogueAudioVolume(Value); }
			else if(Key=="Audio.Music") { BoundSettings->SetMusicAudioVolume(Value); } else if(Key=="Audio.SFX") { BoundSettings->SetSFXAudioVolume(Value); }
			else if(Key=="Audio.Ambience") { BoundSettings->SetAmbienceAudioVolume(Value); } else if(Key=="Audio.Tinnitus") { BoundSettings->SetTinnitusAudioVolume(Value); }
		}
		if(!IsCurrent()) { return; } RefreshRows(); if(!Error.IsEmpty() && Status) { Status->SetText(FText::FromString(Error)); } FocusRow(Row,Row->GetLabel()); return;
	}
	if (Name.StartsWith(TEXT("Cloud.")) && PlatformServices)
	{
		if (Key == "Cloud.Enabled") { PlatformServices->SetCloudEnabled(!PlatformServices->IsCloudEnabled(),Error); }
		else if (Key == "Cloud.Slot") { CloudManualSlot = int32(ChangedValue(CloudManualSlot,Row,Direction)); }
		else if (Key == "Cloud.Inspect") { if (PlatformServices->InspectCloudSlot(ESovSaveSlotKind::Manual,CloudManualSlot,Error) && IsCurrent() && PlatformServices) { CloudRequest = PlatformServices->GetCloudReview().RequestId; } }
		else if (Key == "Cloud.KeepLocal" || Key == "Cloud.UseCloud") { PlatformServices->ResolveCloudReview(CloudRequest,Key == "Cloud.KeepLocal" ? ESovCloudChoice::KeepLocal : ESovCloudChoice::UseCloud,Error); }
		else if (Key == "Cloud.Cancel" && PlatformServices->GetCloudReview().RequestId == CloudRequest) { CloudRequest.Invalidate(); PlatformServices->CancelCloudOperation(); }
		if(!IsCurrent() || !PlatformServices) { return; } CloudChanged(PlatformServices->GetCloudReview()); if (!Error.IsEmpty() && Status) { Status->SetText(FText::FromString(Error)); } return;
	}
	if (Key == "HDR.Preview")
	{
		if (HDRReceipt.IsValid()) { Error = TEXT("Confirm or revert the previous preview before starting another."); }
		else
		{
			FGuid Receipt; USovGameUserSettings* Owner=BoundSettings;
			const bool bStarted = bHDREnabled
				? Owner->PreviewHDRDisplay(true,HDRPeakNits,HDRDraft,Receipt,Error)
				: Owner->PreviewHDRCalibration(false,HDRPeakNits,Receipt,Error);
			if(bStarted)
			{ if(IsCurrent()) { HDRReceipt=Receipt; } else { Owner->RevertHDRCalibration(Receipt); } }
		}
	}
	else if (Key == "HDR.Confirm") { const FGuid Receipt=HDRReceipt; HDRReceipt.Invalidate(); if (!BoundSettings->ConfirmHDRCalibration(Receipt, Error) && IsCurrent() && BoundSettings) { BoundSettings->RevertHDRCalibration(Receipt); } }
	else if (Key == "HDR.Revert") { const FGuid Receipt=HDRReceipt; HDRReceipt.Invalidate(); BoundSettings->RevertHDRCalibration(Receipt); }
	else if (Key == "HDR.Enabled") { bHDREnabled = !bHDREnabled; }
	else if (Key == "HDR.Peak") { HDRPeakNits = int32(ChangedValue(HDRPeakNits, Row, Direction)); }
	else if (Key == "HDR.Black")
	{
		const float Log = FMath::LogX(10.f,FMath::Max(.000001f,HDRDraft.BlackFloorNits));
		int32 Exponent=FMath::RoundToInt(Log)+Direction; if(Exponent>0) { Exponent=-6; } else if(Exponent < -6) { Exponent=0; }
		HDRDraft.BlackFloorNits=FMath::Pow(10.f,float(Exponent));
	}
	else if (Key == "HDR.Paper") { HDRDraft.PaperWhiteNits = ChangedValue(HDRDraft.PaperWhiteNits, Row, Direction); }
	else if (Key == "HDR.UI") { HDRDraft.UIWhiteNits = ChangedValue(HDRDraft.UIWhiteNits, Row, Direction); }
	else if (Name.StartsWith(TEXT("Haptic.")))
	{
		FSovHapticSettings Value = BoundSettings->GetHapticSettings(); FProperty* Property = FSovHapticSettings::StaticStruct()->FindPropertyByName(FName(*Name.RightChop(7)));
		WriteNumber(Property, &Value, ChangedValue(ReadNumber(Property, &Value), Row, Direction)); BoundSettings->ApplyHapticSettings(Value, Error);
	}
	else
	{
		FSovUserSettingsSnapshot Value = BoundSettings->GetSettingsSnapshot();
		if(Key=="IncomingDamageScale" || Key=="EnemyRecoveryScale" || Key=="bAllowCompanionRescue") { Value.Preset=ESovDifficultyPreset::Custom; }
		if (float* Channel = ColorChannel(Value, Key)) { *Channel = ChangedValue(*Channel, Row, Direction); }
		else { FProperty* Property = FSovUserSettingsSnapshot::StaticStruct()->FindPropertyByName(Key); if (!Property) { return; } WriteNumber(Property, &Value, ChangedValue(ReadNumber(Property, &Value), Row, Direction)); }
		BoundSettings->ApplySettingsSnapshot(Value, Error);
	}
	if(!IsCurrent()) { return; } RefreshRows(); if (!Error.IsEmpty() && Status) { Status->SetText(FText::FromString(Error)); }
	FocusRow(Row, Row->GetLabel());
}
#undef LOCTEXT_NAMESPACE
