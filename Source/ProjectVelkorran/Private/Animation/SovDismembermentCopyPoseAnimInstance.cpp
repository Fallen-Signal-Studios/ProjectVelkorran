// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Animation/SovDismembermentCopyPoseAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"

void FSovDismembermentCopyPoseAnimInstanceProxy::Initialize(
	UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);

	FAnimationInitializeContext InitializeContext(this);
	CopyPoseFromMesh.bUseAttachedParent = false;
	CopyPoseFromMesh.bCopyCurves = true;
	CopyPoseFromMesh.bCopyCustomAttributes = true;
	CopyPoseFromMesh.bUseMeshPose = true;
	CopyPoseFromMesh.Initialize_AnyThread(InitializeContext);
}

void FSovDismembermentCopyPoseAnimInstanceProxy::PreUpdate(
	UAnimInstance* InAnimInstance,
	const float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const USovDismembermentCopyPoseAnimInstance* CopyPoseInstance =
		Cast<USovDismembermentCopyPoseAnimInstance>(InAnimInstance);
	CopyPoseFromMesh.SourceMeshComponent = IsValid(CopyPoseInstance)
		? CopyPoseInstance->GetSourceMeshComponent()
		: nullptr;
	CopyPoseFromMesh.PreUpdate(InAnimInstance);
}

void FSovDismembermentCopyPoseAnimInstanceProxy::UpdateAnimationNode(
	const FAnimationUpdateContext& InContext)
{
	CopyPoseFromMesh.Update_AnyThread(InContext);
}

bool FSovDismembermentCopyPoseAnimInstanceProxy::Evaluate(
	FPoseContext& Output)
{
	CopyPoseFromMesh.Evaluate_AnyThread(Output);
	return true;
}

USovDismembermentCopyPoseAnimInstance::USovDismembermentCopyPoseAnimInstance(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseMultiThreadedAnimationUpdate = true;
	bUsingCopyPoseFromMesh = true;
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void USovDismembermentCopyPoseAnimInstance::SetSourceMeshComponent(
	USkeletalMeshComponent* InSourceMeshComponent)
{
	SourceMeshComponent = InSourceMeshComponent;
}

USkeletalMeshComponent*
USovDismembermentCopyPoseAnimInstance::GetSourceMeshComponent() const
{
	return SourceMeshComponent.Get();
}

FAnimInstanceProxy*
USovDismembermentCopyPoseAnimInstance::CreateAnimInstanceProxy()
{
	return new FSovDismembermentCopyPoseAnimInstanceProxy(this);
}
