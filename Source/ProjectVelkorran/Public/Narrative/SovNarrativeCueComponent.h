// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NarrativeSavableComponent.h"
#include "Narrative/SovNarrativeCue.h"
#include "Tales/Dialogue.h"
#include "SovNarrativeCueComponent.generated.h"
class UAudioComponent;
class UTalesComponent;
class ASovPlayerController;

USTRUCT()
struct FSovQueuedCue
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TObjectPtr<USovNarrativeCue> Cue;
	UPROPERTY(SaveGame) FGuid SpeakerGuid;
	UPROPERTY(SaveGame) float RemainingContextSeconds = 0.f;
	UPROPERTY(Transient) TWeakObjectPtr<AActor> Speaker;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSovCueStarted, USovNarrativeCue*, Cue, AActor*, Speaker, const FText&, Caption, float, Seconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCueEnded, USovNarrativeCue*, Cue, bool, bInterrupted);

/** One local speech arbiter. Narrative retains the conversation graph, event execution, audio and subtitle context. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovNarrativeCueComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovNarrativeCueComponent();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Narrative|Cues") bool RequestCue(USovNarrativeCue* Cue, AActor* Speaker, FString& Error);
	UFUNCTION(BlueprintPure, Category="Narrative|Cues") TArray<USovNarrativeCue*> GetUnheardRecords() const;
	UPROPERTY(BlueprintAssignable, Category="Narrative|Cues") FSovCueStarted OnCueStarted;
	UPROPERTY(BlueprintAssignable, Category="Narrative|Cues") FSovCueEnded OnCueEnded;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Presentation; }
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* TickFunction) override;
private:
	friend struct FSovNarrativeCueTestAccess;
	bool ResolveOwner();
	bool IsCombatRequired() const;
	bool MatchesContext(const USovNarrativeCue* Cue) const;
	AActor* ResolveSpeaker(FSovQueuedCue& Request) const;
	bool StartRequest(FSovQueuedCue Request);
	static bool ConfigureControllerOutput(UAudioComponent* Audio, USoundClass* Class, float Volume);
	void StopBark(bool bInterrupted, bool bPreserveCritical = true);
	void RememberUnheard(USovNarrativeCue* Cue);
	UFUNCTION() void HandleDialogueFinished(UDialogue* Dialogue, bool bStartingNew, EExitDialogueReason Reason);
	UFUNCTION() void HandleDialogueBegan(UDialogue* Dialogue);
	UPROPERTY(SaveGame) TArray<FSovQueuedCue> Pending;
	UPROPERTY(SaveGame) TArray<TObjectPtr<USovNarrativeCue>> UnheardRecords;
	UPROPERTY(SaveGame) TMap<FName, int32> RepetitionCounts;
	UPROPERTY(SaveGame) FSovQueuedCue InFlightCriticalSave;
	UPROPERTY(Transient) TObjectPtr<ASovPlayerController> Controller;
	UPROPERTY(Transient) TObjectPtr<UTalesComponent> Tales;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> BarkAudio;
	UPROPERTY(Transient) TObjectPtr<USovNarrativeCue> CurrentBark;
	UPROPERTY(Transient) TObjectPtr<USovNarrativeCue> CurrentConversation;
	UPROPERTY(Transient) TObjectPtr<UDialogue> OwnedDialogue;
	UPROPERTY(Transient) FSovQueuedCue CurrentRequest;
	TMap<FName, double> NextAllowed;
	double BarkEndsAt = 0.;
	uint64 Epoch = 0;
	bool bMutation = false;
	bool bOwnerEndingPlay = false;
	bool bStartingConversation = false;
	bool bBarkUsesControllerOutput = false;
	bool bCompletedDuringStart = false;
};
