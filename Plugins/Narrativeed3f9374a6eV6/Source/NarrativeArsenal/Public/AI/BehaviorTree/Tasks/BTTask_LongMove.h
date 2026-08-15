// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Tasks/BTTask_GameplayTaskBase.h"
#include "BTTask_LongMove.generated.h"

/**
 * Perform AI movement which supports long distances through POI linking
 */
UCLASS()
class NARRATIVEARSENAL_API UBTTask_LongMove : public UBTTask_GameplayTaskBase
{
	GENERATED_BODY()

	UBTTask_LongMove();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual EBTNodeResult::Type DetermineGameplayTaskResult(UAITask& Task) const override;

public:
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Vector GoalLocation;

	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Float AcceptanceRadius;
	
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Bool StopOnOverlap;
	
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Bool AcceptPartialPath;
	
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Bool LockAILogic;

	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Bool ProjectGoalOnNavigation;
	
	UPROPERTY(Category = Node, EditAnywhere)
	FValueOrBBKey_Bool RequireNavigableEndLocation;
};
