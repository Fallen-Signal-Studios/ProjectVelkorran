// Copyright Narrative Tools 2025.

#pragma once

#include "GameplayTagContainer.h"
#include "TaggedMusicSet.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NarrativeMusicSubsystem.generated.h"

struct FStreamableHandle;
class UTaggedMusicSet;

UENUM(BlueprintType)
enum class EThemeOverrideClearMode : uint8
{
	Both          UMETA(ToolTip="remove non persistant and persistant overrides"),
	NonPersistant UMETA(ToolTip="remove non persistant overrides"),
	Persistant    UMETA(ToolTip="remove persistant overrides")
};

// state of a music track
USTRUCT()
struct FMusicTrackState
{
	GENERATED_BODY()

	enum class ETrackFadeState : uint8
	{
		None,
		In,
		Out
	};

	// track id provided on construct
	int32 TrackID;
	// theme of this track
	FGameplayTag Theme;
	// fade in / out timer handle
	FTimerHandle FadeHandle;
	// current fade state
	ETrackFadeState FadeState;

	// music set last used for this track
	UPROPERTY()
	TWeakObjectPtr<UTaggedMusicSet> MusicSet;

	// last sound for this track
	UPROPERTY()
	FMusicSound Sound;
	
	FMusicTrackState(): TrackID(INDEX_NONE), FadeState(ETrackFadeState::None) {}

	// construct with specific track ID
	explicit FMusicTrackState(const int32 InTrackID) : TrackID(InTrackID), FadeState(ETrackFadeState::None) {}

	// get param name for track of this states Track ID
	FName ParamName(FName Name) const
	{
		const FString ParamStr = Name.ToString() + FString::FromInt(TrackID);
		return FName{ParamStr};
	}

	// returns true if the theme and the music set are the same
	bool DoesThemeMatch(const TObjectPtr<UTaggedMusicSet>& InMusicSet, const FGameplayTag InTheme) const
	{
		return InMusicSet == MusicSet && InTheme.MatchesTagExact(Theme);
	}

	// reset back to default, clearing handles.
	void Reset(const UWorld* WorldContext)
	{
		FTimerManager& TimerManager = WorldContext->GetTimerManager();
		TimerManager.ClearTimer(FadeHandle);
		
		Theme = FGameplayTag::EmptyTag;
		FadeHandle.Invalidate();
		MusicSet.Reset();
		Sound.Music = nullptr;
		FadeState = ETrackFadeState::None;
	}

	bool IsFadingIn()  const { return FadeState == ETrackFadeState::In; }
	bool IsFadingOut() const { return FadeState == ETrackFadeState::Out; }
	bool IsFading()    const { return FadeState != ETrackFadeState::None; }
	
};

/**
 * Manages a music. 
 */
UCLASS(MinimalAPI, DisplayName="Narrative Music")
class UNarrativeMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
private:

	// music set in use
	UPROPERTY()
	TObjectPtr<UTaggedMusicSet> CurrentMusicSet;
	// audio component for this world for general music themes
	UPROPERTY()
	UAudioComponent* PrimaryAudioComponent;
	// when overriding music with a specific asset, this component is created and used
	UPROPERTY()
	UAudioComponent* OverrideAudioComponent;
	// handle to the pending music set asset
	TSharedPtr<FStreamableHandle> PendingMusicSetLoadHandle;
	// pending music set asset
	TSoftObjectPtr<UTaggedMusicSet> PendingMusicSet;
	// track 1
	FMusicTrackState MusicTrackOne{1};
	// track 2
	FMusicTrackState MusicTrackTwo{2};
	// track queue
	FMusicTrackState MusicTrackQueue;
	// either 1 or 2, current track that should be playing
	int32 ActiveTrackID = INDEX_NONE;
	// an active theme is a theme that is faded into
	FGameplayTag ActiveTheme;
	// overrides across music set changes. these themes have a higher priority over ThemeOverrides
	TMap<FGameplayTag, FMusicSound> PersistantThemeOverrides;
	// overrides that only exist for the current music set
	TMap<FGameplayTag, FMusicSound> ThemeOverrides;

protected: 
	// manual override sound
	UPROPERTY(BlueprintReadOnly, Category = "Music")
	FMusicSound OverrideMusicSound;

	// current fade duration of the sound wave override 
	UPROPERTY(BlueprintReadOnly, Category = "Music")
	float OverrideFadeDuration;
	
public:
	
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	
	void WorldInit(UWorld* World, FWorldInitializationValues WorldInitializationValues);
	void WorldDeinit(UWorld* World);

	// begin loading and applying music set
	void LoadAndApplyMusicSet(const TSoftObjectPtr<UTaggedMusicSet>& InMusicSet);
	// finished loading music set
	void PostLoadMusicSet();

	// returns true if primary audio component is valid, but will attempt to create one if invalid
	bool InitPrimaryAudioComponent();

	// returns true if override audio component is valid, but will attempt to create one if invalid
	bool InitOverrideAudioComponent(USoundBase* Sound);

	// get a ptr to the music track associated with the track ID
	FMusicTrackState* GetTrackStateFromID(const int32 TrackID) { return TrackID == MusicTrackOne.TrackID ? &MusicTrackOne : &MusicTrackTwo; }

	// called after a track fade in or out
	void PostTrackFade(int32 TrackID, const bool bFadeIn);
	// gets the most relevant override for a current theme
	FMusicSound GetThemeOverride(FGameplayTag Theme);
	// returns true when it is possible to que a given theme from a music set
	bool CanQueueTheme(UTaggedMusicSet* NewMusicSet, FGameplayTag Theme) const;

public:

	// returns the current active music set
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Narrative|Music")
	UTaggedMusicSet* GetActiveMusicSet() const { return CurrentMusicSet; }
	
	// returns the current active theme
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Narrative|Music")
	FGameplayTag GetActiveTheme() const { return ActiveTheme; }

	/// returns the audio component that is playing music.
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Narrative|Music")
	UAudioComponent* GetActiveAudioComponent() { return OverrideMusicSound.Music? OverrideAudioComponent : PrimaryAudioComponent; }
	
	/**
	 * sets the current theme.
	 * @param Theme theme to use.
	 * @param bImmediate if true, fade in and out duration for the music sound is ignored.
	 * @return true if theme was set.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music", meta=(Categories="Music"))
	bool SetTheme(FGameplayTag Theme, bool bImmediate);

	// overrides the current music set until removed or overriden
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music")
	bool OverrideMusicSet(TSoftObjectPtr<UTaggedMusicSet> NewMusicSet);

	// sets the music set back to the default, either from the project settings or world setting
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music")
	bool ResetMusicSetToDefault();

	/**
	 * overrides a given theme with a specific sound asset.
	 * @param Theme the theme to override.
	 * @param Sound sound to override the theme with.
	 * @param FadeInDuration time it takes to fade in this override.
	 * @param FadeOutDuration time it takes to fade out this override.
	 * @param bPersistant if true, this sound with persist across music set changes and will not be cleared until explicitly told.
	 * @return 
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music", meta=(Categories="Music"))
	bool OverrideTheme(FGameplayTag Theme, USoundBase* Sound, float FadeInDuration = 3.0f, float FadeOutDuration = 3.0f, bool bPersistant = false);

	// removes a theme overrides
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music", meta=(Categories="Music"))
	void ClearThemeOverride(FGameplayTag Theme);
	
	// removes all theme override
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music")
	void ClearAllThemeOverrides(EThemeOverrideClearMode RemoveMode);

	/**
	 * overrides the current music with a specific sound asset.
	 * @param Sound sound to play.
	 * @param bUISound is this sound, an ui sound.
	 * @param FadeDuration duration to fade into this sound over. if greater than 0.0, pause the current music and resume it when the override is cleared.
	 * @return true if successful.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music")
	bool OverrideMusicWithSound(USoundBase* Sound, bool bUISound, float FadeDuration);

	// stops overriding the music with a specific sound asset
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Narrative|Music")
	void ClearOverrideMusicWithSound();
	
};
