// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/SovBTTask_UseCombatAbility.h"

#include "AIController.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"

USovBTTask_UseCombatAbility::USovBTTask_UseCombatAbility()
{
	NodeName = TEXT("Use Combat Ability");
	bCreateNodeInstance = true;
	bNotifyTick = true;
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, TargetActorKey), AActor::StaticClass());
	DesiredRangeKey.AddFloatFilter(this, GET_MEMBER_NAME_CHECKED(ThisClass, DesiredRangeKey));
	TargetActorKey.AllowNoneAsValue(true);
	DesiredRangeKey.AllowNoneAsValue(true);
}

void USovBTTask_UseCombatAbility::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	if (const UBlackboardData* Blackboard = GetBlackboardAsset())
	{
		TargetActorKey.ResolveSelectedKey(*Blackboard);
		DesiredRangeKey.ResolveSelectedKey(*Blackboard);
	}
}

EBTNodeResult::Type USovBTTask_UseCombatAbility::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	bWaitingForRetry = false;
	AAIController* Controller = OwnerComp.GetAIOwner();
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!IsValid(Controller) || !Controller->HasAuthority() || !IsValid(Controller->GetPawn())) { return EBTNodeResult::Failed; }
	AActor* Target = Blackboard && !TargetActorKey.SelectedKeyName.IsNone()
		? Cast<AActor>(Blackboard->GetValueAsObject(TargetActorKey.SelectedKeyName)) : Controller->GetFocusActor();
	UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Controller->GetPawn()));
	if (!IsValid(ASC) || !IsValid(Target) || !FMath::IsFinite(MaximumAbilityDuration) || MaximumAbilityDuration <= 0.f)
	{
		return EBTNodeResult::Failed;
	}
	UnbindAbilityEnd();
	ActiveHandle = FGameplayAbilitySpecHandle();
	bObservedEnd = false;
	bObservedCancellation = false;
	EndedDuringSelection.Reset();
	ActiveASC = ASC;
	AbilityEndDelegate = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleSelectedAbilityEnded);
	AttackTarget = Target;
	AttackController = Controller;
	PreviousFocus = Controller->GetFocusActor();
	Controller->SetFocus(Target);
	if (Blackboard && !DesiredRangeKey.SelectedKeyName.IsNone())
	{
		Blackboard->SetValueAsFloat(DesiredRangeKey.SelectedKeyName, ASC->GetBotCombatMovementRange(Target, InputFilter));
	}
	FNarrativeBotAttackCandidate Selected;
	bChoosingAttack = true;
	const bool bActivated = ASC->TryActivateBestBotAttack(Target, InputFilter, Selected);
	bChoosingAttack = false;
	if (!bActivated || !ActiveASC.IsValid() || !ASC->GetWorld())
	{
		RestoreFocus();
		UnbindAbilityEnd();
		ActiveASC.Reset();
		if (!bActivated && OwnerComp.GetWorld())
		{
			bWaitingForRetry = true;
			StartTime = OwnerComp.GetWorld()->GetTimeSeconds();
			return EBTNodeResult::InProgress;
		}
		return EBTNodeResult::Failed;
	}
	ActiveHandle = Selected.Handle;
	if (const bool* Cancelled = EndedDuringSelection.Find(ActiveHandle))
	{
		bObservedEnd = true;
		bObservedCancellation = *Cancelled;
	}
	EndedDuringSelection.Reset();
	StartTime = ASC->GetWorld()->GetTimeSeconds();
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(ActiveHandle);
	if (bObservedEnd || !Spec || !Spec->IsActive())
	{
		RestoreFocus();
		UnbindAbilityEnd();
		ActiveASC.Reset();
		return bObservedCancellation ? EBTNodeResult::Failed : EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::InProgress;
}

void USovBTTask_UseCombatAbility::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, const float DeltaSeconds)
{
	if (bWaitingForRetry)
	{
		const float Delay = FMath::IsFinite(NoReadyRetryDelay) ? FMath::Max(NoReadyRetryDelay, 0.01f) : 0.2f;
		if (!OwnerComp.GetWorld() || OwnerComp.GetWorld()->GetTimeSeconds() - StartTime >= Delay)
		{
			bWaitingForRetry = false;
			FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		}
		return;
	}
	if (bObservedEnd)
	{
		RestoreFocus();
		UnbindAbilityEnd();
		ActiveASC.Reset();
		FinishLatentTask(OwnerComp, bObservedCancellation ? EBTNodeResult::Failed : EBTNodeResult::Succeeded);
		return;
	}
	UNarrativeAbilitySystemComponent* ASC = ActiveASC.Get();
	const bool bContextLost = !IsValid(ASC) || !AttackTarget.IsValid() || !AttackController.IsValid()
		|| AttackController->GetPawn() != (ASC ? ASC->GetAvatarActor() : nullptr)
		|| (ASC && !ASC->IsBotAttackExecutionValid(AttackTarget.Get(), ActiveHandle));
	const bool bTimedOut = IsValid(ASC) && ASC->GetWorld()
		&& ASC->GetWorld()->GetTimeSeconds() - StartTime >= MaximumAbilityDuration;
	if (bContextLost || bTimedOut)
	{
		CancelOwnedAttack();
		RestoreFocus();
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(ActiveHandle);
	if (!Spec || !Spec->IsActive())
	{
		RestoreFocus();
		UnbindAbilityEnd();
		ActiveASC.Reset();
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type USovBTTask_UseCombatAbility::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	bWaitingForRetry = false;
	if (bCancelAbilityOnAbort) { CancelOwnedAttack(); }
	RestoreFocus();
	UnbindAbilityEnd();
	ActiveASC.Reset();
	return EBTNodeResult::Aborted;
}

void USovBTTask_UseCombatAbility::CancelOwnedAttack()
{
	if (UNarrativeAbilitySystemComponent* ASC = ActiveASC.Get())
	{
		if (ActiveHandle.IsValid() && !bObservedEnd) { ASC->CancelAbilityHandle(ActiveHandle); }
	}
	UnbindAbilityEnd();
	ActiveASC.Reset();
}

void USovBTTask_UseCombatAbility::RestoreFocus()
{
	if (AAIController* Controller = AttackController.Get(); Controller && Controller->GetFocusActor() == AttackTarget.Get())
	{
		if (PreviousFocus.IsValid()) { Controller->SetFocus(PreviousFocus.Get()); }
		else { Controller->ClearFocus(EAIFocusPriority::Gameplay); }
	}
	AttackController.Reset();
	PreviousFocus.Reset();
	AttackTarget.Reset();
}

FString USovBTTask_UseCombatAbility::GetStaticDescription() const
{
	return FString::Printf(TEXT("Target: %s; repertoire: %s; timeout: %.2fs"),
		*TargetActorKey.SelectedKeyName.ToString(), InputFilter.IsValid() ? *InputFilter.ToString() : TEXT("all combat specs"), MaximumAbilityDuration);
}

void USovBTTask_UseCombatAbility::UnbindAbilityEnd()
{
	if (UNarrativeAbilitySystemComponent* ASC = ActiveASC.Get()) { ASC->OnAbilityEnded.Remove(AbilityEndDelegate); }
	AbilityEndDelegate.Reset();
}

void USovBTTask_UseCombatAbility::HandleSelectedAbilityEnded(const FAbilityEndedData& Data)
{
	if (bChoosingAttack) { EndedDuringSelection.Add(Data.AbilitySpecHandle, Data.bWasCancelled); }
	else if (Data.AbilitySpecHandle == ActiveHandle)
	{
		bObservedEnd = true;
		bObservedCancellation = Data.bWasCancelled;
	}
}
