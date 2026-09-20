#include "Animation/AnimNode_SovSelenePosture.h"
#include "Animation/AnimInstance.h"
#include "Animation/BlendSpace.h"
#include "AnimationRuntime.h"
#include "AbilitySystemComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealFramework/NarrativeAnimInstance.h"

void FAnimNode_SovSelenePosture::PreUpdate(const UAnimInstance* Instance)
{
    // UObject state is sampled on the game thread, never from parallel animation evaluation.
    bEligible = false;
    const UNarrativeAnimInstance* Narrative = Cast<UNarrativeAnimInstance>(Instance);
    ANarrativeCharacter* Character = Narrative ? Narrative->GetCharacterRef() : nullptr;
    if (!Character) { return; }
    FGameplayTag Identity;
    if (const auto* Hero = Cast<ASovPlayerCharacterBase>(Character)) { Identity = Hero->GetProtagonistIdentityTag(); }
    else if (const auto* Companion = Cast<ASovProtagonistCompanionCharacter>(Character)) { Identity = Companion->GetCompanionIdentity(); }
    if (Identity != FGameplayTag::RequestGameplayTag(TEXT("Sov.Character.Player.Selene"))) { return; }
    Speed = Character->GetVelocity().Size2D();
    CrouchTarget = Character->bIsCrouched ? 1.f : 0.f;
    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Character->IsAlive() || !Movement || !Movement->IsMovingOnGround() ||
        Character->GetWieldedWeapons().Num() || Character->IsPlayingRootMotion()) { return; }
    if (const auto* ASC = Character->GetAbilitySystemComponent())
    {
        for (const TCHAR* Tag : {TEXT("Narrative.State.Weapon.Equipping"), TEXT("Narrative.State.SequencerControlled"),
            TEXT("Narrative.State.RootMotionControlled"), TEXT("Narrative.State.Movement.InCover")})
        {
            const FGameplayTag Gate = FGameplayTag::RequestGameplayTag(FName(Tag), false);
            if (Gate.IsValid() && ASC->HasMatchingGameplayTag(Gate)) { return; }
        }
    }
    bEligible = true;
}

void FAnimNode_SovSelenePosture::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
    FAnimNode_Base::Initialize_AnyThread(Context);
    Alpha = Crouch = 0.f;
    BasePose.Initialize(Context);
    Player.SetBlendSpace(Posture);
    Player.Initialize_AnyThread(Context);
}

void FAnimNode_SovSelenePosture::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
    BasePose.CacheBones(Context);
    Player.CacheBones_AnyThread(Context);
}

void FAnimNode_SovSelenePosture::Update_AnyThread(const FAnimationUpdateContext& Context)
{
    BasePose.Update(Context);
    Alpha = FMath::FInterpConstantTo(Alpha, bEligible && Posture ? 1.f : 0.f, Context.GetDeltaTime(), 6.f);
    Crouch = FMath::FInterpConstantTo(Crouch, CrouchTarget, Context.GetDeltaTime(), 6.f);
    Player.SetPosition(FVector(Speed, Crouch, 0.f));
    if (FAnimWeight::IsRelevant(Alpha)) { Player.Update_AnyThread(Context.FractionalWeight(Alpha)); }
}

void FAnimNode_SovSelenePosture::Evaluate_AnyThread(FPoseContext& Output)
{
    BasePose.Evaluate(Output);
    if (!FAnimWeight::IsRelevant(Alpha)) { return; }
    FPoseContext Delta(Output, true);
    Player.Evaluate_AnyThread(Delta);
    FAnimationPoseData BaseData(Output);
    const FAnimationPoseData DeltaData(Delta);
    FAnimationRuntime::AccumulateAdditivePose(BaseData, DeltaData, Alpha, AAT_LocalSpaceBase);
    Output.Pose.NormalizeRotations();
}

void FAnimNode_SovSelenePosture::GatherDebugData(FNodeDebugData& DebugData)
{
    DebugData.AddDebugItem(FString::Printf(TEXT("Selene posture %.2f, speed %.1f, crouch %.2f"), Alpha, Speed, Crouch));
    BasePose.GatherDebugData(DebugData);
}
