// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Interaction/InteractableComponent.h"
#include "NarrativeSavableComponent.h"
#include "SovCarryTargetComponent.generated.h"
class ASovPlayerCharacterBase;
class ASovRescueDestination;
class UAbilitySystemComponent;
class UNPCActivityComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCarryChanged, bool, bCarried, bool, bRescued);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovCarryRecoveryRequired, const FText&, Reason);

/** Carry/drag an existing Narrative savable prop or living NPC. Pose and offset are authored presentation. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCarryTargetComponent : public UNarrativeInteractableComponent, public INarrativeSavableComponent
{
    GENERATED_BODY()
public:
    USovCarryTargetComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry") FName CarryTargetId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry") FName RequiredMission;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry") FGameplayTag RequiredProtagonist;
    /** Local capsule-space offset; negative X supports an authored drag pose. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry") FVector CarryOffset = FVector(0,0,90);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Carry", meta=(ClampMin="1",ClampMax="300")) float MaximumCarrySeconds = 120.f;
    UPROPERTY(BlueprintAssignable) FSovCarryChanged OnCarryChanged;
    UPROPERTY(BlueprintAssignable) FSovCarryRecoveryRequired OnRecoveryRequired;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Carry") bool RequestCarry(APawn* Player, FText& Error);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Carry") bool RequestRelease(APawn* Player, FText& Error);
    UFUNCTION(BlueprintPure, Category="Carry") bool IsCarried() const { return bCarried; }
    UFUNCTION(BlueprintPure, Category="Carry") bool IsRescued() const { return bRescued; }
    UFUNCTION(BlueprintPure, Category="Carry") bool HasUnresolvedPlacement() const { return bPlacementUnresolved; }
    ASovPlayerCharacterBase* GetCarrier() const { return bCarried ? Carrier.Get() : nullptr; }
    bool CanCarry(const APawn* Player, FText& Error) const;
    bool CanPlaceAt(const FVector& Center) const;
    float GetPlacementHalfHeight() const;
    virtual bool CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error) override;
    virtual FText GetInteractableActionText_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction) const override;
    virtual void Serialize(FArchive& Ar) override;
    virtual void Load_Implementation() override;
protected:
    virtual bool Interact(APawn* Player, UNarrativeInteractionComponent* Interaction) override;
    virtual void BeginPlay() override;
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    friend class ASovRescueDestination;
    friend struct FSovCarryTestAccess;
    bool OwnsCarrier() const;
    bool SweepBody(FVector Start, FVector End) const;
    bool FindSafeRelease(FVector& Center) const;
    bool ReleaseAt(const FVector& Center, bool bNotify);
    void CancelCarry();
    UPROPERTY(SaveGame) bool bRescued = false;
    TWeakObjectPtr<ASovPlayerCharacterBase> Carrier;
    TWeakObjectPtr<UAbilitySystemComponent> CarrierASC;
    TWeakObjectPtr<UAbilitySystemComponent> TargetASC;
    TWeakObjectPtr<UNPCActivityComponent> PausedActivity;
    FActiveGameplayEffectHandle CarrierWindow;
    FActiveGameplayEffectHandle TargetWindow;
    FTransform Origin = FTransform::Identity;
    FVector PreviousBodyCenter = FVector::ZeroVector;
    float BodyRadius = 1.f;
    float BodyHalfHeight = 1.f;
    double StartedAt = 0.;
    uint64 LeaseEpoch = 0;
    int32 PreviousInteractionPriority = 0;
    uint8 PreviousTargetMode = 0;
    uint8 PreviousTargetCustomMode = 0;
    bool bCarried = false;
    bool bPlacementUnresolved = false;
    bool bMutating = false;
    bool bReleasing = false;
    bool bOwnTargetMovement = false;
    bool bOwnActivityPause = false;
    bool bOriginalCollision = true;
};
