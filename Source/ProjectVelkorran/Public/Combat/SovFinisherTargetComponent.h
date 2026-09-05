// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "NarrativeSavableComponent.h"
#include "SovFinisherTargetComponent.generated.h"
class USovGameplayAbility_Finisher;
class UAbilitySystemComponent;
UENUM(BlueprintType)
enum class ESovFinisherTargetKind : uint8 { Normal, Elite, Boss };
/** Explicit opt-in prevents generic executions from killing canon actors or advancing unknown boss phases. */
UCLASS(ClassGroup=(Sovereign), BlueprintType, meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovFinisherTargetComponent : public UActorComponent, public INarrativeSavableComponent
{
    GENERATED_BODY()
public:
    USovFinisherTargetComponent();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finisher")
    ESovFinisherTargetKind TargetKind = ESovFinisherTargetKind::Normal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finisher", meta=(ClampMin="0", ClampMax="1"))
    float LowHealthThreshold = .2f;
    /** Elites and bosses require a nonempty, live phase tag. Each phase can resolve once per saved enemy. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finisher")
    FGameplayTag RequiredPhaseTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Finisher", meta=(ClampMin="0"))
    float PhaseDamage = 35.f;
    UFUNCTION(BlueprintPure, Category="Finisher")
    bool IsAvailableFor(AActor* Attacker) const;
    UFUNCTION(BlueprintPure, Category="Finisher")
    bool HasResolvedPhase(FGameplayTag Phase) const { return ResolvedPhases.HasTagExact(Phase); }
    virtual void Load_Implementation() override;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class USovGameplayAbility_Finisher;
    bool Reserve(USovGameplayAbility_Finisher* Ability, AActor* Attacker, FGuid& OutLease);
    bool OwnsLease(const USovGameplayAbility_Finisher* Ability, const FGuid& Lease) const;
    bool IsReservedTargetValid(const USovGameplayAbility_Finisher* Ability, const FGuid& Lease, AActor* Attacker) const;
    void Release(const USovGameplayAbility_Finisher* Ability, const FGuid& Lease);
    bool CommitPhase(const USovGameplayAbility_Finisher* Ability, const FGuid& Lease);
    UPROPERTY(SaveGame) FGameplayTagContainer ResolvedPhases;
    TWeakObjectPtr<USovGameplayAbility_Finisher> ReservedBy;
    FGuid Reservation;
    FGameplayTag ReservedPhase;
};
