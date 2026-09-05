// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableComponent.h"
#include "NarrativeSavableActor.h"
#include "SovWorldTransitActor.generated.h"
class UBoxComponent;
class UStaticMeshComponent;
class UNavLinkCustomComponent;
class ULevelStreaming;
class ASovPlayerCharacterBase;
class UAbilitySystemComponent;
class UCharacterMovementComponent;

UENUM(BlueprintType) enum class ESovWorldTransitKind : uint8 { Door, Lift };
UENUM(BlueprintType) enum class ESovWorldTransitState : uint8 { AtOrigin, AtDestination, WaitingForDestination, Moving, Blocked, Broken };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovWorldTransitChanged, ESovWorldTransitState, State, const FText&, Message);

/** Existing Narrative focus/hold/slot pipeline invokes this native commit owner. */
UCLASS()
class PROJECTVELKORRAN_API USovWorldTransitInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp, FText& Error) override;
    virtual FText GetInteractableActionText_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp) const override;
protected:
    virtual bool Interact(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp) override;
};

/** Powered, locked, damageable and checkpointable door/lift. No Blueprint timeline is required. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovWorldTransitActor : public AActor, public INarrativeSavableActor
{
    GENERATED_BODY()
public:
    ASovWorldTransitActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> MovingBody;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> EntryBounds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovWorldTransitInteractable> Interactable;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UNavLinkCustomComponent> OriginLink;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UNavLinkCustomComponent> DestinationLink;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Transit") FName TransitId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") ESovWorldTransitKind Kind = ESovWorldTransitKind::Door;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") FVector DestinationOffset = FVector(0,0,260);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit", meta=(ClampMin="0.1",ClampMax="30")) float TravelSeconds = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit", meta=(ClampMin="1",ClampMax="60")) float StreamingTimeoutSeconds = 20.f;
    /** Existing conventional streaming level package. Empty means both endpoints are already in this world. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") FName DestinationStreamingLevel;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") FName RequiredMission;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") FName RequiredBeat;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") FGameplayTag RequiredProtagonist;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") bool bIrreversibleTransition = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transit") bool bRequiresPower = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing=OnRep_State, Category="Transit") bool bPowered = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing=OnRep_State, Category="Transit") FText LockReason;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing=OnRep_State, Category="Transit", meta=(ClampMin="0")) float StructuralHealth = 100.f;
    UPROPERTY(BlueprintAssignable) FSovWorldTransitChanged OnTransitChanged;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Transit") bool RequestUse(APawn* Player, FText& Error);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Transit") void SetPower(bool bAvailable);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Transit") void SetLockReason(const FText& Reason);
    UFUNCTION(BlueprintPure, Category="Transit") ESovWorldTransitState GetTransitState() const { return State; }
    bool CanUse(const APawn* Player, FText& Error) const;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* Instigator, AActor* Causer) override;
    virtual FGuid GetActorGUID_Implementation() const override;
    virtual void SetActorGUID_Implementation(const FGuid& Guid) override { SaveGuid = Guid; }
    virtual bool ShouldRespawn_Implementation() const override { return false; }
    virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Structure; }
    virtual void Load_Implementation() override;
    virtual void Serialize(FArchive& Ar) override;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
private:
    friend struct FSovWorldTransitTestAccess;
    void ReconcileEndpoint();
    void UpdateLinks();
    void Finish(bool bSuccess, const FText& Message);
    bool OwnsPlayer() const;
    UFUNCTION() void OnRep_State();
    UPROPERTY(SaveGame) FGuid SaveGuid;
    UPROPERTY(SaveGame, ReplicatedUsing=OnRep_State) bool bAtDestination = false;
    UPROPERTY(ReplicatedUsing=OnRep_State) ESovWorldTransitState State = ESovWorldTransitState::AtOrigin;
    UPROPERTY(Transient) TObjectPtr<ULevelStreaming> Streaming;
    TWeakObjectPtr<ASovPlayerCharacterBase> TransitPlayer;
    TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;
    TWeakObjectPtr<APlayerController> LockedController;
    TArray<TWeakObjectPtr<UCharacterMovementComponent>> RiderMovements;
    FActiveGameplayEffectHandle TransitWindow;
    FVector MoveStart = FVector::ZeroVector;
    FVector MoveTarget = FVector::ZeroVector;
    double StartedAt = 0;
    float MoveElapsed = 0;
    bool bOwnInputLock = false;
    bool bMutating = false;
    bool bEnding = false;
};
