// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_RotateActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFinishRotateTask);

/**
 * Rotates the actor within a set duration
 */
UCLASS()
class NARRATIVEARSENAL_API UAbilityTask_RotateActor : public UAbilityTask
{
	GENERATED_BODY()

	UAbilityTask_RotateActor();

	UPROPERTY(BlueprintAssignable)
	FFinishRotateTask OnCompleted;

	FRotator DesiredRotation;
	float RotationInterpSpeed;
	
	/**
	 * Rotates the avatar actor to the desired rotation over the set duration.
	 * @param TargetRotation The desired rotation of the avatar actor
	 * @param InterpSpeed the speed, in degrees per second, to rotate the actor
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_RotateActor* RotateActorOverTime(UGameplayAbility* OwningAbility, const FRotator& TargetRotation, float InterpSpeed);

	virtual void Activate() override;

	virtual void OnDestroy(bool AbilityEnding) override;

	virtual void TickTask(float DeltaTime) override;
};
