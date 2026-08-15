// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "AudioDevice.h"
#include <Sound/AudioSettings.h>
#include "ArsenalSettings.h"
#include <Engine/Engine.h>
#include "GenericPlatform/GenericApplication.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameViewportClient.h"

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
	if (GEngine)
	{
		if (const UWorld* World = GEngine->GetCurrentPlayWorld())
		{
			if (World->bAllowAudioPlayback)
			{
				if(const UAudioSettings* EngineAudioSettings = GetDefault<UAudioSettings>())
				{
					if(const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>())
					{
						if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice())
						{
							//Setup assumes we keep the default mix defined in .ini
							USoundMix* DefaultMix = AudioDevice->GetDefaultBaseSoundMixModifier();

							if (USoundClass* OverallClass = Cast<USoundClass>(ArsenalSettings->MasterSoundClass.TryLoad()))
							{
								AudioDevice->SetSoundMixClassOverride(DefaultMix, OverallClass, OverallAudioVolume, 1.f, 0.f, true);
							}

							if (USoundClass* SFXClass = Cast<USoundClass>(ArsenalSettings->SFXSoundClass.TryLoad()))
							{
								AudioDevice->SetSoundMixClassOverride(DefaultMix, SFXClass, SFXAudioVolume, 1.f, 0.f, true);
							}

							if (USoundClass* UIClass = Cast<USoundClass>(ArsenalSettings->UISoundClass.TryLoad()))
							{
								AudioDevice->SetSoundMixClassOverride(DefaultMix, UIClass, UIAudioVolume, 1.f, 0.f, true);
							}

							if (USoundClass* DialogueClass = Cast<USoundClass>(ArsenalSettings->DialogueSoundClass.TryLoad()))
							{
								AudioDevice->SetSoundMixClassOverride(DefaultMix, DialogueClass, DialogueAudioVolume, 1.f, 0.f, true);
							}

							if (USoundClass* MusicClass = Cast<USoundClass>(ArsenalSettings->MusicSoundClass.TryLoad()))
							{
								AudioDevice->SetSoundMixClassOverride(DefaultMix, MusicClass, MusicAudioVolume, 1.f, 0.f, true);
							}
						}
					}
				}
			}
		}
	}
}

void UNarrativeGameUserSettings::ApplyMonitorSelection()
{
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
			GEngine->GameViewport->GetWindow()->MoveWindowTo(WindowPosition);
		}
	}
}

void UNarrativeGameUserSettings::SetOverallAudioVolume(const float NewOverallAudioVolume)
{
	OverallAudioVolume = NewOverallAudioVolume;
}

float UNarrativeGameUserSettings::GetOverallAudioVolume() const
{
	return OverallAudioVolume;
}

void UNarrativeGameUserSettings::SetDialogueAudioVolume(const float NewDialogueAudioVolume)
{
	DialogueAudioVolume = NewDialogueAudioVolume;
}

float UNarrativeGameUserSettings::GetDialogueAudioVolume() const
{
	return DialogueAudioVolume;
}

void UNarrativeGameUserSettings::SetUIAudioVolume(const float NewUIAudioVolume)
{
	UIAudioVolume = NewUIAudioVolume;
}

float UNarrativeGameUserSettings::GetUIAudioVolume() const
{
	return UIAudioVolume;
}

void UNarrativeGameUserSettings::SetSFXAudioVolume(const float NewSFXAudioVolume)
{
	SFXAudioVolume = NewSFXAudioVolume;
}

float UNarrativeGameUserSettings::GetSFXAudioVolume() const
{
	return SFXAudioVolume;
}

void UNarrativeGameUserSettings::SetMusicAudioVolume(const float NewMusicAudioVolume)
{
	MusicAudioVolume = NewMusicAudioVolume;
}

float UNarrativeGameUserSettings::GetMusicAudioVolume() const
{
	return MusicAudioVolume;
}

void UNarrativeGameUserSettings::SetShouldCrouchToggle(const bool bNewCrouchToggles)
{
	bCrouchToggles =  bNewCrouchToggles;
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
}

bool UNarrativeGameUserSettings::WantsEnableBloom()
{
	return bEnableBloom;
}

void UNarrativeGameUserSettings::SetEnableMotionBlur(const bool bNewEnableMotionBlur)
{
	bEnableMotionBlur = bNewEnableMotionBlur;
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
	FieldOfView = NewFieldOfView;
}

float UNarrativeGameUserSettings::GetWeaponFieldOfView()
{
	return WeaponFieldOfView;
}

void UNarrativeGameUserSettings::SetWeaponFieldOfView(const float NewWeaponFieldOfView)
{
	WeaponFieldOfView = NewWeaponFieldOfView;
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
