#pragma once

#include "CoreMinimal.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "AnimNode_SovSelenePosture.generated.h"

/** GASP feminine posture over existing locomotion. Weapon overlays retain their own stance. */
USTRUCT(BlueprintInternalUseOnly)
struct PROJECTVELKORRAN_API FAnimNode_SovSelenePosture : public FAnimNode_Base
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="Links")
    FPoseLink BasePose;

    UPROPERTY(EditAnywhere, Category="Posture")
    TObjectPtr<UBlendSpace> Posture = nullptr;

    virtual bool HasPreUpdate() const override { return true; }
    virtual void PreUpdate(const UAnimInstance* Instance) override;
    virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
    virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
    virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
    virtual void Evaluate_AnyThread(FPoseContext& Output) override;
    virtual void GatherDebugData(FNodeDebugData& DebugData) override;
    FString DescribePosture() const
    {
        return FString::Printf(TEXT("eligible=%d alpha=%.4f speed=%.2f crouch=%.4f"), bEligible, Alpha, Speed, Crouch);
    }

private:
    UPROPERTY(Transient)
    FAnimNode_BlendSpacePlayer_Standalone Player;
    float Speed = 0.f;
    float CrouchTarget = 0.f;
    float Crouch = 0.f;
    float Alpha = 0.f;
    bool bEligible = false;
};
