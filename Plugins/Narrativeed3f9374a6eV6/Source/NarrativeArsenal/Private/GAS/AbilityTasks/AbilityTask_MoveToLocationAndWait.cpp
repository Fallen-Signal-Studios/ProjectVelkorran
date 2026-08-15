// Copyright Narrative Tools 2025.


#include "GAS/AbilityTasks/AbilityTask_MoveToLocationAndWait.h"

#include "ArsenalStatics.h"
#include "NavigationSystem.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Navigation/PathFollowingComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogAbilityTaskMoveToLocationAndWait, Log, All)

UAbilityTask_MoveToLocationAndWait* UAbilityTask_MoveToLocationAndWait::MoveToLocationAndWait(UGameplayAbility* OwningAbility, const FVector& TargetLocation)
{
	UAbilityTask_MoveToLocationAndWait* MyObj = NewAbilityTask<UAbilityTask_MoveToLocationAndWait>(OwningAbility);
	MyObj->Destination = TargetLocation;

	return MyObj;
}

void UAbilityTask_MoveToLocationAndWait::FinishMove(FAIRequestID FAIRequestID,
                                                    const FPathFollowingResult& PathFollowingResult)
{
	if (PathFollowingResult.Code == EPathFollowingResult::Success)
	{
		EndTask();
		OnCompleted.Broadcast(PathFollowingResult.Code);
	}
	else
	{
		EndTask();
		OnFailed.Broadcast(PathFollowingResult.Code);
	}
}

void UAbilityTask_MoveToLocationAndWait::Activate()
{
	// Allows us to end ability early if the avatar is not valid
	SetWaitingOnAvatar();
	
	auto AvatarPawn = Cast<APawn>(GetAvatarActor());
	Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	
	if (Controller)
	{
		// We need to initialize the PFComp beforehand to catch any early exits within SimpleMoveToLocation
		PFComp = Controller->FindComponentByClass<UPathFollowingComponent>();
		if (PFComp == nullptr)
		{
			PFComp = NewObject<UPathFollowingComponent>(Controller);
			PFComp->RegisterComponentWithWorld(Controller->GetWorld());
			PFComp->Initialize();
		}

		FinishMoveHandle = PFComp->OnRequestFinished.AddUObject(this, &UAbilityTask_MoveToLocationAndWait::FinishMove);
		
		UArsenalStatics::MoveToLocationExtended(Controller, Destination);
	}
	else
	{
		EndTask();
		OnFailed.Broadcast(EPathFollowingResult::Invalid);
		UE_LOG(LogAbilityTaskMoveToLocationAndWait, Error, TEXT("Unable to start movement task due to invalid avatar/controller. Avatar: %s | Controller: %s"), *GetNameSafe(AvatarPawn), *GetNameSafe(Controller))
	}

	Super::Activate();
}

void UAbilityTask_MoveToLocationAndWait::OnDestroy(bool AbilityEnding)
{
	if (PFComp)
	{
		PFComp->OnRequestFinished.Remove(FinishMoveHandle);
	}

	Super::OnDestroy(AbilityEnding);
}