// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "LevelSequenceActor.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "Navigation/GameplayTasks/GameplayTask_MoveToLocationAndRotation.h"
#include "NarrativeLevelSequencePlayer.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "NarrativeLevelSequenceActor.generated.h"


USTRUCT(BlueprintType)
struct FNarrativeSequencerBindingConfig
{
	GENERATED_BODY()

	FNarrativeSequencerBindingConfig()
	{
		Character = nullptr;
		CinematicStartTransform = FTransform();

		//Cant reference FNarrativeGameplayTags in constructor unfortunately
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.SequencerControlled", false));
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.Invulnerable", false));
	};

	/** The binding tag we're using for this character   */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	FName BindingTag;

	/** The character we wish to bind into the cinematic.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	TObjectPtr<class ANarrativeCharacter> Character;

	/** If set we'll run the character here before we start the cinematic so everything lines up properly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	FTransform CinematicStartTransform;

	/** We'll apply these tags to the character whilst they are bound into the cinematic. Overrides TagsToApply in base settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	FGameplayTagContainer TagsToApplyWhilstBound;

};

/**
 * Custom settings that we're adding to the narrative level sequence player.
 */
USTRUCT(BlueprintType)
struct FNarrativeSequencePlaybackSettings : public FMovieSceneSequencePlaybackSettings
{
	GENERATED_BODY()

	FNarrativeSequencePlaybackSettings()
	{
		bHideEvenEssentialHUDElements = false;
		bShowCinematicBars = false;
		bStopDialogue = false;
		bAutoPlay = true;
		bCanSkip = true;
		bUpdateControlRotationToPawn = false;

		//Cant reference FNarrativeGameplayTags in constructor unfortunately so these are hardcoded.
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.Player.WantsCinematicBars", false));
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.Player.WantsHideHUD", false));
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.SequencerControlled", false));
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.Invulnerable", false));
		TagsToApplyWhilstBound.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.DontReturnToSpawn", false));

		StopTags.AddTag(FGameplayTag::RequestGameplayTag("Narrative.State.IsDead", false));
	};

	/** If true, will keep control rotation yaw matched with the controlled pawns yaw. This is useful when we blend out of the sequence
	, if you leave this as false the camera will go back to where we were pre-sequence, which often isn't what you want. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bUpdateControlRotationToPawn : 1;

	/** Do we want to show cinematic black bars?
	DEPRECATED - the better way to add these is to add the Narrative.State.Player.WantsCinematicBars tag to the player, as this supports dynamically/adding removing bars using GameplayTag track. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Cinematic")
	uint32 bShowCinematicBars : 1;

	/** If true, the cinematic menu will give a skip option to the player when this cinematic is playing.
	 * DEPRECATED - use a skip track instead to define where skipping will skip the sequence to.
	 */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category="Cinematic")
	uint32 bCanSkip : 1;

	/** If dialogue is playing should we stop it before starting the cinematic?  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bStopDialogue : 1;

	/** Do we want to hide the essential HUD elements, like quest updates, etc when this is playing.
	DEPRECATED - the better way is to add the Narrative.State.Player.WantsHideHUD.All tag to the player, as this supports dynamically/adding removing bars using GameplayTag track.*/
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category="Cinematic")
	uint32 bHideEvenEssentialHUDElements : 1;

	/** We'll apply these tags to any bound actors with ASCs when they are bound into this sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	FGameplayTagContainer TagsToApplyWhilstBound;

	/** We'll prevent playback beginning if any of these tags are present.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	FGameplayTagContainer StopTags;

	/** Required possessable participant bindings. Empty preserves generic Narrative scenes. Missing participants never wait indefinitely. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	TArray<FName> RequiredParticipantBindingTags;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.1", ClampMax="30"))
	float ParticipantReadyTimeoutSeconds = 5.f;


};

/**
 * A cinematic dialogue line that can be selected whilst a cinematic is playing.
 */
USTRUCT(BlueprintType)
struct FNarrativeCineDialogueLine
{
	GENERATED_BODY()

	FNarrativeCineDialogueLine()
	{
		OptionText = FText();
		DestinationSequence = nullptr;
		DestinationMarker = FString();
		bKeepExistingSequenceSettings = true;
		bBlendAnimsBetweenSequences = true;
		DestinationPlaybackSettings = FNarrativeSequencePlaybackSettings();
	}

	/** Conditions that must pass for the line to be selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Cinematic")
	TArray<TObjectPtr<class UNarrativeCondition>> Conditions;

	/** The dialogue option text to show the player */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	FText OptionText;

	/** The hint option text to show the player after the Option Text - ie Persurade, Intimidate, Help, etc*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	FText HintText;

	/** The destination sequence to to jump to when this is selected. If empty, we'll try jump to the DestinationMarker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	TObjectPtr<class ULevelSequence> DestinationSequence;

	/** The destination marker to jump to when this is selected.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	FString DestinationMarker;

	/** Jumping to the destination will cause the character animations to snap in default UE5. You can set this to true to blend instead.
	This is essential if you're not changing cameras when going to the destination because otherwise you will see your character snap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bBlendAnimsBetweenSequences;

	/** Whether to use the existing settings or override them with custom ones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bKeepExistingSequenceSettings;

	/** The playback settings for the destination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "!bKeepExistingSequenceSettings", EditConditionHides))
	FNarrativeSequencePlaybackSettings DestinationPlaybackSettings;

};

/**
 * Level sequence actor that has been modified to allow for extra functionality base UE5 sequencer can't do, such as moving NPCs into place before sequence begins to allow seamless transition, etc.
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativeLevelSequenceActor : public ALevelSequenceActor
{
	GENERATED_BODY()


public:

	ANarrativeLevelSequenceActor(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	UFUNCTION(BlueprintPure, Category = "Playback")
	virtual TArray<UObject*> GetBoundObjects() const;

	//Given an NPCSpawner, bind any NPCs managed by that spawn into the sequence using their spawns Component name .
	UFUNCTION(BlueprintCallable, Category = "Playback")
	virtual void SetBindingsUsingSpawner(class ANPCSpawner* Spawner);

	//Update sequence and binding params etc.
	UFUNCTION(BlueprintCallable, Category = "Playback")
	virtual void UpdateSequence(ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings InSettings);

	//Blend captured participant poses for a bounded interval, then stop and release this session.
	//The actor remains reusable for subsequent playback.
	UFUNCTION(BlueprintCallable, Category = "Playback")
	virtual void BlendOutAndStop();
	uint64 GetPlaybackGeneration() const { return PlaybackGeneration; }
	/** Cancel an unstarted request only while the caller still owns its generation. */
	void CancelPendingPlayback(uint64 ExpectedGeneration);
	/** Start when bound participants are ready, or queue the existing bounded readiness wait. */
	UFUNCTION()
	virtual void PlaySequence();
	bool CanAcceptPlayback() const { return !bIsEndingPlay && !bEndingPlayback && !bChangingSequence; }
	/** Emitted after an explicit blend has stopped playback and released only this sequence's ownership. */
	UPROPERTY(BlueprintAssignable, Category="Playback") FOnMovieSceneSequencePlayerEvent OnBlendOutFinished;
	/** Setup timeout, fatal/missing participant, or invalid sequence. Never counts as a complete viewing. */
	UPROPERTY(BlueprintAssignable, Category="Playback") FOnMovieSceneSequencePlayerEvent OnPlaybackFailed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playback", meta=(ClampMin="0", ClampMax="2")) float BlendOutSeconds = .5f;

	/**
	 * Create a new level sequence player.
	 *
	 * @param WorldContextObject Context object from which to retrieve a UWorld.
	 * @param Players The players that this cinematic should be replicated to - if empty this will rep to everyone that the cinematic is relevant for
	 * @param SpawnLocation The location to spawn the cine actor, which is used for net relevancy.
	 * @param RelevancyDist If we're within this dist from the spawn location
	 * @param LevelSequence The level sequence to play.
	 * @param Settings The desired playback settings
	 * @param OutActor The level sequence actor created to play this sequence.
	 */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Narrative", meta=(WorldContext="WorldContextObject", DynamicOutputParam="OutActor"))
	static UNarrativeLevelSequencePlayer* CreateNarrativeLevelSequencePlayer(UObject* WorldContextObject, const TArray<APlayerController*>& Players, const FVector SpawnLocation, const float RelevancyDist, ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings InSettings, ANarrativeLevelSequenceActor*& OutActor);

public:

	//The specialized narrative playback settings, containing some extra pieces of functionality.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ShowOnlyInnerProperties, ExposeOnSpawn))
	FNarrativeSequencePlaybackSettings NarrativeSequenceParams;

	//Controllers we want the cinematic to be relevant to
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ExposeOnSpawn))
	TArray<TObjectPtr<class APlayerController>> OwnerControllers;

protected:

	UFUNCTION()
	void BindingVisualReady(class ANarrativeCharacter* Character);

	//Try and bind any characters and their visuals into the cutscene. Called automatically, however is exposed if you want to manually ask cutscene to try re-bind everything.
	UFUNCTION(BlueprintCallable, Category = "Playback")
	void RefreshBindings();

	UFUNCTION()
	void OnPlay();

	UFUNCTION()
	void OnStop();

private:
	friend struct FNarrativeSequenceLifecycleTestAccess;
	bool ParticipantsReady() const;
	bool AcquireParticipantOwnership(uint64 ExpectedEpoch);
	void ReleaseParticipantOwnership();
	void FailPlayback();
	UFUNCTION() void FinishBlendOut();
	TMap<TWeakObjectPtr<class UAbilitySystemComponent>, FGameplayTagContainer> OwnedParticipantTags;
	TArray<TWeakObjectPtr<class ANarrativePlayerController>> NotifiedControllers;
	TArray<TWeakObjectPtr<ANarrativeCharacter>> WaitingVisuals;
	FNarrativeSequencePlaybackSettings ActiveNotificationSettings;
	FTimerHandle BlendOutTimer;
	bool bSessionActive = false;
	bool bPendingPlayback = false;
	bool bBlendingOut = false;
	bool bEndingPlayback = false;
	float PendingPlaybackSeconds = 0.f;
	float ParticipantPollSeconds = 0.f;
	uint64 OwnershipEpoch = 0;
	uint64 PlaybackGeneration = 0;
	bool bIsEndingPlay = false;
	bool bChangingSequence = false;
};

//This handy node will play a narrative sequence and notify you when it finishes
UCLASS()
class NARRATIVEARSENAL_API UAsyncAction_PlayNarrativeSequence : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	/** Execute the actual load */
	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "Sequences"))
	static UAsyncAction_PlayNarrativeSequence* PlayNarrativeSequence(ANarrativeLevelSequenceActor* SequenceActor, ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings InSettings);


	UPROPERTY(BlueprintAssignable)
	FOnMovieSceneSequencePlayerEvent OnFinished;
	/** A stopped, replaced, destroyed or invalid sequence does not emit successful completion. */
	UPROPERTY(BlueprintAssignable) FOnMovieSceneSequencePlayerEvent OnInterrupted;

	UFUNCTION()
	virtual void Finished();
	UFUNCTION() void Stopped();
	UFUNCTION() void ActorDestroyed(AActor* Actor);
	UFUNCTION() void Interrupted();
	void FinishAction(bool bSuccessful);
	bool bCompleted = false;
	uint64 ExpectedPlaybackGeneration = 0;

	FNarrativeSequencePlaybackSettings PlaybackSettings;
	TWeakObjectPtr<ULevelSequence> Sequence;
	TWeakObjectPtr<ANarrativeLevelSequenceActor> LevelSequenceActor;
};

//This handy node will blend out a narrative sequence and notify you when blended out.
UCLASS()
class NARRATIVEARSENAL_API UAsyncAction_BlendOutNarrativeSequence : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	/** Execute the actual load */
	virtual void Activate() override;

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "Sequences"))
	static UAsyncAction_BlendOutNarrativeSequence* BlendOutNarrativeSequence(ANarrativeLevelSequenceActor* SequenceActor);


	UPROPERTY(BlueprintAssignable)
	FOnMovieSceneSequencePlayerEvent OnFinished;
	UPROPERTY(BlueprintAssignable) FOnMovieSceneSequencePlayerEvent OnInterrupted;

	UFUNCTION()
	virtual void Finished();
	UFUNCTION() void ActorDestroyed(AActor* Actor);
	void FinishAction(bool bSuccessful);
	bool bCompleted = false;


	TWeakObjectPtr<ULevelSequence> Sequence;
	TWeakObjectPtr<ANarrativeLevelSequenceActor> LevelSequenceActor;
};
