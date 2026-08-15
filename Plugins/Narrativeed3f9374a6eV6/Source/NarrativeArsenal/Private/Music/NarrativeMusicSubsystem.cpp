// Copyright Narrative Tools 2025.

#include "Music/NarrativeMusicSubsystem.h"
#include "ArsenalSettings.h"
#include "NarrativeWorldSettings.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "Music/NativeMusicThemeTags.h"
#include "Music/TaggedMusicSet.h"

bool UNarrativeMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// only create on the client
	const UWorld* World = Outer? Outer->GetWorld() : nullptr;
	return World? !World->GetAuthGameMode() : false;
}

void UNarrativeMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// listen for world changes
	FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UNarrativeMusicSubsystem::WorldInit);
	FWorldDelegates::OnPreWorldFinishDestroy.AddUObject(this, &UNarrativeMusicSubsystem::WorldDeinit);
	
}

void UNarrativeMusicSubsystem::Deinitialize()
{
	// stop listening to world changes
	FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
	FWorldDelegates::OnPreWorldFinishDestroy.RemoveAll(this);
	
	Super::Deinitialize();
}

void UNarrativeMusicSubsystem::WorldInit(UWorld* World, FWorldInitializationValues WorldInitializationValues)
{	
	PendingMusicSet.Reset();
	
	ActiveTheme = TAG_MUSIC_AMBIENT;
	
	// if the world overrides the default music set, then use that instead
	if (ANarrativeWorldSettings* WorldSettings = Cast<ANarrativeWorldSettings>(World->GetWorldSettings()))
	{
		CurrentMusicSet = WorldSettings->DefaultMusicSetOverride;
	}

	// no world override, load default
	if (!CurrentMusicSet)
	{
		const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();

		// try collect live ptr
		CurrentMusicSet = ArsenalSettings->DefaultMusicSet.Get();
		
		// no live ptr, do async load
		if (!CurrentMusicSet)
		{
			LoadAndApplyMusicSet(ArsenalSettings->DefaultMusicSet);
		}		
	}		
}

void UNarrativeMusicSubsystem::WorldDeinit(UWorld* World)
{
	// destroy override
	if (OverrideAudioComponent)
	{
		OverrideAudioComponent->Stop();
		OverrideAudioComponent->DestroyComponent();
	}

	// destroy primary
	if (PrimaryAudioComponent)
	{
		PrimaryAudioComponent->Stop();
		PrimaryAudioComponent->DestroyComponent();
	}

	// clear pending music set handle
	if (PendingMusicSetLoadHandle.IsValid())
	{
		PendingMusicSetLoadHandle->CancelHandle();
	}

	// clear pending music set
	if (!PendingMusicSet.IsNull())
	{
		PendingMusicSet.Reset();
	}

	// set track id to none, clear theme and reset tracks
	ActiveTrackID = INDEX_NONE;
	ActiveTheme = FGameplayTag::EmptyTag;
	MusicTrackOne.Reset(World);
	MusicTrackTwo.Reset(World);
	MusicTrackQueue.Reset(World);
}

void UNarrativeMusicSubsystem::LoadAndApplyMusicSet(const TSoftObjectPtr<UTaggedMusicSet>& InMusicSet)
{
	// stop loading the current object if another is requested
	if (PendingMusicSetLoadHandle.IsValid() && PendingMusicSetLoadHandle->IsLoadingInProgress())
	{
		PendingMusicSetLoadHandle->CancelHandle();
		PendingMusicSet.Reset();
	}
	
	PendingMusicSet = InMusicSet;
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
	PendingMusicSetLoadHandle = StreamableManager.RequestAsyncLoad(PendingMusicSet.ToSoftObjectPath(), FStreamableDelegate::CreateUObject(this, &UNarrativeMusicSubsystem::PostLoadMusicSet));
}

void UNarrativeMusicSubsystem::PostLoadMusicSet()
{
	if (PendingMusicSetLoadHandle.IsValid() && !PendingMusicSetLoadHandle->WasCanceled() && PendingMusicSet.IsValid())
	{
		CurrentMusicSet = PendingMusicSet.Get();

		// switch to the current theme sound for the music set
		if (GetActiveTheme().IsValid() && CurrentMusicSet->Has(GetActiveTheme()))
		{
			ClearAllThemeOverrides(EThemeOverrideClearMode::NonPersistant);
			SetTheme(GetActiveTheme(), false);
		}
	}
}

bool UNarrativeMusicSubsystem::InitPrimaryAudioComponent()
{
	if (!PrimaryAudioComponent)
	{
		const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
		PrimaryAudioComponent = UGameplayStatics::SpawnSound2D(this, ArsenalSettings->MasterMetaSound.LoadSynchronous(), 1, 1, 0, nullptr, false, false);
	}
	return PrimaryAudioComponent != nullptr;
}

bool UNarrativeMusicSubsystem::InitOverrideAudioComponent(USoundBase* Sound)
{
	if (!OverrideAudioComponent)
	{
		OverrideAudioComponent = UGameplayStatics::SpawnSound2D(this, Sound, 1, 1, 0, nullptr, false, false);
	}
	return OverrideAudioComponent != nullptr;
}

void UNarrativeMusicSubsystem::PostTrackFade(int32 TrackID, const bool bFadeIn)
{
	FMusicTrackState* TrackState = GetTrackStateFromID(TrackID);
	
	if (bFadeIn)
	{
		// only set fade state as this track is now active
		TrackState->FadeState = FMusicTrackState::ETrackFadeState::None;
	}
	else
	{
		// this track is now free
		TrackState->Reset(GetWorld());
	}

	// try queue 
	if (MusicTrackQueue.Theme.IsValid())
	{
		const auto PendingTheme = MusicTrackQueue.Theme;
		MusicTrackQueue.Reset(GetWorld());
		
		SetTheme(PendingTheme, false);
	}
}

FMusicSound UNarrativeMusicSubsystem::GetThemeOverride(FGameplayTag Theme)
{
	if (PersistantThemeOverrides.Contains(Theme))
	{
		return PersistantThemeOverrides[Theme];
	}
	
	return ThemeOverrides.Contains(Theme)? ThemeOverrides[Theme] : FMusicSound();
}

bool UNarrativeMusicSubsystem::CanQueueTheme(UTaggedMusicSet* NewMusicSet, FGameplayTag Theme) const
{
	if (MusicTrackQueue.DoesThemeMatch(NewMusicSet, Theme))
	{
		// already queued
		return false;
	}

	if (MusicTrackOne.DoesThemeMatch(CurrentMusicSet, Theme) && !MusicTrackOne.IsFadingOut())
	{
		// theme is not fading out, can not requeue it
		return false;
	}
	
	if (MusicTrackTwo.DoesThemeMatch(CurrentMusicSet, Theme) && !MusicTrackTwo.IsFadingOut())
	{
		// theme is not fading out, can not requeue it
		return false;
	}
	
	return true;
}

bool UNarrativeMusicSubsystem::SetTheme(FGameplayTag Theme, bool bImmediate)
{
	if (!Theme.IsValid() || !CurrentMusicSet)
	{
		return false;
	}

	// when music set is pending, set just the active theme so that it starts transitioning to the new theme
	if (PendingMusicSetLoadHandle.IsValid() && !PendingMusicSetLoadHandle->HasLoadCompleted())
	{
		ActiveTheme = Theme;
		return true;
	}
	
	if (!InitPrimaryAudioComponent())
	{
		return false;
	}

	// we're overidding music already 
	if (OverrideMusicSound.Music)
	{
		return false; 
		//ClearOverrideMusicWithSound();
	}
	
	// check override
	FMusicSound MusicSound = GetThemeOverride(Theme);
	if (!MusicSound.Music)
	{
		MusicSound = CurrentMusicSet->Get(Theme);
		
		// no override, check from set
		if (!MusicSound.Music)
		{
			return false;
		}
	}
	
	if (MusicTrackOne.IsFadingOut() || MusicTrackTwo.IsFadingOut())
	{
		// a track is fading out, check if this theme can be queued
		if (CanQueueTheme(CurrentMusicSet, Theme))
		{
			MusicTrackQueue.Theme	 = Theme;
			MusicTrackQueue.MusicSet = CurrentMusicSet;
			MusicTrackQueue.Sound	 = MusicSound;
			return true;
		}
		return false;
	}
	
	if (MusicTrackOne.DoesThemeMatch(CurrentMusicSet, Theme) || MusicTrackTwo.DoesThemeMatch(CurrentMusicSet, Theme))
	{
		// theme is already playing from current music set
		return false;
	}

	FMusicTrackState* FadeInTrackState = ActiveTrackID == INDEX_NONE? &MusicTrackOne : nullptr;
	FMusicTrackState* FadeOutTrackState = nullptr;
	
	if (!FadeInTrackState)
	{
		// select correct tracks for fade in - out
		if (ActiveTrackID == MusicTrackOne.TrackID)
		{
			FadeInTrackState  = &MusicTrackTwo;
			FadeOutTrackState = &MusicTrackOne;
		}
		else
		{
			FadeInTrackState  = &MusicTrackOne;
			FadeOutTrackState = &MusicTrackTwo;
		}
	}
	else
	{
		// first time playing, start right away
		bImmediate = true;
	}

	TArray<FAudioParameter> ParamsStack;
	ParamsStack.Add({"NarrativeSoundAsset", MusicSound.Music});

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	
	/* fade in */
	{
		const float FadeInTime = bImmediate? 0.01f : MusicSound.FadeInDuration;
		ParamsStack.Add({FadeInTrackState->ParamName("NarrativeTrackVolume"), 1.0f});
		ParamsStack.Add({FadeInTrackState->ParamName("NarrativeTrackFadeTime"), FadeInTime});
		ParamsStack.Add({FadeInTrackState->ParamName("NarrativeTrackStart"), EAudioParameterType::Trigger});

		ActiveTrackID = FadeInTrackState->TrackID;
		ActiveTheme = Theme;
		
		FadeInTrackState->Theme     = Theme;
		FadeInTrackState->MusicSet	= CurrentMusicSet;
		FadeInTrackState->Sound		= MusicSound;
		FadeInTrackState->FadeState = FMusicTrackState::ETrackFadeState::In; 
		
		// post fade in
		TimerManager.SetTimer(FadeInTrackState->FadeHandle, FTimerDelegate::CreateUObject(this, &UNarrativeMusicSubsystem::PostTrackFade, FadeInTrackState->TrackID, true), FadeInTime, false);
	}
	/* fade in */

	/* fade out */
	if (FadeOutTrackState)
	{
		const float FadeOutTime = bImmediate? 0.01f : FadeOutTrackState->Sound.FadeOutDuration;
		ParamsStack.Add({FadeOutTrackState->ParamName("NarrativeTrackVolume"), 0.0f});
		ParamsStack.Add({FadeOutTrackState->ParamName("NarrativeTrackFadeTime"), FadeOutTime});
		FadeOutTrackState->FadeState = FMusicTrackState::ETrackFadeState::Out;
		
		// post fade out
		TimerManager.SetTimer(FadeOutTrackState->FadeHandle, FTimerDelegate::CreateUObject(this, &UNarrativeMusicSubsystem::PostTrackFade, FadeOutTrackState->TrackID, false), FadeOutTime, false);
	}
	/* fade out */

	PrimaryAudioComponent->SetParameters(MoveTemp(ParamsStack));
	return true;
}

bool UNarrativeMusicSubsystem::OverrideMusicSet(TSoftObjectPtr<UTaggedMusicSet> NewMusicSet)
{
	if (!NewMusicSet.IsNull())
	{
		LoadAndApplyMusicSet(NewMusicSet);
	}
	return false;
}

bool UNarrativeMusicSubsystem::ResetMusicSetToDefault()
{
	// if the world overrides the default music set, then use that instead
	if (ANarrativeWorldSettings* WorldSettings = Cast<ANarrativeWorldSettings>(GetWorld()->GetWorldSettings()))
	{
		CurrentMusicSet = WorldSettings->DefaultMusicSetOverride;
	}

	// no world override, load default
	if (!CurrentMusicSet)
	{
		const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();

		// try collect live ptr
		CurrentMusicSet = ArsenalSettings->DefaultMusicSet.Get();
		
		// no live ptr, do async load
		if (!CurrentMusicSet)
		{
			LoadAndApplyMusicSet(ArsenalSettings->DefaultMusicSet);
		}		
	}
	return false;
}

bool UNarrativeMusicSubsystem::OverrideTheme(FGameplayTag Theme, USoundBase* Sound, float FadeInDuration, float FadeOutDuration, bool bPersistant)
{
	if (!Sound || !Theme.IsValid())
	{
		return false;
	}
	
	if (FMusicSound* OverrideSound = bPersistant? &PersistantThemeOverrides.FindOrAdd(Theme) : &ThemeOverrides.FindOrAdd(Theme))
	{
		OverrideSound->Music           = Sound;
		OverrideSound->FadeInDuration  = FadeInDuration;
		OverrideSound->FadeOutDuration = FadeOutDuration;

		// update the new theme sound
		if (GetActiveTheme().IsValid())
		{
			return SetTheme(GetActiveTheme(), false);
		}
		
		return true;
	}
	return false;
}

void UNarrativeMusicSubsystem::ClearThemeOverride(FGameplayTag Theme)
{
	ThemeOverrides.Remove(Theme);
	PersistantThemeOverrides.Remove(Theme);

	// update the theme
	if (GetActiveTheme().IsValid())
	{
		SetTheme(GetActiveTheme(), false);
	}
}

void UNarrativeMusicSubsystem::ClearAllThemeOverrides(EThemeOverrideClearMode RemoveMode)
{
	const bool bRemoveBoth = RemoveMode == EThemeOverrideClearMode::Both;
	
	if (bRemoveBoth || RemoveMode == EThemeOverrideClearMode::NonPersistant)
	{
		ThemeOverrides.Empty(ThemeOverrides.Num());
	}
	
	if (bRemoveBoth || RemoveMode == EThemeOverrideClearMode::Persistant)
	{
		PersistantThemeOverrides.Empty(ThemeOverrides.Num());
	}
}

bool UNarrativeMusicSubsystem::OverrideMusicWithSound(USoundBase* Sound, bool bUISound, float FadeDuration)
{
	if (!Sound)
	{
		return false;
	}

	// reset
	OverrideFadeDuration = 0.0f;
	const bool bFadeInOut = FadeDuration > 0.0f;

	// stop primary
	if (PrimaryAudioComponent)
	{
		if (bFadeInOut)
		{
			PrimaryAudioComponent->AdjustVolume(FadeDuration, 0.01f);
		}
		else
		{
			PrimaryAudioComponent->SetPaused(true);
		}
	}
	
	if (InitOverrideAudioComponent(Sound))
	{
		OverrideMusicSound.Music = Sound;
		
		OverrideAudioComponent->SetUISound(bUISound);
		
		// set and start playing the new sound
		OverrideAudioComponent->SetSound(Sound);

		// component may be stopped, in this case we need to kick-start play 
		if (!OverrideAudioComponent->IsPlaying())
		{
			OverrideAudioComponent->Play();
		}

		if (bFadeInOut)
		{
			OverrideFadeDuration = FadeDuration;
			OverrideAudioComponent->AdjustVolume(FadeDuration, 1.0f);
		}
	}
	
	return true;
}

void UNarrativeMusicSubsystem::ClearOverrideMusicWithSound()
{
	if (!OverrideAudioComponent)
	{
		return;
	}
		
	if (OverrideMusicSound.Music)
	{
		OverrideMusicSound.Music = nullptr;
		
		// start primary again
		if (PrimaryAudioComponent)
		{
			if (PrimaryAudioComponent->bIsPaused)
			{
				PrimaryAudioComponent->SetPaused(false);
			}
			else
			{
				PrimaryAudioComponent->AdjustVolume(OverrideFadeDuration, 1.0f);
				OverrideAudioComponent->AdjustVolume(OverrideFadeDuration, 0.0f);
			}
		}
		
		SetTheme(ActiveTheme, true);
	}
	
	OverrideFadeDuration = 0.0f;
}
