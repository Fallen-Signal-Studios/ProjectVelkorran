// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "BehaviorTree/BTTaskNode.h"
#include "GameplayAbilitySpec.h"
#include "BTTask_SovDominionHandlerCommandHound.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
struct FAbilityEndedData;

enum class ESovDominionHandlerCommandSpecSelection : uint8
{
	Missing,
	UniqueInactive,
	Unavailable,
	Ambiguous,
	IdentityCollision
};

/**
 * Activates exactly the Dominion Handler command ability and waits for its
 * authoritative GAS result.
 *
 * This task deliberately does not use Narrative's generic attack-input or
 * attack-token helpers. Candidate selection, command authorization, and the
 * successful-command cooldown remain owned by the native Handler gameplay
 * code.
 */
UCLASS(meta = (DisplayName = "Sovereign: Command Linked Dominion Hound"))
class PROJECTVELKORRAN_API UBTTask_SovDominionHandlerCommandHound
	: public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SovDominionHandlerCommandHound(
		const FObjectInitializer& ObjectInitializer);

	/** True only for the native Handler command contract and its identity tag. */
	static bool IsExactCommandAbilityDefinition(
		const UGameplayAbility* AbilityDefinition);
	static ESovDominionHandlerCommandSpecSelection SelectExactCommandAbilitySpec(
		TConstArrayView<FGameplayAbilitySpec> AbilitySpecs,
		FGameplayAbilitySpecHandle& OutHandle,
		UGameplayAbility*& OutAbilitySource);

	float GetFailureBackoffSeconds() const
	{
		return FailureBackoffSeconds;
	}

	virtual FString GetStaticDescription() const override;

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual EBTNodeResult::Type AbortTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult) override;

private:
	ESovDominionHandlerCommandSpecSelection ResolveExactCommandAbility(
		UAbilitySystemComponent& AbilitySystem,
		FGameplayAbilitySpecHandle& OutHandle,
		UGameplayAbility*& OutAbilitySource) const;

	bool IsFailureBackoffActive(
		const UBehaviorTreeComponent& OwnerComp) const;
	void ArmFailureBackoff(UBehaviorTreeComponent& OwnerComp);
	void BeginObservingAbility(
		UBehaviorTreeComponent& OwnerComp,
		UAbilitySystemComponent& AbilitySystem,
		FGameplayAbilitySpecHandle AbilityHandle);
	void StopObservingAbility();
	void ClearObservedRequest();
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);

	/**
	 * Short retry suppression for structural or cancelled command failures.
	 * A successful command never writes this gate; its gameplay ability owns
	 * the separate success-only cooldown.
	 */
	UPROPERTY(EditAnywhere, Category = "Sovereign|Dominion Handler", meta = (ClampMin = "0.0", Units = "s"))
	float FailureBackoffSeconds = 0.75f;

	TWeakObjectPtr<UBehaviorTreeComponent> ObservedOwnerComp;
	TWeakObjectPtr<UAbilitySystemComponent> ObservedAbilitySystem;
	FGameplayAbilitySpecHandle ObservedAbilityHandle;
	FDelegateHandle AbilityEndedDelegateHandle;
	double FailureRetryNotBefore = 0.0;
	bool bStartedObservedAbility = false;
	bool bInsideTryActivate = false;
	bool bEndedSynchronously = false;
	bool bSynchronousEndWasCancelled = false;
};
