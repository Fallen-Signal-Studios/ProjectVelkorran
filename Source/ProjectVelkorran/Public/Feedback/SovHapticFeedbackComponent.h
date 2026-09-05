// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "Feedback/SovPlatformOutputTypes.h"
#include "Feedback/SovHapticPolicy.h"
#include "GAS/SovCombatTypes.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "SovHapticFeedbackComponent.generated.h"

class ANarrativePlayerController;
class UNarrativeAbilitySystemComponent;
class UPlayerInteractionComponent;
class UNarrativeInteractableComponent;
class USovGameUserSettings;

/** Local owned feedback over Unreal's force-feedback output. No replication, polling-generated cues, or device assumptions. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovHapticFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USovHapticFeedbackComponent();
	/** Returns an exact cancellation receipt, or zero if suppressed/invalid/full. At most 16 requests, each <=5 seconds. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Feedback") int64 PlayFeedback(ESovHapticChannel Channel, float Intensity, float Duration, int32 Priority = 0);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Feedback") bool CancelFeedback(int64 Receipt);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Feedback") void CancelAllFeedback();
	void RefreshSources();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
	virtual void Deactivate() override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual bool CanOutput(ESovHapticChannel Channel) const;
	virtual FSovHapticSettings ReadFeedbackSettings() const;
	virtual double FeedbackTime() const;
	/** One short-lived native handle per channel. Tests override this seam without touching hardware. */
	virtual void SubmitChannel(ESovHapticChannel Channel, float Intensity);
private:
	void FlushOutput();
	void UnbindSources();
	bool HasCurrentPawn() const;
	UFUNCTION() void HandleDamage(const FSovDamageResult& Result);
	UFUNCTION() void HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead);
	UFUNCTION() void HandleReadiness(int32 ReadyEpoch);
	UFUNCTION() void HandleInteraction(AActor* Actor, UNarrativeInteractableComponent* Interactable);
	UFUNCTION() void HandleSequencePlay(ANarrativeLevelSequenceActor* Sequence, const FNarrativeSequencePlaybackSettings& Settings);
	UFUNCTION() void HandleSequenceStop(ANarrativeLevelSequenceActor* Sequence, const FNarrativeSequencePlaybackSettings& Settings);
	UFUNCTION() void HandleSettings(const FSovHapticSettings& Settings);
	TWeakObjectPtr<ANarrativePlayerController> BoundController;
	TWeakObjectPtr<APawn> BoundPawn;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	TWeakObjectPtr<UPlayerInteractionComponent> BoundInteraction;
	TWeakObjectPtr<USovGameUserSettings> BoundSettings;
	TWeakObjectPtr<ANarrativeLevelSequenceActor> FeedbackSequence;
	TSet<int64> SequenceReceipts;
	int32 BoundReadyEpoch = 0;
	bool bEnding = false;
	SovHapticPolicy::Mixer Mixer;
	FDynamicForceFeedbackHandle OutputHandles[SovHapticPolicy::ChannelCount] = {};
	double LastOutputTimes[SovHapticPolicy::ChannelCount] = {};
	float LastOutputIntensities[SovHapticPolicy::ChannelCount] = {};
};
