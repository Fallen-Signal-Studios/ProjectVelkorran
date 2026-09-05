// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Interaction/InteractableComponent.h"
#include "SovTraversalAnchor.generated.h"
class UBoxComponent;
class ASovPlayerCharacterBase;
class UAbilitySystemComponent;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovTraversalCompleted, bool, bSucceeded, const FText&, Message);

UCLASS()
class PROJECTVELKORRAN_API USovTraversalInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error) override;
protected:
    virtual bool Interact(APawn* Player, UNarrativeInteractionComponent* Interaction) override;
};

/** Authored vault/climb/squeeze polyline over the existing Character capsule and Narrative interaction. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovTraversalAnchor : public AActor
{
    GENERATED_BODY()
public:
    ASovTraversalAnchor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> EntryBounds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovTraversalInteractable> Interactable;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Traversal") FName TraversalId;
    /** Capsule-center points in anchor space, including alignment start and walkable exit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal") TArray<FVector> PathPoints;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal", meta=(ClampMin="0.1",ClampMax="30")) float DurationSeconds = 1.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal") FName RequiredMission;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal") FName RequiredBeat;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal") FGameplayTag RequiredProtagonist;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal") FText ActionText;
    /** Protect the origin before an irreversible route and queue the completed safe exit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Traversal") bool bCheckpointTransition = false;
    UPROPERTY(BlueprintAssignable) FSovTraversalCompleted OnTraversalCompleted;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Traversal") bool RequestTraverse(APawn* Player, FText& Error);
    UFUNCTION(BlueprintPure, Category="Traversal") bool IsTraversalActive() const { return bActive; }
    bool CanTraverse(const APawn* Player, FText& Error) const;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    friend struct FSovWorldTraversalTestAccess;
    bool BuildPath(const ASovPlayerCharacterBase* Player, TArray<FVector>& OutPath, FText& Error) const;
    bool SweepClear(const ASovPlayerCharacterBase* Player, FVector From, FVector To) const;
    bool OwnsPlayer() const;
    void Finish(bool bSuccess, const FText& Message);
    TWeakObjectPtr<ASovPlayerCharacterBase> TraversingPlayer;
    TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;
    TWeakObjectPtr<APlayerController> LockedController;
    FActiveGameplayEffectHandle TraversalWindow;
    TArray<FVector> WorldPath;
    TArray<float> CumulativeDistance;
    FVector Origin = FVector::ZeroVector;
    float Elapsed = 0.f;
    int32 Segment = 1;
    uint8 PreviousMovementMode = 0;
    uint8 PreviousCustomMode = 0;
    bool bActive = false;
    bool bMutating = false;
    bool bOwnInputLock = false;
    bool bOwnMovementMode = false;
    bool bEnding = false;
};
