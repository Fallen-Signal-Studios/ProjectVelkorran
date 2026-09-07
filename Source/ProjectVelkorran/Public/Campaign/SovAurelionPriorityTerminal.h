// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Campaign/SovAurelionPrioritySupport.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableComponent.h"
#include "SovAurelionPriorityTerminal.generated.h"

class ASovPlayerCharacterBase;
class ASovPlayerController;
class UNarrativeAbilitySystemComponent;
class USovCampaignDefinition;
class USovCampaignStateComponent;
struct FSovCampaignJournalEntry;

UCLASS()
class PROJECTVELKORRAN_API USovAurelionPriorityInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error) override;
    virtual FText GetInteractableActionText_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction) const override;
protected:
    virtual bool Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction) override;
};

/** Place two labeled options at Z07's physical recovery panel. Narrative owns focus/hold input. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionPriorityTerminal : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionPriorityTerminal();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Aurelion") ESovAurelionRescuePriority Priority = ESovAurelionRescuePriority::Unset;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<class UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Aurelion") TObjectPtr<USovAurelionPriorityInteractable> Interactable;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Aurelion") FText LastResult;
    FText ActionText() const;
    bool CanUse(const APawn* Pawn, FText& Error) const;
    bool RequestUse(APawn* Pawn, FText& Error);
    UFUNCTION(BlueprintPure, Category="Aurelion") bool IsRequestPending() const { return bPending || bExecuting; }
protected:
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    struct FRequest
    {
        TWeakObjectPtr<ASovPlayerCharacterBase> Pawn;
        TWeakObjectPtr<ASovPlayerController> Controller;
        TWeakObjectPtr<UNarrativeAbilitySystemComponent> ASC;
        TWeakObjectPtr<USovCampaignStateComponent> State;
        TWeakObjectPtr<USovCampaignDefinition> Mission;
        TWeakObjectPtr<USovAurelionPriorityInteractable> Source;
        TWeakObjectPtr<ASovAurelionPrioritySupport> Support;
        ESovAurelionRescuePriority Priority = ESovAurelionRescuePriority::Unset;
        int32 ReadyEpoch = 0;
        uint64 TransitionEpoch = 0;
        uint64 ActorInfoEpoch = 0;
        int32 JournalSize = 0;
        uint64 StateEpoch = 0;
    };
    bool Validate(const APawn* Pawn, FText& Error, bool bDuringExecution) const;
    bool OwnsRequest(const FRequest& Request) const;
    void Execute(FRequest Request);
    UFUNCTION() void HandleStateRestored(bool bValid);
    TWeakObjectPtr<USovCampaignStateComponent> ObservedState;
    uint64 StateEpoch = 0;
    bool bPending = false, bExecuting = false, bEnding = false;
    FTimerHandle RequestTimer;
};
