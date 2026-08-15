// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Navigation/PathFollowingComponent.h"
#include "AbilityTask_MoveToLocationAndWait.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFinishMoveTask, EPathFollowingResult::Type, Result);

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UAbilityTask_MoveToLocationAndWait : public UAbilityTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FFinishMoveTask	OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FFinishMoveTask	OnFailed;

	UPROPERTY()
	UPathFollowingComponent* PFComp;
	
	FDelegateHandle FinishMoveHandle;

	FVector Destination;

	UPROPERTY()
	AController* Controller;

	/**
	 * Have the avatar actor move to the location specified. On success or failed, will also return a Result outlining the reason for the output.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_MoveToLocationAndWait* MoveToLocationAndWait(UGameplayAbility* OwningAbility, const FVector& TargetLocation);

	void CustomMoveToLocation(AController* InController, const FVector& InDestination);

	void FinishMove(FAIRequestID FAIRequestID, const FPathFollowingResult& PathFollowingResult);

	virtual void Activate() override;

	virtual void OnDestroy(bool AbilityEnding) override;
};
