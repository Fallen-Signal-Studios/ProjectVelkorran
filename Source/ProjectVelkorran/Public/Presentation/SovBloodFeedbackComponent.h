#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "SovBloodFeedbackComponent.generated.h"
class UNarrativeAbilitySystemComponent;
class UNiagaraSystem;
struct FStreamableHandle;

/** Target-owned cosmetic blood, driven only by committed native health damage. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovBloodFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USovBloodFeedbackComponent();
    UPROPERTY(EditDefaultsOnly, Category="Blood") bool bEnabled = true;
    UPROPERTY(EditDefaultsOnly, Category="Blood") bool bBlackBlood = false;
    static bool ShouldPresent(const FSovDamageResult& Result);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void BindASC();
    UFUNCTION() void OnDamage(const FSovDamageResult& Result);
    UFUNCTION(NetMulticast, Unreliable) void MulticastBlood(uint8 Kind, FVector_NetQuantize Position, FVector_NetQuantizeNormal Normal);
    UPROPERTY(Transient) TObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
    UPROPERTY() TArray<TSoftObjectPtr<UNiagaraSystem>> Systems;
    TSharedPtr<FStreamableHandle> LoadHandle;
    double LastBurst = -100.;
};
