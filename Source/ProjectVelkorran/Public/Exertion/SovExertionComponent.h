// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovExertionProvider.h"
#include "SovExertionComponent.generated.h"

class UAbilitySystemComponent;
class UNarrativeCharacterMovement;
struct FOnAttributeChangeData;

USTRUCT(BlueprintType)
struct FSovExertionProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaximumStamina = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float IdleRegenRate = 32.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RegenDelay = 0.65f;
	/** TDD leaves the active/idle ratio open; this is editable prototype tuning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float ActiveRegenScale = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float CombatSprintDrain = 16.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EvadeCost = 24.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EvadeDistance = 240.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EvadeDuration = 0.32f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float EvadeInvulnerability = 0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float WalkSpeed = 210.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RunSpeed = 540.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float SprintSpeed = 730.f;
	bool IsValid() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSovExertionChanged, float, OldStamina, float, NewStamina, bool, bExhausted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovExertionSpent, float, Amount, float, Remaining);

/** Sole native player Stamina lifecycle. Narrative's existing attributes remain storage. */
UCLASS(ClassGroup = (Sovereign), BlueprintType, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovExertionComponent : public UActorComponent, public ISovExertionProvider
{
	GENERATED_BODY()
public:
	USovExertionComponent();
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Exertion") bool InitializeWithAbilitySystem(UAbilitySystemComponent* AbilitySystem);
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") bool IsInitialized() const;
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") float GetStamina() const;
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") bool CanSpendExertion(float Cost) const override;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Exertion") bool TrySpendExertion(float Cost) override;
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") FSovExertionProfile GetProfile() const;
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") bool IsCombatActive() const;
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") float GetSecondsUntilRegen() const { return RemainingRegenDelay; }
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") bool IsExhausted() const { return GetStamina() <= KINDA_SMALL_NUMBER; }
	UFUNCTION(BlueprintPure, Category = "Sovereign|Exertion") bool CanSprint() const;
	UFUNCTION(BlueprintCallable, Category = "Sovereign|Exertion") void ResetForCheckpoint();
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Exertion") FSovExertionChanged OnStaminaChanged;
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Exertion") FSovExertionSpent OnStaminaSpent;
	UPROPERTY(EditDefaultsOnly, Category = "Sovereign|Exertion") FSovExertionProfile TarrikProfile;
	UPROPERTY(EditDefaultsOnly, Category = "Sovereign|Exertion") FSovExertionProfile SeleneProfile;
	/** Initializes base resource and movement tuning once per avatar before save restoration. */
	UPROPERTY(EditDefaultsOnly, Category = "Sovereign|Exertion") bool bApplyPrototypeDefaults = true;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
private:
	friend struct FSovExertionTestAccess;
	void UpdateExertion(float DeltaTime);
	bool CanMutate() const;
	bool TrySpendScaledCost(float PaidCost);
	bool HasCompetingRegenEffect() const;
	void Uninitialize();
	void RefreshExhaustionTag();
	void HandleStaminaChanged(const FOnAttributeChangeData& Data);
	UPROPERTY(Transient) TObjectPtr<UAbilitySystemComponent> ASC;
	FDelegateHandle StaminaChangedHandle;
	float RemainingRegenDelay = 0.f;
	bool bChangingResource = false;
	bool bOwnsExhaustedTag = false;
};
