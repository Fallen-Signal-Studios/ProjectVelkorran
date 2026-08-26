// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "SovDismembermentCopyPoseAnimInstance.generated.h"

class USkeletalMeshComponent;

/**
 * Lightweight native Copy Pose proxy used only after a Narrative Leader Pose
 * follower needs an independent bone visibility buffer for dismemberment.
 */
USTRUCT()
struct FSovDismembermentCopyPoseAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FSovDismembermentCopyPoseAnimInstanceProxy() = default;
	explicit FSovDismembermentCopyPoseAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void UpdateAnimationNode(
		const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	FAnimNode_CopyPoseFromMesh CopyPoseFromMesh;
};

/**
 * Native AnimInstance equivalent of a one-node Copy Pose From Mesh AnimBP.
 * This removes the need for a project asset and is assigned only to severed
 * modular presentation meshes.
 */
UCLASS(Transient, NotBlueprintable, NotEditInlineNew)
class USovDismembermentCopyPoseAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	USovDismembermentCopyPoseAnimInstance(
		const FObjectInitializer& ObjectInitializer);

	void SetSourceMeshComponent(USkeletalMeshComponent* InSourceMeshComponent);
	USkeletalMeshComponent* GetSourceMeshComponent() const;

	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	TWeakObjectPtr<USkeletalMeshComponent> SourceMeshComponent;
};
