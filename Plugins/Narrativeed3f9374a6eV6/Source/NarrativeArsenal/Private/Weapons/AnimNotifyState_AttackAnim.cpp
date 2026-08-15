// Copyright Narrative Tools 2025.

#include "Weapons/AnimNotifyState_AttackAnim.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNotifyQueue.h"

UAnimNotifyState_AttackAnim::UAnimNotifyState_AttackAnim(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UAnimNotifyState_AttackAnim::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	//Epic appear to intentionally cull event reference in the base class for branching points. We've overriden to add that back, as we need Branching Points for attack animations as they appear much more reliable at lower server tick rates. 
	const FAnimNotifyEventReference EventReference = FAnimNotifyEventReference(BranchingPointPayload.NotifyEvent, BranchingPointPayload.SequenceAsset);
	NotifyBegin(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, BranchingPointPayload.NotifyEvent ? BranchingPointPayload.NotifyEvent->GetDuration() : 0.f, EventReference);
}
