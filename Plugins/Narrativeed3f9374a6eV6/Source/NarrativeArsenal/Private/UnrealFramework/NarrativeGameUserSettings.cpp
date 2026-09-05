// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "AudioDevice.h"
#include <Sound/AudioSettings.h>
#include "ArsenalSettings.h"
#include <Engine/Engine.h>
#include "GenericPlatform/GenericApplication.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UnrealFramework/NarrativeAudioSettingsPolicy.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformProperties.h"
#include "Widgets/SWindow.h"

UNarrativeGameUserSettings::UNarrativeGameUserSettings()
{
	OverallAudioVolume = 1.f;
	SFXAudioVolume = 1.f;
	UIAudioVolume = 1.f;
	DialogueAudioVolume = 1.f;
	MusicAudioVolume = 0.3f;
	bCrouchToggles = true;
	bInventoryWantsTile = true; 
	GameplayDifficulty = ENarrativeGameplayDifficulty::Medium;
	FieldOfView = 90.f; 
	WeaponFieldOfView = 90.f; 
	SubtitleLevel = ENarrativeSubtitleLevel::Enabled;
	SelectedMonitor = "";

	bEnableMotionBlur = true;
	bEnableBloom = true; 

	Gamma = 2.2f;
}

void UNarrativeGameUserSettings::ApplySettings(bool bCheckForCommandLineOverrides)
{
	Super::ApplySettings(bCheckForCommandLineOverrides);

	//In editor we want to let UE handle setting resoltuions for PIE etc. 
#if !WITH_EDITOR
	ApplyMonitorSelection();
#endif 

	ApplySoundSettings();

	if (GEngine)
	{
		Gamma = FMath::Clamp<float>(Gamma, 0.5f, 5.f);

		GEngine->DisplayGamma = Gamma;
	}

}

void UNarrativeGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings();

	ApplySoundSettings();

	if (GEngine)
	{
		Gamma = FMath::Clamp<float>(Gamma, 0.5f, 5.f);

		GEngine->DisplayGamma = Gamma;
	}
}

void UNarrativeGameUserSettings::ApplySoundSettings()
{
	NormalizeAudioSettings();
	const UArsenalSettings* Configuration = ReadAudioConfiguration();
	TStrongObjectPtr<USoundMix> BaseMix(ReadAudioBaseMix());
	if (!HasAudioOutput() || !Configuration || !BaseMix.IsValid())
	{ SubmitDynamicRangeMix(nullptr); AppliedAudioDynamicRange = ENarrativeAudioDynamicRange::Full; return; }
	struct FBus { const FSoftObjectPath* Path; float Volume; };
	const FBus Buses[] = {
		{ &Configuration->MasterSoundClass, OverallAudioVolume }, { &Configuration->SFXSoundClass, SFXAudioVolume },
		{ &Configuration->UISoundClass, UIAudioVolume }, { &Configuration->DialogueSoundClass, DialogueAudioVolume },
		{ &Configuration->MusicSoundClass, MusicAudioVolume }, { &Configuration->AmbienceSoundClass, AmbienceAudioVolume },
		{ &Configuration->TinnitusSoundClass, TinnitusAudioVolume }
	};
	TSet<USoundClass*> Submitted;
	TArray<TStrongObjectPtr<USoundClass>> KeepClasses;
	for (const auto& Bus : Buses)
	{
		USoundClass* Class = Bus.Path->IsValid() ? Cast<USoundClass>(Bus.Path->TryLoad()) : nullptr;
		// An incorrectly aliased tinnitus/ambience bus must never overwrite Master or another slider.
		if (!Class || Submitted.Contains(Class)) { continue; }
		KeepClasses.Emplace(Class); Submitted.Add(Class);
		SubmitSoundClassVolume(BaseMix.Get(), Class, Bus.Volume);
	}
	const FSoftObjectPath* Preset = AudioDynamicRange == ENarrativeAudioDynamicRange::Reduced ? &Configuration->ReducedDynamicRangeSoundMix
		: AudioDynamicRange == ENarrativeAudioDynamicRange::Night ? &Configuration->NightDynamicRangeSoundMix : nullptr;
	TStrongObjectPtr<USoundMix> RangeMix(Preset && Preset->IsValid() ? Cast<USoundMix>(Preset->TryLoad()) : nullptr);
	// SoundMix duration -1 is the engine's persistent modifier contract. Timed authored effects cannot
	// represent a durable range preference, and the base volume mix must not be pushed a second time.
	if (RangeMix.IsValid() && (RangeMix.Get() == BaseMix.Get() || RangeMix->Duration >= 0.f || !FMath::IsFinite(RangeMix->Duration)))
	{ RangeMix.Reset(); }
	SubmitDynamicRangeMix(RangeMix.Get());
	AppliedAudioDynamicRange = RangeMix.IsValid() ? AudioDynamicRange : ENarrativeAudioDynamicRange::Full;
}

void UNarrativeGameUserSettings::NormalizeAudioSettings()
{
	using NarrativeAudioSettingsPolicy::Volume;
	OverallAudioVolume = Volume(OverallAudioVolume); DialogueAudioVolume = Volume(DialogueAudioVolume);
	UIAudioVolume = Volume(UIAudioVolume); SFXAudioVolume = Volume(SFXAudioVolume); MusicAudioVolume = Volume(MusicAudioVolume, .3f);
	AmbienceAudioVolume = Volume(AmbienceAudioVolume); TinnitusAudioVolume = Volume(TinnitusAudioVolume);
	if (!NarrativeAudioSettingsPolicy::ValidRange(static_cast<unsigned>(AudioDynamicRange))) { AudioDynamicRange = ENarrativeAudioDynamicRange::Full; }
}
void UNarrativeGameUserSettings::LoadSettings(bool bForceReload)
{ Super::LoadSettings(bForceReload); NormalizeAudioSettings(); }
void UNarrativeGameUserSettings::BeginDestroy()
{ ReleaseDynamicRangeMix(); Super::BeginDestroy(); }
bool UNarrativeGameUserSettings::HasAudioOutput() const
{
	const UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	return World && World->bAllowAudioPlayback && World->GetAudioDevice().IsValid();
}
const UArsenalSettings* UNarrativeGameUserSettings::ReadAudioConfiguration() const { return GetDefault<UArsenalSettings>(); }
USoundMix* UNarrativeGameUserSettings::ReadAudioBaseMix() const
{
	const UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	FAudioDeviceHandle Device = World ? World->GetAudioDevice() : FAudioDeviceHandle();
	return Device ? Device->GetDefaultBaseSoundMixModifier() : nullptr;
}
void UNarrativeGameUserSettings::SubmitSoundClassVolume(USoundMix* Mix, USoundClass* Class, float Volume)
{
	const UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	FAudioDeviceHandle Device = World && World->bAllowAudioPlayback ? World->GetAudioDevice() : FAudioDeviceHandle();
	if (Device && Mix && Class) { Device->SetSoundMixClassOverride(Mix, Class, NarrativeAudioSettingsPolicy::Volume(Volume), 1.f, 0.f, true); }
}
void UNarrativeGameUserSettings::ReleaseDynamicRangeMix()
{
	if (DynamicRangeAudioDevice && ActiveDynamicRangeMix) { DynamicRangeAudioDevice->PopSoundMixModifier(ActiveDynamicRangeMix, false); }
	ActiveDynamicRangeMix = nullptr; DynamicRangeAudioDevice.Reset();
}
void UNarrativeGameUserSettings::SubmitDynamicRangeMix(USoundMix* Mix)
{
	const UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	FAudioDeviceHandle Device = World && World->bAllowAudioPlayback ? World->GetAudioDevice() : FAudioDeviceHandle();
	if (Mix && Device && ActiveDynamicRangeMix == Mix && DynamicRangeAudioDevice
		&& DynamicRangeAudioDevice.GetDeviceID() == Device.GetDeviceID()) { return; }
	ReleaseDynamicRangeMix();
	if (Mix && Device)
	{
		ActiveDynamicRangeMix = Mix; DynamicRangeAudioDevice = Device;
		DynamicRangeAudioDevice->PushSoundMixModifier(Mix, false, false);
	}
}
bool UNarrativeGameUserSettings::IsAudioDynamicRangeAvailable(ENarrativeAudioDynamicRange Value) const
{
	if (Value == ENarrativeAudioDynamicRange::Full) { return true; }
	if (!NarrativeAudioSettingsPolicy::ValidRange(static_cast<unsigned>(Value))) { return false; }
	const UArsenalSettings* Configuration = ReadAudioConfiguration();
	if (!Configuration) { return false; }
	const auto& Path = Value == ENarrativeAudioDynamicRange::Reduced ? Configuration->ReducedDynamicRangeSoundMix : Configuration->NightDynamicRangeSoundMix;
	TStrongObjectPtr<USoundMix> Mix(Path.IsValid() ? Cast<USoundMix>(Path.TryLoad()) : nullptr);
	return Mix.IsValid() && FMath::IsFinite(Mix->Duration) && Mix->Duration < 0.f && Mix.Get() != ReadAudioBaseMix();
}
void UNarrativeGameUserSettings::SetAmbienceAudioVolume(float Value)
{ AmbienceAudioVolume = NarrativeAudioSettingsPolicy::Volume(Value); SaveSettings(); ApplySoundSettings(); }
void UNarrativeGameUserSettings::SetTinnitusAudioVolume(float Value)
{ TinnitusAudioVolume = NarrativeAudioSettingsPolicy::Volume(Value); SaveSettings(); ApplySoundSettings(); }
void UNarrativeGameUserSettings::SetAudioDynamicRange(ENarrativeAudioDynamicRange Value)
{
	AudioDynamicRange = NarrativeAudioSettingsPolicy::ValidRange(static_cast<unsigned>(Value)) ? Value : ENarrativeAudioDynamicRange::Full;
	SaveSettings(); ApplySoundSettings();
}

void UNarrativeGameUserSettings::ApplyMonitorSelection()
{
	// Consoles do not have desktop monitor selection. A saved PC preference must not move a
	// platform-owned viewport, and startup/headless teardown can have no Slate or native window.
	if (!FPlatformProperties::SupportsWindowedMode() || FPlatformProperties::HasFixedResolution()
		|| !GEngine || !GEngine->GameViewport || !FSlateApplication::IsInitialized()) { return; }
	const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
	if (!Window.IsValid()) { return; }
	//Apply resolution settings will have been called, move window to selected monitor! 
	// Move window to the corresponding monitor
	const FString DesiredMonitorName = GetSelectedMonitor();

	if (GEngine->GameViewport && !DesiredMonitorName.IsEmpty())
	{
		int32 MonitorIndex = -1;

		FDisplayMetrics Display;
		FSlateApplication::Get().GetDisplayMetrics(Display);

		for (int32 i = 0; i < Display.MonitorInfo.Num(); ++i)
		{
			if (Display.MonitorInfo[i].Name == DesiredMonitorName)
			{
				MonitorIndex = i;
				break;
			}
		}

		if (MonitorIndex >= 0 && Display.MonitorInfo.IsValidIndex(MonitorIndex))
		{
			//const float WidthPosition = (MonitorIndex)*Display.PrimaryDisplayWidth - Display.MonitorInfo[MonitorIndex].NativeWidth;
			//const float HeightPosition = (MonitorIndex)*Display.PrimaryDisplayHeight - Display.MonitorInfo[MonitorIndex].NativeHeight;

			const FVector2D WindowPosition = FVector2D(Display.MonitorInfo[MonitorIndex].WorkArea.Left, Display.MonitorInfo[MonitorIndex].WorkArea.Top);
			Window->MoveWindowTo(WindowPosition);
		}
	}
}

void UNarrativeGameUserSettings::SetOverallAudioVolume(const float NewOverallAudioVolume)
{
	OverallAudioVolume = FMath::IsFinite(NewOverallAudioVolume) ? FMath::Clamp(NewOverallAudioVolume, 0.f, 1.f) : 1.f;
	SaveSettings();
	ApplySoundSettings();
}

float UNarrativeGameUserSettings::GetOverallAudioVolume() const
{
	return OverallAudioVolume;
}

void UNarrativeGameUserSettings::SetDialogueAudioVolume(const float NewDialogueAudioVolume)
{
	DialogueAudioVolume = FMath::IsFinite(NewDialogueAudioVolume) ? FMath::Clamp(NewDialogueAudioVolume, 0.f, 1.f) : 1.f;
	SaveSettings();
	ApplySoundSettings();
}

float UNarrativeGameUserSettings::GetDialogueAudioVolume() const
{
	return DialogueAudioVolume;
}

void UNarrativeGameUserSettings::SetUIAudioVolume(const float NewUIAudioVolume)
{
	UIAudioVolume = FMath::IsFinite(NewUIAudioVolume) ? FMath::Clamp(NewUIAudioVolume, 0.f, 1.f) : 1.f;
	SaveSettings();
	ApplySoundSettings();
}

float UNarrativeGameUserSettings::GetUIAudioVolume() const
{
	return UIAudioVolume;
}

void UNarrativeGameUserSettings::SetSFXAudioVolume(const float NewSFXAudioVolume)
{
	SFXAudioVolume = FMath::IsFinite(NewSFXAudioVolume) ? FMath::Clamp(NewSFXAudioVolume, 0.f, 1.f) : 1.f;
	SaveSettings();
	ApplySoundSettings();
}

float UNarrativeGameUserSettings::GetSFXAudioVolume() const
{
	return SFXAudioVolume;
}

void UNarrativeGameUserSettings::SetMusicAudioVolume(const float NewMusicAudioVolume)
{
	MusicAudioVolume = FMath::IsFinite(NewMusicAudioVolume) ? FMath::Clamp(NewMusicAudioVolume, 0.f, 1.f) : 1.f;
	SaveSettings();
	ApplySoundSettings();
}

float UNarrativeGameUserSettings::GetMusicAudioVolume() const
{
	return MusicAudioVolume;
}

void UNarrativeGameUserSettings::SetShouldCrouchToggle(const bool bNewCrouchToggles)
{
	bCrouchToggles = bNewCrouchToggles;
	SaveSettings();
}

bool UNarrativeGameUserSettings::ShouldCrouchToggle()
{
	return bCrouchToggles;
}

void UNarrativeGameUserSettings::SetInventoryWantsTile(const bool bNewInventoryWantsTile)
{
	bInventoryWantsTile = bNewInventoryWantsTile;
}

bool UNarrativeGameUserSettings::InventoryWantsTile()
{
	return bInventoryWantsTile;
}

void UNarrativeGameUserSettings::SetEnableBloom(const bool bNewEnableBloom)
{
	bEnableBloom = bNewEnableBloom;
	SaveSettings();
}

bool UNarrativeGameUserSettings::WantsEnableBloom()
{
	return bEnableBloom;
}

void UNarrativeGameUserSettings::SetEnableMotionBlur(const bool bNewEnableMotionBlur)
{
	bEnableMotionBlur = bNewEnableMotionBlur;
	SaveSettings();
}

bool UNarrativeGameUserSettings::WantsEnableMotionBlur()
{
	return bEnableMotionBlur;
}

void UNarrativeGameUserSettings::SetGameplayDifficulty(const ENarrativeGameplayDifficulty NewDifficulty)
{
	GameplayDifficulty = NewDifficulty;
}

ENarrativeGameplayDifficulty UNarrativeGameUserSettings::GetGameplayDifficulty()
{
	return GameplayDifficulty;
}

void UNarrativeGameUserSettings::SetSubtitleLevel(const ENarrativeSubtitleLevel NewLevel)
{
	SubtitleLevel = NewLevel;
	SaveSettings();
}

ENarrativeSubtitleLevel UNarrativeGameUserSettings::GetSubtitleLevel()
{
	return SubtitleLevel;
}

FString UNarrativeGameUserSettings::GetSelectedMonitor()
{
	return SelectedMonitor;
}

void UNarrativeGameUserSettings::SetSelectedMonitor(const FString NewSelectedMonitor)
{
	SelectedMonitor = NewSelectedMonitor;
}

float UNarrativeGameUserSettings::GetFieldOfView()
{
	return FieldOfView;
}

void UNarrativeGameUserSettings::SetFieldOfView(const float NewFieldOfView)
{
	FieldOfView = FMath::IsFinite(NewFieldOfView) ? FMath::Clamp(NewFieldOfView, 60.f, 120.f) : 90.f;
	SaveSettings();
}

float UNarrativeGameUserSettings::GetWeaponFieldOfView()
{
	return WeaponFieldOfView;
}

void UNarrativeGameUserSettings::SetWeaponFieldOfView(const float NewWeaponFieldOfView)
{
	WeaponFieldOfView = FMath::IsFinite(NewWeaponFieldOfView) ? FMath::Clamp(NewWeaponFieldOfView, 60.f, 120.f) : 90.f;
	SaveSettings();
}

float UNarrativeGameUserSettings::GetGamma()
{
	return Gamma;
}

void UNarrativeGameUserSettings::SetGamma(const float NewGamma)
{
	Gamma = NewGamma;
}

FString UNarrativeGameUserSettings::GetOnlineUsername()
{
	return OnlineUsername;
}

void UNarrativeGameUserSettings::SetOnlineUsername(const FString Username)
{
	OnlineUsername = Username;
}

const UNarrativeGameUserSettings* UNarrativeGameUserSettings::GetSovSettings()
{
	return GEngine ? Cast<UNarrativeGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}
