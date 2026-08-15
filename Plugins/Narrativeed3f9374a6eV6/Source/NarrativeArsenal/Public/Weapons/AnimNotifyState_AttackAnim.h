// Copyright Narrative Tools 2025.


#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_AttackAnim.generated.h"

UCLASS()
class NARRATIVEARSENAL_API UAnimNotifyState_AttackAnim : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()
	
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	
};

