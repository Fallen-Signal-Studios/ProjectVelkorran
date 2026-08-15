// Copyright Narrative Tools 2025.


#include "Weapons/WeaponAnimPose.h"

#include "Engine/SkeletalMeshSocket.h"
#include <ReferenceSkeleton.h>
#include <Animation/Skeleton.h>
#include <Engine/SkeletalMesh.h>
#include <BonePose.h>
#include <BoneContainer.h>
#include <Components/SkeletalMeshComponent.h>
#include <Animation/AnimInstance.h>

DEFINE_LOG_CATEGORY_STATIC(LogWeaponAnimPose, Verbose, All);

void FWeaponAnimPose::Init(const FBoneContainer& InBoneContainer)
{
	Reset();

	const FReferenceSkeleton& RefSkeleton = InBoneContainer.GetSkeletonAsset()->GetReferenceSkeleton();
	
	for (const FBoneIndexType BoneIndex : InBoneContainer.GetBoneIndicesArray())
	{			
		const FCompactPoseBoneIndex CompactIndex(BoneIndex);
		const FCompactPoseBoneIndex CompactParentIndex = InBoneContainer.GetParentBoneIndex(CompactIndex);

		const int32 SkeletonBoneIndex = InBoneContainer.GetSkeletonIndex(CompactIndex);
		if (SkeletonBoneIndex != INDEX_NONE)
		{
			const int32 ParentBoneIndex = CompactParentIndex.GetInt() != INDEX_NONE ? InBoneContainer.GetSkeletonIndex(CompactParentIndex) : INDEX_NONE;

			BoneIndices.Add(SkeletonBoneIndex);
			ParentBoneIndices.Add(ParentBoneIndex);

			BoneNames.Add(RefSkeleton.GetBoneName(SkeletonBoneIndex));

			RefLocalSpacePoses.Add(InBoneContainer.GetRefPoseTransform(FCompactPoseBoneIndex(BoneIndex)));
		}
	}

	TArray<bool> Processed;
	Processed.SetNumZeroed(BoneNames.Num());
	RefWorldSpacePoses.SetNum(BoneNames.Num());
	for (int32 EntryIndex = 0; EntryIndex < BoneNames.Num(); ++EntryIndex)
	{
		const int32 ParentIndex = ParentBoneIndices[EntryIndex];
		const int32 TransformIndex = BoneIndices.IndexOfByKey(ParentIndex);

		if (TransformIndex != INDEX_NONE)
		{
			ensure(Processed[TransformIndex]);
			RefWorldSpacePoses[EntryIndex] = RefLocalSpacePoses[EntryIndex] * RefWorldSpacePoses[TransformIndex];
		}
		else
		{
			RefWorldSpacePoses[EntryIndex] = RefLocalSpacePoses[EntryIndex];
		}

		Processed[EntryIndex] = true;
	}
}

void FWeaponAnimPose::GetPose(FCompactPose& InOutCompactPose) const
{
	if (IsValid())
	{
		for (int32 Index = 0; Index < BoneNames.Num(); ++Index)
		{
			const FName& BoneName = BoneNames[Index];
			const int32 MeshBoneIndex = InOutCompactPose.GetBoneContainer().GetPoseBoneIndexForBoneName(BoneName);
			const FCompactPoseBoneIndex PoseBoneIndex = InOutCompactPose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
			if (PoseBoneIndex != INDEX_NONE)
			{
				InOutCompactPose[PoseBoneIndex] = LocalSpacePoses[Index];
			}
		}
	}
}

void FWeaponAnimPose::SetPose(USkeletalMeshComponent* Component)
{
	if (IsInitialized())
	{		
		const FBoneContainer& ContextBoneContainer = Component->GetAnimInstance()->GetRequiredBones();

		LocalSpacePoses.SetNum(RefLocalSpacePoses.Num());

		TArray<FTransform> BoneSpaceTransforms = Component->GetBoneSpaceTransforms();
		for (const FBoneIndexType BoneIndex : ContextBoneContainer.GetBoneIndicesArray())
		{
			const int32 SkeletonBoneIndex = ContextBoneContainer.GetSkeletonIndex(FCompactPoseBoneIndex(BoneIndex));
			LocalSpacePoses[BoneIndices.IndexOfByKey(SkeletonBoneIndex)] = BoneSpaceTransforms[BoneIndex];
		}

		ensure(LocalSpacePoses.Num() == RefLocalSpacePoses.Num());	
		GenerateWorldSpaceTransforms();
	}
}

void FWeaponAnimPose::GenerateWorldSpaceTransforms()
{
	if (IsPopulated())
	{
		TArray<bool> Processed;
		Processed.SetNumZeroed(BoneNames.Num());
		WorldSpacePoses.SetNum(BoneNames.Num());
		for (int32 EntryIndex = 0; EntryIndex < BoneNames.Num(); ++EntryIndex)
		{
			const int32 ParentIndex = ParentBoneIndices[EntryIndex];
			const int32 TransformIndex = BoneIndices.IndexOfByKey(ParentIndex);
			if (TransformIndex != INDEX_NONE)
			{
				ensure(Processed[TransformIndex]);
				WorldSpacePoses[EntryIndex] = LocalSpacePoses[EntryIndex] * WorldSpacePoses[TransformIndex];
			}
			else
			{
				WorldSpacePoses[EntryIndex] = LocalSpacePoses[EntryIndex];
			}

			Processed[EntryIndex] = true;
		}
	}
	else
	{
		UE_LOG(LogWeaponAnimPose, Warning, TEXT("Anim Pose was not previously populated"));
	}
}

void FWeaponAnimPose::SetPose(const FAnimationPoseData& PoseData)
{
	const FCompactPose& CompactPose = PoseData.GetPose();
	if (IsInitialized())
	{
		const FBoneContainer& ContextBoneContainer = CompactPose.GetBoneContainer();
			
		LocalSpacePoses.SetNum(RefLocalSpacePoses.Num());
		for (const FCompactPoseBoneIndex BoneIndex : CompactPose.ForEachBoneIndex())
		{
			const int32 SkeletonBoneIndex = ContextBoneContainer.GetSkeletonIndex(BoneIndex);
			LocalSpacePoses[BoneIndices.IndexOfByKey(SkeletonBoneIndex)] = CompactPose[BoneIndex];
		}

		ensure(LocalSpacePoses.Num() == RefLocalSpacePoses.Num());
		GenerateWorldSpaceTransforms();

		const FBlendedCurve& Curve = PoseData.GetCurve();
		Curve.ForEachElement([&CurveNames = CurveNames, &CurveValues = CurveValues](const UE::Anim::FCurveElement& InElement)
		{
			CurveNames.Add(InElement.Name);
			CurveValues.Add(InElement.Value);
		});

		TArray<USkeletalMeshSocket*> Sockets;
		const USkeleton* Skeleton = ContextBoneContainer.GetSkeletonAsset();
		const USkeletalMesh* SkeletalMesh = ContextBoneContainer.GetSkeletalMeshAsset();
		if (SkeletalMesh)
		{
			Sockets = SkeletalMesh->GetActiveSocketList();
		}
		else if (Skeleton)
		{
			Sockets = Skeleton->Sockets;
		}

		for (const USkeletalMeshSocket* Socket : Sockets)
		{
			const int32 PoseBoneIndex = ContextBoneContainer.GetPoseBoneIndexForBoneName(Socket->BoneName);
			if (PoseBoneIndex != INDEX_NONE)
			{
				SocketNames.Add(Socket->SocketName);
				SocketParentBoneNames.Add(Socket->BoneName);
				SocketTransforms.Add(Socket->GetSocketLocalTransform());
			}
		}		
	}
	else
	{
		UE_LOG(LogWeaponAnimPose, Warning, TEXT("Anim Pose was not previously initialized"));
	}
}

void FWeaponAnimPose::SetToRefPose()
{
	if (IsInitialized())
	{
		LocalSpacePoses = RefLocalSpacePoses;
		WorldSpacePoses = RefWorldSpacePoses;
	}
	else
	{
		UE_LOG(LogWeaponAnimPose, Warning, TEXT("Anim Pose was not previously initialized"));
	}
}

bool FWeaponAnimPose::IsValid() const
{
	const int32 ExpectedNumBones = BoneNames.Num();
	bool bIsValid = ExpectedNumBones != 0;
	
	bIsValid &= BoneIndices.Num() == ExpectedNumBones;
	bIsValid &= ParentBoneIndices.Num() == ExpectedNumBones;
	bIsValid &= LocalSpacePoses.Num() == ExpectedNumBones;
	bIsValid &= WorldSpacePoses.Num() == ExpectedNumBones;
	bIsValid &= RefLocalSpacePoses.Num() == ExpectedNumBones;
	bIsValid &= RefWorldSpacePoses.Num() == ExpectedNumBones;
	
	return bIsValid;
}

void FWeaponAnimPose::Reset()
{
	BoneNames.Empty();
	BoneIndices.Empty();
	ParentBoneIndices.Empty();
	LocalSpacePoses.Empty();
	WorldSpacePoses.Empty();
	RefLocalSpacePoses.Empty();
	RefWorldSpacePoses.Empty();
}
