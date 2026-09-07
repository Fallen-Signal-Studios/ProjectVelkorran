// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "SovCampaignRelayReceiver.generated.h"

class ASovCampaignEncounterObjective;
class ASovPlayerCharacterBase;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** The existing Narrative focus/hold/dispatch path supplies the physical interaction. */
UCLASS()
class PROJECTVELKORRAN_API USovCampaignRelayInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error) override;
    virtual FText GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const override;
protected:
    virtual bool Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction) override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovRelayDisabledChanged, bool, bDisabled);

/** A physical receiver with a native attempt receipt, never a writable disabled flag.
 * Final disabled state derives from the existing campaign journal; partial attempts reset on retry. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCampaignRelayReceiver : public AActor
{
    GENERATED_BODY()
public:
    ASovCampaignRelayReceiver();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovCampaignRelayInteractable> Interactable;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Relay") FName ReceiverId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Relay") TObjectPtr<ASovCampaignEncounterObjective> EncounterObjective;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Relay") FText LastError;
    UPROPERTY(BlueprintAssignable, Category="Relay") FSovRelayDisabledChanged OnDisabledChanged;
    UFUNCTION(BlueprintPure, Category="Relay") bool IsDisabled() const;
    UFUNCTION(BlueprintPure, Category="Relay") bool IsRequestPending() const { return bPending || bExecuting; }
    bool CanUse(const APawn* Pawn, FText& Error) const;
    bool RequestUse(APawn* Pawn, FText& Error);
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    friend class ASovCampaignEncounterObjective;
    struct FDisableRequest
    {
        TWeakObjectPtr<ASovPlayerCharacterBase> Player;
        TWeakObjectPtr<ASovCampaignEncounterObjective> Objective;
        TWeakObjectPtr<USovCampaignRelayInteractable> Interactable;
        FName ReceiverId;
        FGuid AttemptId;
    };
    bool CanUseInternal(const APawn* Pawn, FText& Error, bool bExecutingRequest) const;
    bool OwnsRequest(const FDisableRequest& Request) const;
    bool HasPhysicalDisableReceipt(const ASovCampaignEncounterObjective* Objective, const FGuid& AttemptId) const;
    void ExecuteRequest(FDisableRequest Request);
    void RetireRequest();
    void RefreshPresentation();
    void BindCampaignState();
    UFUNCTION() void HandleCampaignRestored(bool bValid);
    UFUNCTION() void HandleMissionChanged(FName MissionId, bool bSucceeded);
    UFUNCTION() void HandleBeatCommitted(const FSovCampaignJournalEntry& Entry);
    TWeakObjectPtr<USovCampaignStateComponent> BoundCampaign;
    FDisableRequest ActiveRequest;
    FTimerHandle RequestTimer;
    bool bPending = false, bExecuting = false, bEnding = false, bPresentedDisabled = false, bHasPresentedState = false;
};
