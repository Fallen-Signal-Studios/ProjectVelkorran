// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "GAS/SovCombatTypes.h"
#include "SovCombatFeedbackComponent.generated.h"

class UNarrativeAbilitySystemComponent;
class UNiagaraComponent;
class UNiagaraSystem;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class ESovCombatFeedback : uint8 { None, TarrikImpact, SeleneImpact, Break };

/** Cosmetic policy only. Native receipt/ownership validation precedes selection. */
struct PROJECTVELKORRAN_API FSovCombatFeedbackPolicy
{
    static ESovCombatFeedback Select(const FSovDamageResult& Result, bool bSelene);
    static bool IsCritical(ESovCombatFeedback Kind) { return Kind == ESovCombatFeedback::Break; }
    static bool Admit(double Now, double& Last, ESovCombatFeedback Kind, int32 LocalCount, int32 WorldCount);
    static constexpr int32 LocalRoutineLimit = 4;
    static constexpr int32 LocalTotalLimit = 6;
    static constexpr int32 WorldRoutineLimit = 12;
    static constexpr int32 WorldTotalLimit = 16;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSovCombatFeedbackPresented,
    ESovCombatFeedback, Kind, FVector, Location, bool, bReducedVariant);

/** One bounded cosmetic owner for real protagonist damage transactions, including the companion proxy. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCombatFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USovCombatFeedbackComponent();
    UFUNCTION(BlueprintCallable, Category="Sovereign|Presentation") void SetReducedCombatEffects(bool bReduced) { bReducedCombatEffects = bReduced; }
    UFUNCTION(BlueprintPure, Category="Sovereign|Presentation") bool IsReducedCombatEffects() const;
    UFUNCTION(BlueprintPure, Category="Sovereign|Presentation") int32 GetPresentedCount() const { return PresentedCount; }
    UFUNCTION(BlueprintPure, Category="Sovereign|Presentation") int32 GetBudgetDropCount() const { return BudgetDropCount; }
    UFUNCTION(BlueprintPure, Category="Sovereign|Presentation") int32 GetLiveBurstCount() const;
    UPROPERTY(BlueprintAssignable, Category="Sovereign|Presentation") FSovCombatFeedbackPresented OnFeedbackPresented;
    // Explicit local comfort preference. Effects quality Low also selects the authored reduced variant.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sovereign|Presentation") bool bReducedCombatEffects = false;
    static bool HasAdmissibleNativeReceipt(const FSovDamageResult& Result);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    UFUNCTION() void HandleASCInitialized();
    UFUNCTION() void HandleOutgoing(const FSovDamageResult& Result);
    UFUNCTION() void HandleIncoming(const FSovDamageResult& Result);
    UFUNCTION(NetMulticast, Unreliable) void MulticastFeedback(ESovCombatFeedback Kind, FVector_NetQuantize Location, FVector_NetQuantizeNormal Normal);
    void Dispatch(const FSovDamageResult& Result, bool bIncoming);
    void RetireBurst(TWeakObjectPtr<UNiagaraComponent> Burst);
    void Unbind();
    bool HasCurrentBinding() const;
    UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
    // Soft references are serialized on the native component CDO and therefore discoverable by cook.
    UPROPERTY() TArray<TSoftObjectPtr<UNiagaraSystem>> NormalSystems;
    UPROPERTY() TArray<TSoftObjectPtr<UNiagaraSystem>> ReducedSystems;
    TSharedPtr<FStreamableHandle> LoadHandle;
    struct FLiveBurst { TWeakObjectPtr<UNiagaraComponent> Component; FTimerHandle Timer; bool bCritical = false; };
    TArray<FLiveBurst> LiveBursts;
    double LastAuthorityRoutine = -1.e20, LastAuthorityCritical = -1.e20;
    double LastLocalRoutine = -1.e20, LastLocalCritical = -1.e20;
    int32 PresentedCount = 0, BudgetDropCount = 0;
    bool bEndingPlay = false;
};
