#include "SovBlueprintAuthoringLibrary.h"
#include "Editor.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/Skeleton.h"

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::AuthorBracedCastClip(
    UObject* Asset, UObject* Source, float Duration, float ReleaseTime, bool bSecondVariant)
{
    FSovBlueprintAuthoringResult Result;
    auto* Clip = Cast<UAnimSequence>(Asset);
    auto* Idle = Cast<UAnimSequence>(Source);
    if (!GEditor || GEditor->PlayWorld || !Clip || !Idle || Clip == Idle ||
        !Clip->GetPathName().StartsWith(TEXT("/Game/Characters/Animation/ProtagonistCasts/")) ||
        !Idle->GetSkeleton() || Clip->GetSkeleton() != Idle->GetSkeleton() || Idle->IsValidAdditive() ||
        Duration < .2f || Duration > 2.f || ReleaseTime < 0.f || ReleaseTime > Duration - .1f)
    { Result.Report = TEXT("Requires matching project clip and non-additive weapon stance."); return Result; }
    const FReferenceSkeleton& Skeleton = Idle->GetSkeleton()->GetReferenceSkeleton();
    const int32 Spine = Skeleton.FindBoneIndex(TEXT("spine_03"));
    if (Spine == INDEX_NONE) { Result.Report = TEXT("Missing mannequin upper spine."); return Result; }
    const int32 Frames = FMath::RoundToInt(Duration * 30.f);
    Clip->Modify();
    IAnimationDataController& Controller = Clip->GetController();
    Controller.OpenBracket(NSLOCTEXT("SovCastAuthoring", "Brace", "Author braced Echo discharge"));
    Controller.InitializeModel();
    Controller.RemoveAllBoneTracks();
    Controller.SetFrameRate(FFrameRate(30, 1));
    Controller.SetNumberOfFrames(FFrameNumber(Frames));
    for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
    {
        FTransform Stance;
        Idle->GetBoneTransform(Stance, FSkeletonPoseBoneIndex(Bone), FAnimExtractContext(0.0), true);
        TArray<FVector3f> Positions, Scales;
        TArray<FQuat4f> Rotations;
        for (int32 Frame = 0; Frame <= Frames; ++Frame)
        {
            FTransform Pose = Stance;
            if (Bone == Spine)
            {
                const float Time = Frame / 30.f;
                const float Peak = FMath::Max(ReleaseTime, .08f);
                const float Envelope = Time <= Peak ? Time / Peak :
                    FMath::Max(0.f, 1.f - (Time - Peak) / FMath::Max(Duration - Peak, .1f));
                const FQuat Offset = FRotator(-6.f * Envelope,
                    (bSecondVariant ? -3.f : 3.f) * Envelope, (bSecondVariant ? 2.f : -2.f) * Envelope).Quaternion();
                Pose.SetRotation((Pose.GetRotation() * Offset).GetNormalized());
            }
            Positions.Add(FVector3f(Pose.GetTranslation()));
            Rotations.Add(FQuat4f(Pose.GetRotation()));
            Scales.Add(FVector3f(Pose.GetScale3D()));
        }
        Controller.AddBoneCurve(Skeleton.GetBoneName(Bone));
        Controller.SetBoneTrackKeys(Skeleton.GetBoneName(Bone), Positions, Rotations, Scales);
    }
    Controller.NotifyPopulated();
    Controller.CloseBracket();
    Clip->Notifies.Reset();
    Clip->bEnableRootMotion = false;
    Clip->bForceRootLock = true;
    Clip->PostEditChange();
    Result.bSucceeded = true;
    Result.Report = TEXT("Weapon grip retained from stance; opposing upper-body brace/recoil variants baked at 30 fps.");
    return Result;
}

FSovBlueprintAuthoringResult USovBlueprintAuthoringLibrary::ConfigureProtagonistCastMontage(
    UObject* Asset, UObject* Sequence, float StartTime, float EndTime, float PlayRate)
{
    FSovBlueprintAuthoringResult Result;
    auto* Montage = Cast<UAnimMontage>(Asset);
    auto* Clip = Cast<UAnimSequence>(Sequence);
    const FString Root(TEXT("/Game/Characters/Animation/ProtagonistCasts/"));
    if (!GEditor || GEditor->PlayWorld || !Montage || !Clip ||
        !Montage->GetPathName().StartsWith(Root) || !Clip->GetPathName().StartsWith(Root) ||
        Montage->GetSkeleton() != Clip->GetSkeleton() || Clip->IsValidAdditive() ||
        StartTime < 0.f || EndTime <= StartTime || EndTime > Clip->GetPlayLength() ||
        !FMath::IsFinite(PlayRate) || PlayRate <= 0.f)
    { Result.Report = TEXT("Requires stopped PIE, matching project non-additive clips and valid range/rate."); return Result; }
    Clip->Modify();
    Clip->Notifies.Reset();
    Clip->bEnableRootMotion = false;
    Clip->bForceRootLock = true;
    Clip->RefreshCacheData();
    Clip->PostEditChange();
    Montage->Modify();
    Montage->Notifies.Reset();
    Montage->CompositeSections.Reset();
    Montage->SlotAnimTracks.Reset();
    FSlotAnimationTrack& Track = Montage->SlotAnimTracks.AddDefaulted_GetRef();
    Track.SlotName = TEXT("FullBody");
    FAnimSegment& Segment = Track.AnimTrack.AnimSegments.AddDefaulted_GetRef();
    Segment.SetAnimReference(Clip);
    Segment.AnimStartTime = StartTime;
    Segment.AnimEndTime = EndTime;
    Segment.AnimPlayRate = PlayRate;
    Segment.LoopingCount = 1;
    Montage->SetCompositeLength((EndTime - StartTime) / PlayRate);
    Montage->AddAnimCompositeSection(TEXT("Cast"), 0.f);
    Montage->BlendIn.SetBlendTime(0.08f);
    Montage->BlendOut.SetBlendTime(0.15f);
    Montage->bEnableAutoBlendOut = true;
    Montage->UpdateLinkableElements();
    Montage->RefreshCacheData();
    Montage->PostEditChange();
    Result.bSucceeded = true;
    Result.Report = TEXT("Single FullBody cast; sequence/montage notifies cleared; root locked; automatic blend out.");
    return Result;
}
