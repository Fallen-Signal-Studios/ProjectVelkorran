// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "SovBTTask_UseCombatAbility.generated.h"

class UNarrativeAbilitySystemComponent;
class AAIController;
struct FAbilityEndedData;

/** Native attack task over Narrative's granted combat specs and token lifecycle. */
UCLASS(meta = (DisplayName = "Sovereign: Use Combat Ability"))
class PROJECTVELKORRAN_API USovBTTask_UseCombatAbility : public UBTTaskNode
{
	GENERATED_BODY()
public:
	USovBTTask_UseCombatAbility();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual FString GetStaticDescription() const override;

	/** Empty/invalid key falls back to the controller's current focus. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FBlackboardKeySelector TargetActorKey;

	/** Leave empty to include primary, alternate and every special ability input. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FGameplayTag InputFilter;

	/** Optional float key receives the repertoire's current useful movement range. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FBlackboardKeySelector DesiredRangeKey;

	/** Native/BP payload must end its GAS activation. The task cancels a stuck one. */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.1", Units = "s"))
	float MaximumAbilityDuration = 12.f;

	/** Yield before returning "no ready attack" so looping combat branches do not spin. */
	UPROPERTY(EditAnywhere, Category = "Combat", meta = (ClampMin = "0.01", Units = "s"))
	float NoReadyRetryDelay = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	bool bCancelAbilityOnAbort = true;

private:
	void RestoreFocus();
	void UnbindAbilityEnd();
	void HandleSelectedAbilityEnded(const FAbilityEndedData& Data);
	void CancelOwnedAttack();
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> ActiveASC;
	TWeakObjectPtr<AActor> AttackTarget;
	TWeakObjectPtr<AAIController> AttackController;
	TWeakObjectPtr<AActor> PreviousFocus;
	FGameplayAbilitySpecHandle ActiveHandle;
	double StartTime = 0.0;
	FDelegateHandle AbilityEndDelegate;
	TMap<FGameplayAbilitySpecHandle, bool> EndedDuringSelection;
	bool bChoosingAttack = false;
	bool bWaitingForRetry = false;
	bool bObservedEnd = false;
	bool bObservedCancellation = false;
};
