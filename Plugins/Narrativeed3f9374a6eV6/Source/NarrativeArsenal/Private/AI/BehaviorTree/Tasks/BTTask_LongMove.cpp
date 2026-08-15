// Copyright Narrative Tools 2025.


#include "AI/BehaviorTree/Tasks/BTTask_LongMove.h"

#include "AI/AITask_LongMove.h"

UBTTask_LongMove::UBTTask_LongMove()
{
	NodeName = "Long Move Task";
}

EBTNodeResult::Type UBTTask_LongMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	auto LongMoveTask = NewBTAITask<UAITask_LongMove>(OwnerComp);
	
	LongMoveTask->Setup(OwnerComp.GetAIOwner(),
		GoalLocation.GetValue(OwnerComp),
		AcceptanceRadius.GetValue(OwnerComp),
		StopOnOverlap.GetValue(OwnerComp) ? EAIOptionFlag::Enable : EAIOptionFlag::Disable,
		AcceptPartialPath.GetValue(OwnerComp) ? EAIOptionFlag::Enable : EAIOptionFlag::Disable, LockAILogic.GetValue(OwnerComp),
		ProjectGoalOnNavigation.GetValue(OwnerComp) ? EAIOptionFlag::Enable : EAIOptionFlag::Disable,
		RequireNavigableEndLocation.GetValue(OwnerComp) ? EAIOptionFlag::Enable : EAIOptionFlag::Disable);

	return StartGameplayTask(OwnerComp, NodeMemory, *LongMoveTask);
}

EBTNodeResult::Type UBTTask_LongMove::DetermineGameplayTaskResult(UAITask& Task) const
{
	auto LongMoveTask = static_cast<UAITask_LongMove*>(&Task);

	return LongMoveTask->HasMoveFailed() ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
}
