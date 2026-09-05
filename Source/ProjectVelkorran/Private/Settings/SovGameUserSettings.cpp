// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Settings/SovGameUserSettings.h"
#include "Settings/SovSettingsPolicy.h"
#include "UI/SovAccessibilityPolicy.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Engine/Engine.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

FString SovCurrentDisplayIdentity();

namespace
{
	bool ReadPortable(const TArray<uint8>& Data, FSovUserSettingsSnapshot& Out, FString& Error)
	{
		// Fixed schema: version, preset, two float32 scalars and one bool byte. No strings or allocations from input.
		if (Data.Num() != 11) { Error = TEXT("Invalid portable settings size."); return false; }
		FMemoryReader Reader(Data, true);
		uint8 Version = 0, Preset = 0, Rescue = 0;
		Reader << Version << Preset << Out.IncomingDamageScale << Out.EnemyRecoveryScale << Rescue;
		Out.Preset = static_cast<ESovDifficultyPreset>(Preset);
		Out.bAllowCompanionRescue = Rescue != 0;
		if (Reader.IsError() || Version != SovSettingsPolicy::Schema || Rescue > 1
			|| !SovSettingsPolicy::ValidGameplay(Preset, Out.IncomingDamageScale, Out.EnemyRecoveryScale, true))
		{ Error = TEXT("Invalid portable settings schema or values."); return false; }
		Error.Reset(); return true;
	}
}
USovGameUserSettings* USovGameUserSettings::Get()
{
	return GEngine ? Cast<USovGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
bool USovGameUserSettings::ValidateSnapshot(const FSovUserSettingsSnapshot& Value, bool bUnlocked, FString& Error)
{
	if (!SovSettingsPolicy::ValidGameplay(static_cast<uint8>(Value.Preset), Value.IncomingDamageScale, Value.EnemyRecoveryScale, bUnlocked)
		|| !SovSettingsPolicy::ValidAssists(Value.DefenseWindowScale, Value.ExertionCostScale,
			Value.InputBufferAssistanceSeconds, Value.MeleeAimAssistStrength, Value.RangedAimAssistStrength)
		|| !SovSettingsPolicy::InRange(Value.AutoCameraStrength, 0.f, 1.f)
		|| !SovSettingsPolicy::InRange(Value.InteractionHoldScale, .1f, 1.f)
		|| !SovAccessibilityPolicy::ValidLayout(Value.UIScale, Value.SubtitleScale, Value.SubtitleBackgroundOpacity,
			Value.SubtitleCharactersPerLine, Value.SubtitleMaximumLines, Value.OutlineThickness)
		|| static_cast<uint8>(Value.ColorVisionPreset) > 3
		|| !SovAccessibilityPolicy::ValidColor(Value.TeamColor.R, Value.TeamColor.G, Value.TeamColor.B, Value.TeamColor.A)
		|| !SovAccessibilityPolicy::ValidColor(Value.ThreatColor.R, Value.ThreatColor.G, Value.ThreatColor.B, Value.ThreatColor.A)
		|| !SovAccessibilityPolicy::ValidPressure(static_cast<uint8>(Value.DialoguePressureMode),
			Value.DialogueMinimumReadSeconds, Value.DialoguePressureExtension)
		|| !SovAccessibilityPolicy::Range(Value.ControllerAudioVolume,0.f,1.f))
	{ Error = TEXT("Settings contain unsupported values or locked Sovereign difficulty."); return false; }
	Error.Reset(); return true;
}
void USovGameUserSettings::LoadSettings(bool bForceReload)
{
	if (bHDRTransaction) { return; }
	if (HDRPreviewReceipt.IsValid()) { RevertHDRCalibration(HDRPreviewReceipt); }
	Super::LoadSettings(bForceReload);
	if (!DisplayCalibration.IsValid()) { DisplayCalibration = FSovHDRCalibration(); bHasDisplayCalibration = false; }
	if (!HapticSettings.IsValid()) { HapticSettings = FSovHapticSettings(); HapticSettings.Master = 0.f; SaveSettings(); }
	FString Error;
	if (SettingsSchemaVersion != 1 || !ValidateSnapshot(Settings, bCampaignCompleted, Error))
	{
		Settings = FSovUserSettingsSnapshot(); SettingsSchemaVersion = 1;
		// Bad/unknown config never enables diagnostic collection.
		bLocalDiagnosticsEnabled = false;
		bAccessibilitySetupCompleted = false;
		SaveSettings();
	}
}

void USovGameUserSettings::SaveSettings()
{
	TGuardValue<bool> Guard(bHDRTransaction, true);
	// Narrative audio/input widgets save immediately. Never let an unrelated save commit an HDR preview.
	if (HDRPreviewReceipt.IsValid())
	{
		TGuardValue<bool> RestoreEnabled(bUseHDRDisplayOutput, bHDRBeforePreview);
		TGuardValue<int32> RestoreNits(HDRDisplayOutputNits, HDRNitsBeforePreview);
		TGuardValue<FSovHDRCalibration> RestoreCalibration(DisplayCalibration, BeforeDisplayCalibration);
		PersistSettings();
		return;
	}
	PersistSettings();
}
void USovGameUserSettings::PersistSettings() { Super::SaveSettings(); }
bool USovGameUserSettings::CompleteAccessibilitySetup()
{
	if (bApplying) { return false; }
	if (bAccessibilitySetupCompleted) { return true; }
	TGuardValue<bool> Guard(bApplying, true);
	bAccessibilitySetupCompleted = true;
	SaveSettings();
	OnUserSettingsChanged.Broadcast(Settings);
	return true;
}
void USovGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	if (bHDRTransaction) { return; }
	if (HDRPreviewReceipt.IsValid()) { RevertHDRCalibration(HDRPreviewReceipt); }
	TGuardValue<bool> Guard(bHDRTransaction, true);
	const bool bRequestedHDR = bUseHDRDisplayOutput;
	const int32 RequestedNits = HDRDisplayOutputNits;
	Super::ApplySettings(bCheckForCommandLineOverrides);
	// Narrative moves the window after UGameUserSettings applies resolution. Query/reapply on the final monitor.
	if (CanApplyHDROutput()) { WriteHDROutput(bRequestedHDR && ReadHDROutput().bSupported, RequestedNits); ApplyConfirmedDisplayCalibration(); SaveSettings(); }
}
bool USovGameUserSettings::ApplyHapticSettings(const FSovHapticSettings& Value, FString& Error)
{
	if (bApplying || !Value.IsValid()) { Error = TEXT("Feedback settings are invalid or a settings transaction is active."); return false; }
	TGuardValue<bool> Guard(bApplying, true);
	HapticSettings = Value;
	SaveSettings();
	OnHapticSettingsChanged.Broadcast(HapticSettings);
	Error.Reset(); return true;
}
bool USovGameUserSettings::CanApplyHDROutput() const { return GEngine && !IsRunningCommandlet() && FApp::CanEverRender(); }
double USovGameUserSettings::HDRTime() const { return FPlatformTime::Seconds(); }
FSovHDROutputStatus USovGameUserSettings::ReadHDROutput()
{
	FSovHDROutputStatus Result;
	if (!CanApplyHDROutput()) { return Result; }
	Result.bSupported = SupportsHDRDisplayOutput();
	Result.bEnabled = Result.bSupported && IsHDREnabled();
	Result.PeakNits = Result.bEnabled ? GetCurrentHDRDisplayNits() : 0;
	Result.DisplayIdentity = SovCurrentDisplayIdentity();
	return Result;
}
void USovGameUserSettings::WriteHDROutput(bool bEnable, int32 PeakNits)
{
	if (CanApplyHDROutput()) { EnableHDRDisplayOutput(bEnable, PeakNits); }
}
FSovHDROutputStatus USovGameUserSettings::GetHDROutputStatus() { return ReadHDROutput(); }
bool USovGameUserSettings::PreviewHDRCalibration(bool bEnable, int32 PeakNits, FGuid& Receipt, FString& Error)
{
	return BeginHDRPreview(bEnable, PeakNits, nullptr, Receipt, Error);
}
bool USovGameUserSettings::PreviewHDRDisplay(bool bEnable, int32 PeakNits, const FSovHDRCalibration& Calibration, FGuid& Receipt, FString& Error)
{
	return BeginHDRPreview(bEnable, PeakNits, &Calibration, Receipt, Error);
}
bool USovGameUserSettings::BeginHDRPreview(bool bEnable, int32 PeakNits, const FSovHDRCalibration* Calibration, FGuid& Receipt, FString& Error)
{
	Receipt.Invalidate();
	if (bHDRTransaction || HDRPreviewReceipt.IsValid() || !CanApplyHDROutput() || PeakNits < 400 || PeakNits > 2000)
	{ Error = TEXT("HDR preview is unavailable, already active, or outside the supported 400-2000 nit request range."); return false; }
	const FSovHDROutputStatus Before = ReadHDROutput();
	if (Before.DisplayIdentity.IsEmpty())
	{ Error = TEXT("The viewport's physical display cannot be identified unambiguously; move it fully onto one display before calibration."); return false; }
	if (bEnable && !Before.bSupported) { Error = TEXT("The current platform/display does not support HDR output."); return false; }
	if (Calibration && (!bEnable || !Calibration->IsValid() || !CanApplyDisplayCalibration()
		|| !ReadDisplayCalibration(BeforeRenderCalibration)))
	{ Error = TEXT("Full calibration requires HDR and writable scene black/gray and HDR UI compositor controls on this renderer."); return false; }
	if (Calibration)
	{
		PreviewUIBaseNits = DisplayUIBaseNits();
		if (!FMath::IsFinite(PreviewUIBaseNits) || PreviewUIBaseNits <= 0.f)
		{ Error = TEXT("The HDR UI luminance reference is unavailable."); return false; }
		BeforeRenderUILevel = BeforeRenderCalibration.UIWhiteNits / PreviewUIBaseNits;
		PreviewUILevel = Calibration->UIWhiteNits / PreviewUIBaseNits;
	}
	TGuardValue<bool> Guard(bHDRTransaction, true);
	bHDRBeforePreview = bUseHDRDisplayOutput;
	HDRNitsBeforePreview = HDRDisplayOutputNits;
	BeforeDisplayCalibration = DisplayCalibration;
	bHadDisplayCalibration = bHasDisplayCalibration;
	bPreviewOwnsCalibration = Calibration != nullptr;
	if (Calibration) { PreviewDisplayCalibration = *Calibration; }
	HDRPreviewDisplayIdentity = Before.DisplayIdentity;
	HDRPreviewReceipt = FGuid::NewGuid(); // Protect unrelated saves during the engine call as well.
	WriteHDROutput(bEnable, PeakNits);
	const bool bCalibrationApplied = !Calibration || (FMath::IsNearlyEqual(DisplayUIBaseNits(), PreviewUIBaseNits, .01f) && WriteDisplayCalibration(*Calibration));
	HDRPreviewOutput = ReadHDROutput();
	FSovHDRCalibration Applied;
	if (!CanApplyHDROutput() || !bCalibrationApplied || HDRPreviewOutput.DisplayIdentity != HDRPreviewDisplayIdentity
		|| (Calibration && (!CanApplyDisplayCalibration() || !FMath::IsNearlyEqual(DisplayUIBaseNits(), PreviewUIBaseNits, .01f)
			|| !ReadDisplayCalibration(Applied) || !Applied.Equals(*Calibration)))
		|| HDRPreviewOutput.bSupported != Before.bSupported || HDRPreviewOutput.bEnabled != bEnable
		|| (bEnable && (!HDRPreviewOutput.bSupported || HDRPreviewOutput.PeakNits <= 0)))
	{
		WriteHDROutput(Before.bEnabled && ReadHDROutput().bSupported, Before.PeakNits > 0 ? Before.PeakNits : 1000);
		if (Calibration) { RestorePreviewDisplayCalibration(); }
		// Physical rollback can be unavailable after display/device loss. Never leave the rejected preview in config.
		bUseHDRDisplayOutput = bHDRBeforePreview;
		HDRDisplayOutputNits = HDRNitsBeforePreview;
		DisplayCalibration = BeforeDisplayCalibration; bHasDisplayCalibration = bHadDisplayCalibration;
		bPreviewOwnsCalibration = false;
		HDRPreviewReceipt.Invalidate();
		Error = TEXT("The engine could not apply the requested HDR output; the previous available output was restored."); return false;
	}
	if (Calibration) { DisplayCalibration = Applied; PreviewDisplayCalibration = Applied; }
	HDRPreviewDeadline = HDRTime() + 15.;
	BeginDisplayObservation();
	HDRPreviewTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickHDRPreview), .25f);
	Receipt = HDRPreviewReceipt;
	Error.Reset(); return true;
}
void USovGameUserSettings::RemoveHDRPreviewTicker()
{
	if (HDRPreviewTicker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(HDRPreviewTicker); HDRPreviewTicker.Reset(); }
}
bool USovGameUserSettings::ConfirmHDRCalibration(FGuid Receipt, FString& Error)
{
	if (bHDRTransaction || !Receipt.IsValid() || Receipt != HDRPreviewReceipt)
	{ Error = TEXT("HDR preview receipt is stale or unavailable."); return false; }
	const FSovHDROutputStatus Current = ReadHDROutput();
	FSovHDRCalibration CurrentCalibration;
	if (!CanApplyHDROutput() || HDRTime() >= HDRPreviewDeadline || bDisplayMetricsInvalidated
		|| Current.DisplayIdentity.IsEmpty() || Current.DisplayIdentity != HDRPreviewDisplayIdentity
		|| (bPreviewOwnsCalibration && (!CanApplyDisplayCalibration() || !FMath::IsNearlyEqual(DisplayUIBaseNits(), PreviewUIBaseNits, .01f)
			|| !ReadDisplayCalibration(CurrentCalibration) || !CurrentCalibration.Equals(PreviewDisplayCalibration)))
		|| Current.bEnabled != HDRPreviewOutput.bEnabled
		|| Current.PeakNits != HDRPreviewOutput.PeakNits || Current.bSupported != HDRPreviewOutput.bSupported)
	{ RevertHDRCalibration(Receipt); Error = TEXT("HDR preview expired or display output changed; preview reverted."); return false; }
	TGuardValue<bool> Guard(bHDRTransaction, true);
	if (bPreviewOwnsCalibration) { bHasDisplayCalibration = true; }
	bPreviewOwnsCalibration = false;
	HDRPreviewReceipt.Invalidate(); RemoveHDRPreviewTicker(); EndDisplayObservation();
	SaveSettings();
	Error.Reset(); return true;
}
bool USovGameUserSettings::RevertHDRCalibration(FGuid Receipt)
{
	if (bHDRTransaction || !Receipt.IsValid() || Receipt != HDRPreviewReceipt) { return false; }
	TGuardValue<bool> Guard(bHDRTransaction, true);
	RemoveHDRPreviewTicker(); EndDisplayObservation();
	// Keep save masking active during the engine call. The transaction guard rejects every reentrant receipt action.
	WriteHDROutput(bHDRBeforePreview && ReadHDROutput().bSupported, HDRNitsBeforePreview);
	if (bPreviewOwnsCalibration) { RestorePreviewDisplayCalibration(); }
	// Confirmed config must recover even when rendering/output is unavailable and WriteHDROutput does nothing.
	bUseHDRDisplayOutput = bHDRBeforePreview;
	HDRDisplayOutputNits = HDRNitsBeforePreview;
	DisplayCalibration = BeforeDisplayCalibration; bHasDisplayCalibration = bHadDisplayCalibration;
	bPreviewOwnsCalibration = false;
	HDRPreviewReceipt.Invalidate();
	return true;
}
bool USovGameUserSettings::TickHDRPreview(float DeltaTime)
{
	if (!HDRPreviewReceipt.IsValid()) { return false; }
	const FSovHDROutputStatus Current = ReadHDROutput();
	FSovHDRCalibration CurrentCalibration;
	if (HDRTime() >= HDRPreviewDeadline || !CanApplyHDROutput() || bDisplayMetricsInvalidated
		|| Current.DisplayIdentity.IsEmpty() || Current.DisplayIdentity != HDRPreviewDisplayIdentity
		|| (bPreviewOwnsCalibration && (!CanApplyDisplayCalibration() || !FMath::IsNearlyEqual(DisplayUIBaseNits(), PreviewUIBaseNits, .01f)
			|| !ReadDisplayCalibration(CurrentCalibration) || !CurrentCalibration.Equals(PreviewDisplayCalibration)))
		|| Current.bEnabled != HDRPreviewOutput.bEnabled || Current.PeakNits != HDRPreviewOutput.PeakNits
		|| Current.bSupported != HDRPreviewOutput.bSupported)
	{ HDRPreviewTicker.Reset(); RevertHDRCalibration(HDRPreviewReceipt); return false; }
	return true;
}
void USovGameUserSettings::BeginDestroy()
{
	if (HDRPreviewReceipt.IsValid()) { RevertHDRCalibration(HDRPreviewReceipt); }
	RemoveHDRPreviewTicker();
	EndDisplayObservation();
	Super::BeginDestroy();
}
bool USovGameUserSettings::ApplySettingsSnapshot(const FSovUserSettingsSnapshot& NewSettings, FString& Error)
{
	if (bApplying) { Error = TEXT("A settings transaction is already active."); return false; }
	if (!ValidateSnapshot(NewSettings, bCampaignCompleted, Error)) { return false; }
	TGuardValue<bool> Guard(bApplying, true);
	Settings = NewSettings;
	SettingsSchemaVersion = 1;
	// Keep existing Narrative widgets in sync without owning an independent difficulty variable.
	GameplayDifficulty = Settings.Preset == ESovDifficultyPreset::Story ? ENarrativeGameplayDifficulty::Easy
		: Settings.Preset == ESovDifficultyPreset::Veteran ? ENarrativeGameplayDifficulty::Hard
		: Settings.Preset == ESovDifficultyPreset::Sovereign ? ENarrativeGameplayDifficulty::Insane : ENarrativeGameplayDifficulty::Medium;
	SaveSettings();
	OnUserSettingsChanged.Broadcast(Settings);
	return true;
}
bool USovGameUserSettings::ApplyDifficultyPreset(ESovDifficultyPreset Preset, FString& Error)
{
	FSovUserSettingsSnapshot New = Settings;
	New.Preset = Preset;
	switch (Preset)
	{
	case ESovDifficultyPreset::Story:
		New.IncomingDamageScale = .65f; New.EnemyRecoveryScale = 1.4f; New.bAllowCompanionRescue = true;
		New.DefenseWindowScale = FMath::Max(New.DefenseWindowScale, 1.5f);
		New.MeleeAimAssistStrength = FMath::Max(New.MeleeAimAssistStrength, .5f);
		New.RangedAimAssistStrength = FMath::Max(New.RangedAimAssistStrength, .5f);
		break;
	case ESovDifficultyPreset::Standard: New.IncomingDamageScale = 1.f; New.EnemyRecoveryScale = 1.f; New.bAllowCompanionRescue = true; break;
	case ESovDifficultyPreset::Veteran: New.IncomingDamageScale = 1.2f; New.EnemyRecoveryScale = .85f; New.bAllowCompanionRescue = false; break;
	case ESovDifficultyPreset::Sovereign: New.IncomingDamageScale = 1.35f; New.EnemyRecoveryScale = .7f; New.bAllowCompanionRescue = false; break;
	case ESovDifficultyPreset::Custom: break;
	default: Error = TEXT("Unknown difficulty preset."); return false;
	}
	return ApplySettingsSnapshot(New, Error);
}
FName USovGameUserSettings::GetDifficultyId() const
{
	switch (Settings.Preset)
	{
	case ESovDifficultyPreset::Story: return TEXT("Story");
	case ESovDifficultyPreset::Veteran: return TEXT("Veteran");
	case ESovDifficultyPreset::Sovereign: return TEXT("Sovereign");
	case ESovDifficultyPreset::Custom: return TEXT("Custom");
	default: return TEXT("Standard");
	}
}
bool USovGameUserSettings::UnlockSovereignFromCampaign(const USovCampaignStateComponent* Campaign)
{
	if (!IsValid(Campaign) || !Campaign->IsStateValid() || !IsValid(Campaign->GetActiveMission())
		|| !Campaign->GetActiveMission()->bCompletesCampaign
		|| !Campaign->IsMissionComplete(Campaign->GetActiveMission()->MissionId)) { return false; }
	if (bCampaignCompleted) { return true; }
	if (bApplying) { return false; }
	TGuardValue<bool> Guard(bApplying, true);
	bCampaignCompleted = true; SaveSettings(); OnUserSettingsChanged.Broadcast(Settings); return true;
}
void USovGameUserSettings::SetLocalDiagnosticsEnabled(bool bEnabled)
{
	if (bApplying || bLocalDiagnosticsEnabled == bEnabled) { return; }
	TGuardValue<bool> Guard(bApplying, true);
	bLocalDiagnosticsEnabled = bEnabled; SaveSettings();
	OnUserSettingsChanged.Broadcast(Settings);
}
bool USovGameUserSettings::CapturePortableSettings(TArray<uint8>& OutData) const
{
	FString Error;
	if (!ValidateSnapshot(Settings, bCampaignCompleted, Error)) { return false; }
	TArray<uint8> Data;
	FMemoryWriter Writer(Data, true);
	uint8 Version = SovSettingsPolicy::Schema, Preset = static_cast<uint8>(Settings.Preset), Rescue = Settings.bAllowCompanionRescue ? 1 : 0;
	float Damage = Settings.IncomingDamageScale, Recovery = Settings.EnemyRecoveryScale;
	Writer << Version << Preset << Damage << Recovery << Rescue;
	if (Writer.IsError()) { return false; }
	OutData = MoveTemp(Data); return true;
}
bool USovGameUserSettings::ValidatePortableSettings(const TArray<uint8>& Data, FString& Error)
{
	FSovUserSettingsSnapshot Candidate;
	return ReadPortable(Data, Candidate, Error);
}
bool USovGameUserSettings::RestorePortableSettings(const TArray<uint8>& Data, FString& Error)
{
	FSovUserSettingsSnapshot Portable;
	if (!ReadPortable(Data, Portable, Error)) { return false; }
	FSovUserSettingsSnapshot Candidate = Settings;
	Candidate.Preset = Portable.Preset;
	Candidate.IncomingDamageScale = Portable.IncomingDamageScale;
	Candidate.EnemyRecoveryScale = Portable.EnemyRecoveryScale;
	Candidate.bAllowCompanionRescue = Portable.bAllowCompanionRescue;
	return ApplySettingsSnapshot(Candidate, Error);
}

void USovGameUserSettings::SetGameplayDifficulty(const ENarrativeGameplayDifficulty NewDifficulty)
{
	FString Error;
	switch (NewDifficulty)
	{
	case ENarrativeGameplayDifficulty::Easy: ApplyDifficultyPreset(ESovDifficultyPreset::Story, Error); break;
	case ENarrativeGameplayDifficulty::Medium: ApplyDifficultyPreset(ESovDifficultyPreset::Standard, Error); break;
	case ENarrativeGameplayDifficulty::Hard: ApplyDifficultyPreset(ESovDifficultyPreset::Veteran, Error); break;
	case ENarrativeGameplayDifficulty::Insane: ApplyDifficultyPreset(ESovDifficultyPreset::Sovereign, Error); break;
	default: break;
	}
}
