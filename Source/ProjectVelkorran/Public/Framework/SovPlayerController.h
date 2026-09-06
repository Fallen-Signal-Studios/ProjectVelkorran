// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Campaign/SovEncounterTypes.h"
#include "SovPlayerController.generated.h"

class USovCampaignDefinition;
class USovCampaignStateComponent;
class ASovPlayerCharacterBase;

UENUM(BlueprintType)
enum class ESovCampaignTransitionState : uint8 { Idle, Initializing, Switching, Recovering, Travelling, Failed };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCampaignTransitionChanged,
	ESovCampaignTransitionState, State, const FString&, Message);

/** Project ownership seam for campaign input, HUD, possession, and handoffs. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovPlayerController : public ANarrativePlayerController
{
	GENERATED_BODY()

public:
	ASovPlayerController(const FObjectInitializer& ObjectInitializer);
	UFUNCTION(BlueprintPure, Category="Campaign") USovCampaignStateComponent* GetCampaignState() const { return CampaignState; }
	class USovConvergenceCompanionState* GetConvergenceCompanionState() const { return ConvergenceCompanionState; }
	UFUNCTION(BlueprintPure, Category="Narrative") class USovNarrativeCueComponent* GetNarrativeCues() const { return NarrativeCues; }
	UFUNCTION(BlueprintPure, Category="Feedback") class USovHapticFeedbackComponent* GetHapticFeedback() const { return HapticFeedback; }
	UFUNCTION(BlueprintPure, Category="Accessibility") class USovFrontendComponent* GetFrontend() const { return Frontend; }
	UFUNCTION(BlueprintPure, Category="Platform") class USovApplicationLifecycleComponent* GetApplicationLifecycle() const { return ApplicationLifecycle; }
	/** Named pause ownership composes first-boot, save failure and platform interruptions. */
	bool AcquireSystemPause(FName PauseOwner);
	void ReleaseSystemPause(FName PauseOwner);
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;
	UFUNCTION(BlueprintCallable, Category="Accessibility") bool OpenAccessibilitySettings();
	UFUNCTION(BlueprintPure, Category="Campaign") ESovCampaignTransitionState GetCampaignTransitionState() const { return TransitionState; }
	/** Authored handoff after mandatory beats, into content already loaded in this world. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign")
	bool HandoffToMission(USovCampaignDefinition* Destination, const FTransform& SpawnTransform, FString& OutError);
	/** Requires a matching physical authored M12/M13 anchor; there is no free-switch endpoint. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign")
	bool RequestAuthoredHandoff(class ASovCampaignHandoffAnchor* Anchor, FString& OutError);
	FGameplayTag GetPendingProtagonist() const;
	/** Writes a Narrative player record before non-seamless authored map travel. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign")
	bool TravelToMission(USovCampaignDefinition* Destination, FString& OutError);
	UPROPERTY(BlueprintAssignable, Category="Campaign") FSovCampaignTransitionChanged OnCampaignTransitionChanged;

	/** GameMode-only staging: actor bytes first; quest/component records after the matching pawn exists. */
	bool StageCampaignLoad(USovCampaignDefinition* Mission, const FNarrativeSavePlayer* Records, bool bFromTravel, FString& OutError);
	void InitializeCampaignPawn(ASovPlayerCharacterBase* CampaignPawn);
	static bool ValidateMissionPawn(USovCampaignDefinition* Mission, FString& OutError, FGameplayTag Lead = FGameplayTag());
	uint64 GetCampaignTransitionEpoch() const { return TransitionEpoch; }
	static const TCHAR* TravelSaveSlot() { return TEXT("SovCampaignTravel"); }
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	virtual bool ShouldRespawn_Implementation() const override { return false; }

protected:
	virtual bool IsGameplayAbilityInputSuppressed() const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") TObjectPtr<USovCampaignStateComponent> CampaignState;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") TObjectPtr<class USovConvergenceCompanionState> ConvergenceCompanionState;
	UPROPERTY(EditDefaultsOnly, Category="Campaign", meta=(ClampMin="1",ClampMax="120")) float InitializationTimeoutSeconds = 30.f;

private:
	friend class USovSaveSubsystem;
	friend struct FSovLifecycleTestAccess;
	friend struct FSovTransitionCallbackTestAccess;
	UPROPERTY(VisibleAnywhere, Category="Narrative") TObjectPtr<class USovNarrativeCueComponent> NarrativeCues;
	UPROPERTY(VisibleAnywhere, Category="Feedback") TObjectPtr<class USovHapticFeedbackComponent> HapticFeedback;
	UPROPERTY(VisibleAnywhere, Category="Accessibility") TObjectPtr<class USovFrontendComponent> Frontend;
	UPROPERTY(VisibleAnywhere, Category="Dialogue") TObjectPtr<class USovDialoguePresentationComponent> DialoguePresentation;
	UPROPERTY(VisibleAnywhere, Category="Platform") TObjectPtr<class USovApplicationLifecycleComponent> ApplicationLifecycle;
	bool CanReleaseSystemPause() const;
	bool RequestNativePause(FCanUnpause CanUnpauseDelegate);
	TSet<FName> SystemPauseOwners;
	bool bExternalPauseRequested = false;
	bool PrepareTransitionCheckpoint(FName BoundaryId, FString& OutError, bool bRequireDurable = false);
	bool CanTransitionTo(USovCampaignDefinition* Destination, FString& OutError, bool bRequireDifferentProtagonist = true) const;
	ASovPlayerCharacterBase* SpawnCampaignPawn(USovCampaignDefinition* Mission, const FTransform& Transform, FGameplayTag Lead = FGameplayTag());
	bool StartPawnHandoff(USovCampaignDefinition* Destination, FGameplayTag Lead, const FTransform& Transform, FName HandoffBeat, const FGuid& HandoffRequest, FString& OutError);
	bool ClearOutgoingCombatState();
	void PollCampaignInitialization(uint64 ExpectedEpoch);
	void FailCampaignInitialization(const FString& Message);
	void SetTransitionInputLock(bool bLock);
	void SetTransitionState(ESovCampaignTransitionState State, const FString& Message = FString());
	UPROPERTY(SaveGame) FGuid CampaignControllerGuid;
	UPROPERTY(SaveGame) TObjectPtr<USovCampaignDefinition> PendingTravelMission;
	UPROPERTY(SaveGame) FGuid PendingTravelOperationId;
	UPROPERTY(SaveGame) int64 PendingTravelOriginGeneration = 0;
	UPROPERTY(Transient) TObjectPtr<USovCampaignDefinition> PendingMission;
	FGameplayTag PendingProtagonist;
	FName PendingHandoffBeat;
	FGuid PendingHandoffRequest;
	UPROPERTY(Transient) TObjectPtr<USovCampaignDefinition> OriginMission;
	UPROPERTY(Transient) TObjectPtr<ASovPlayerCharacterBase> PendingPawn;
	UPROPERTY(Transient) FNarrativeSavePlayer PendingRecords;
	UPROPERTY(Transient) FNarrativeActorRecord OriginControllerRecord;
	UPROPERTY(Transient) FSovProtagonistSnapshot OriginSnapshot;
	ESovCampaignTransitionState TransitionState = ESovCampaignTransitionState::Idle;
	FTimerHandle InitializationTimer;
	uint64 TransitionEpoch = 0;
	double InitializationDeadline = 0;
	bool bHasPendingRecords = false;
	bool bFromLevelTravel = false;
	bool bHasOriginSnapshot = false;
	bool bOwnInputLock = false;
	bool bWasSavingDisabled = false;
	bool bFailureInProgress = false;
};
