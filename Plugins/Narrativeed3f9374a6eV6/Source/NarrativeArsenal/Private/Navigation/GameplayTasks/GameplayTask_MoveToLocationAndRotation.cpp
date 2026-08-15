// Copyright Narrative Tools 2025.


#include "Navigation/GameplayTasks/GameplayTask_MoveToLocationAndRotation.h"

	
#if ENABLE_VISUAL_LOG
#include "VisualLogger/VisualLogger.h"
#endif

#include "ArsenalStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "AbilitySystemComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogMoveToLocationAndRotation, Log, All);

UGameplayTask_MoveToLocationAndRotation::UGameplayTask_MoveToLocationAndRotation(const FObjectInitializer& ObjectInitializer):
	Super(ObjectInitializer),
	DesiredLocation(FVector::ZeroVector),
	GoalReachMode(EGoalReachMode::OverlapAgent),
	DesiredRotation(FRotator::ZeroRotator),
	RotationInterpSpeed(0)
{
	bTickingTask = true;
	bFinishedRotating = false; 
}

UGameplayTask_MoveToLocationAndRotation* UGameplayTask_MoveToLocationAndRotation::MoveToLocationAndRotation(
	TScriptInterface<IGameplayTaskOwnerInterface> TaskOwner, const FVector& TargetLocation, const FRotator& TargetRotation,
	float InterpSpeed, EGoalReachMode GoalReachMode, bool bEndTaskWhenFinishedMove)
{
	auto MyObj = NewTask<UGameplayTask_MoveToLocationAndRotation>(TaskOwner);

	if (MyObj)
	{
		MyObj->DesiredLocation = TargetLocation;
		MyObj->DesiredRotation = TargetRotation;
		MyObj->RotationInterpSpeed = InterpSpeed;
		MyObj->GoalReachMode = GoalReachMode;
		MyObj->bEndTaskWhenFinishedMove = bEndTaskWhenFinishedMove;
	}

	return MyObj;
}

UGameplayTask_MoveToLocationAndRotation* UGameplayTask_MoveToLocationAndRotation::MoveToLocationAndRotation(
	ANarrativeCharacter* Character, const FVector& TargetLocation, const FRotator& TargetRotation, float InterpSpeed,
	EGoalReachMode GoalReachMode, bool bEndWhenFinishedMove)
{
	if (!Character)
	{
		UE_LOG(LogMoveToLocationAndRotation, Error, TEXT("Invalid Character class passed in: %s"), *GetNameSafe(Character))
	}
	
	IGameplayTaskOwnerInterface* TaskOwner = ConvertToTaskOwner(*Character);
	if (!TaskOwner)
	{
		UE_LOG(LogMoveToLocationAndRotation, Error, TEXT("Unable to find task owner for Character: %s"), *GetNameSafe(Character))
	}
	
	return MoveToLocationAndRotation(TaskOwner->_getUObject(), TargetLocation, TargetRotation, InterpSpeed, GoalReachMode, bEndWhenFinishedMove);
}

bool UGameplayTask_MoveToLocationAndRotation::HasFinishedMove() const
{
	return bFinishedRotating;
}

void UGameplayTask_MoveToLocationAndRotation::FinishMove(FAIRequestID FailRequestID,
	const FPathFollowingResult& PathFollowingResult)
{

	//Aborting is normal as beginning this task may abort an existing move. Ignore these. 
	if (PathFollowingResult.Code == EPathFollowingResult::Aborted && !bStartedMoving)
	{
		return; 
	}

	// Only listen to our move request
	// We need to make a special case for invalid request because if the target is already at the location, the MoveRequestID var will not have a chance to be set properly.
	if (MoveRequestID != FAIRequestID::InvalidRequest && FailRequestID != MoveRequestID) { return; } 
	
	if (PathFollowingResult.Code == EPathFollowingResult::Success)
	{
		UE_LOG(LogMoveToLocationAndRotation, Verbose, TEXT("%s: Finished move task, begin rotation task."), *GetName())
		bFinishedMoving = true;
	}
	else
	{
		EndTask();
		OnFailed.Broadcast(PathFollowingResult.Code);
	}
}

void UGameplayTask_MoveToLocationAndRotation::Activate()
{
	auto AvatarPawn = Cast<ANarrativeCharacter>(GetAvatarActor());
	auto Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;
	
	if (Controller && AvatarPawn)
	{
		//If player controlled lock movement for duration of the move
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->SetIgnoreMoveInput(true);
		}

		// We need to initialize the PFComp beforehand to catch any early exits within SimpleMoveToLocation
		PFComp = Controller->FindComponentByClass<UPathFollowingComponent>();
		if (PFComp == nullptr)
		{
			PFComp = NewObject<UPathFollowingComponent>(Controller);
			PFComp->RegisterComponentWithWorld(Controller->GetWorld());
			PFComp->Initialize();
		}

		//TODO pass these in via param 
		if (auto* ASC = AvatarPawn->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_PostponePathUpdates);
			ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
		}

		FinishMoveHandle = PFComp->OnRequestFinished.AddUObject(this, &UGameplayTask_MoveToLocationAndRotation::FinishMove);
		
		MoveRequestID = UArsenalStatics::MoveToLocationExtended(Controller, DesiredLocation, false, -1.f, (EPathFollowingReachMode)GoalReachMode);

		if(MoveRequestID == FAIRequestID::InvalidRequest)
		{
			OnFailed.Broadcast(EPathFollowingResult::Invalid);
			EndTask();
			return;
		}

		bStartedMoving = true; 
	}
	else if (AvatarPawn && GetWorld()->GetNetMode() == NM_Client)
	{
		// If we are a client, we cannot tell characters where to move or listen for finish events. We will handle this in the Tick function
	}
	else
	{
		EndTask();
		OnFailed.Broadcast(EPathFollowingResult::Invalid);
		UE_LOG(LogMoveToLocationAndRotation, Error, TEXT("%s: Unable to start movement task due to invalid avatar/controller. Avatar: %s | Controller: %s"), *GetName(), *GetNameSafe(AvatarPawn), *GetNameSafe(Controller))
	}
}

void UGameplayTask_MoveToLocationAndRotation::OnDestroy(bool AbilityEnding)
{
	auto AvatarPawn = Cast<ANarrativeCharacter>(GetAvatarActor());
	auto Controller = AvatarPawn ? AvatarPawn->GetController() : nullptr;

	if (AvatarPawn)
	{
		if (auto* ASC = AvatarPawn->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Movement_PostponePathUpdates);
			ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy);
		}
	}

	if (Controller)
	{
		if (APlayerController* PC = Cast<APlayerController>(Controller))
		{
			PC->SetIgnoreMoveInput(false);
		}
	}

	Super::OnDestroy(AbilityEnding);

	if (PFComp)
	{
		PFComp->OnRequestFinished.Remove(FinishMoveHandle);
	}
}

void UGameplayTask_MoveToLocationAndRotation::TickTask(float DeltaTime)
{
	ANarrativeCharacter* Avatar = Cast<ANarrativeCharacter>(GetAvatarActor());
	
	if (!Avatar)
	{
		UE_LOG(LogMoveToLocationAndRotation, Error, TEXT("%s: Unable to get avatar actor, cannot perform move/rotate"), *GetName());
		EndTask();
		OnFailed.Broadcast(EPathFollowingResult::Invalid);
	}
	
	// On clients, we will simply poll the location and rotation of the avatar actor to see if they reached the destination
	if (GetWorld()->GetNetMode() == NM_Client)
	{
		FRotator AvatarRotation = Avatar->bUseControllerRotationYaw ? Avatar->GetViewRotation() : Avatar->GetActorRotation();
		FVector AvatarLocation = Avatar->GetFloorLocation();
	
#if ENABLE_VISUAL_LOG
		UE_VLOG_SEGMENT_THICK(this, LogMoveToLocationAndRotation, VeryVerbose, AvatarLocation, DesiredLocation, FColor::Red, 5.f, TEXT("Client Loc Diff: %s | Rot Diff: %s"), *(DesiredLocation - AvatarLocation).ToCompactString(), *(DesiredRotation - AvatarRotation).ToCompactString());
#endif
		
		if (AvatarLocation.Equals(DesiredLocation, ClientLocationTolerance) && AvatarRotation.Equals(DesiredRotation, ClientRotationTolerance))
		{
			UE_LOG(LogMoveToLocationAndRotation, Log, TEXT("%s: Client reached desired location and rotation"), *GetNameSafe(Avatar));
			EndTask();
			OnCompleted.Broadcast(EPathFollowingResult::Success);
		}
	}
	
	// Rotate actor once we have finished our move
	if (bFinishedMoving)
	{
		const FRotator CharRot = Avatar->GetActorRotation();

		//We dont care about pitch and roll 
		DesiredRotation.Pitch = CharRot.Pitch;
		DesiredRotation.Roll = CharRot.Roll;

		// Calculate the alpha based on how much time as passed and how long the rotation should be taking
		const FRotator NewRotator = FMath::RInterpConstantTo(CharRot, DesiredRotation, DeltaTime, RotationInterpSpeed);

		//If we're following control rot change that instead of directly setting pawn rot as that won't work. 
		if (Avatar->bUseControllerRotationYaw)
		{
			if (AController* Controller = Avatar->GetController())
			{
				const FRotator ExistingRot = Controller->GetControlRotation();
				Avatar->GetController()->SetControlRotation(FRotator(ExistingRot.Pitch, NewRotator.Yaw, ExistingRot.Roll));
			}
		}

		Avatar->SetActorRotation(NewRotator);

		if (NewRotator.Equals(DesiredRotation))
		{
			FinishedRotating();
		}
	}
}

void UGameplayTask_MoveToLocationAndRotation::FinishedRotating()
{
	bFinishedRotating = true;
	OnCompleted.Broadcast(EPathFollowingResult::Success);

	if (bEndTaskWhenFinishedMove)
	{
		EndTask();
	}
}
