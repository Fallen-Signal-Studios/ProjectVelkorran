// Copyright Narrative Tools 2025.


#include "GAS/AbilityTasks/AbilityTask_RotateActor.h"

#include "Kismet/KismetMathLibrary.h"

UAbilityTask_RotateActor::UAbilityTask_RotateActor(): DesiredRotation(), RotationInterpSpeed(0)
{
	bTickingTask = true;
}

UAbilityTask_RotateActor* UAbilityTask_RotateActor::RotateActorOverTime(UGameplayAbility* OwningAbility,
                                                                        const FRotator& TargetRotation, float InterpSpeed)
{
	auto MyObj = NewAbilityTask<UAbilityTask_RotateActor>(OwningAbility);
	MyObj->DesiredRotation = TargetRotation;
	MyObj->RotationInterpSpeed = InterpSpeed;

	return MyObj;
}

void UAbilityTask_RotateActor::Activate()
{
	Super::Activate();
	
	if (!GetAvatarActor())
	{
		EndTask();
		return;
	}
}

void UAbilityTask_RotateActor::OnDestroy(bool AbilityEnding)
{
	Super::OnDestroy(AbilityEnding);
}

void UAbilityTask_RotateActor::TickTask(float DeltaTime)
{
	AActor* Avatar = GetAvatarActor();
	if (!Avatar)
	{
		EndTask();
		return;
	}

	// Calculate the alpha based on how much time as passed and how long the rotation should be taking
	FRotator NewRotator = FMath::RInterpConstantTo(Avatar->GetActorRotation(), DesiredRotation, DeltaTime, RotationInterpSpeed);
	
	Avatar->SetActorRotation(NewRotator);

	if (NewRotator.Equals(DesiredRotation))
	{
		OnCompleted.Broadcast();
		EndTask();
		return;
	}
}
