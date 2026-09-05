// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Settings/SovGameUserSettings.h"
#include "Settings/SovSettingsPolicy.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Engine/Engine.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

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
		|| !SovSettingsPolicy::InRange(Value.InteractionHoldScale, .1f, 1.f))
	{ Error = TEXT("Settings contain unsupported values or locked Sovereign difficulty."); return false; }
	Error.Reset(); return true;
}
void USovGameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	FString Error;
	if (SettingsSchemaVersion != 1 || !ValidateSnapshot(Settings, bCampaignCompleted, Error))
	{
		Settings = FSovUserSettingsSnapshot(); SettingsSchemaVersion = 1;
		// Bad/unknown config never enables diagnostic collection.
		bLocalDiagnosticsEnabled = false;
		SaveSettings();
	}
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
