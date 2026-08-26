// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Dismemberment/SovDismembermentProfile.h"

USovDismembermentProfile::USovDismembermentProfile()
{
	BuildSKMannequinRegionDefinitions(Regions);
}

void USovDismembermentProfile::BuildSKMannequinRegionDefinitions(
	TArray<FSovDismembermentRegionDefinition>& OutRegions)
{
	OutRegions.Reset();

	const auto AddRegion = [&OutRegions](
		const ESovDismembermentRegion Region,
		const FName BoneToHide,
		const FName StumpAttachBone,
		const bool bHideHeadPresentation,
		const FName AdditionalHitBoneRoot = NAME_None)
	{
		FSovDismembermentRegionDefinition Definition;
		Definition.Region = Region;
		Definition.HitBoneRoots.Add(BoneToHide);
		if (!AdditionalHitBoneRoot.IsNone())
		{
			Definition.HitBoneRoots.Add(AdditionalHitBoneRoot);
		}
		Definition.BoneToHide = BoneToHide;
		Definition.StumpAttachBone = StumpAttachBone;
		Definition.bHideHeadPresentation = bHideHeadPresentation;
		OutRegions.Add(MoveTemp(Definition));
	};

	AddRegion(
		ESovDismembermentRegion::Head,
		TEXT("head"),
		TEXT("neck_01"),
		true,
		TEXT("neck_01"));
	AddRegion(
		ESovDismembermentRegion::LeftUpperArm,
		TEXT("upperarm_l"),
		TEXT("clavicle_l"),
		false,
		TEXT("clavicle_l"));
	AddRegion(
		ESovDismembermentRegion::RightUpperArm,
		TEXT("upperarm_r"),
		TEXT("clavicle_r"),
		false,
		TEXT("clavicle_r"));
	AddRegion(
		ESovDismembermentRegion::LeftForearm,
		TEXT("lowerarm_l"),
		TEXT("upperarm_l"),
		false);
	AddRegion(
		ESovDismembermentRegion::RightForearm,
		TEXT("lowerarm_r"),
		TEXT("upperarm_r"),
		false);
	AddRegion(
		ESovDismembermentRegion::LeftHand,
		TEXT("hand_l"),
		TEXT("lowerarm_l"),
		false);
	AddRegion(
		ESovDismembermentRegion::RightHand,
		TEXT("hand_r"),
		TEXT("lowerarm_r"),
		false);
	AddRegion(
		ESovDismembermentRegion::LeftUpperLeg,
		TEXT("thigh_l"),
		TEXT("pelvis"),
		false);
	AddRegion(
		ESovDismembermentRegion::RightUpperLeg,
		TEXT("thigh_r"),
		TEXT("pelvis"),
		false);
	AddRegion(
		ESovDismembermentRegion::LeftLowerLeg,
		TEXT("calf_l"),
		TEXT("thigh_l"),
		false);
	AddRegion(
		ESovDismembermentRegion::RightLowerLeg,
		TEXT("calf_r"),
		TEXT("thigh_r"),
		false);
	AddRegion(
		ESovDismembermentRegion::LeftFoot,
		TEXT("foot_l"),
		TEXT("calf_l"),
		false);
	AddRegion(
		ESovDismembermentRegion::RightFoot,
		TEXT("foot_r"),
		TEXT("calf_r"),
		false);
}

void USovDismembermentProfile::ApplySKMannequinBoneMap()
{
#if WITH_EDITOR
	Modify();
#endif

	TArray<FSovDismembermentRegionDefinition> MannequinRegions;
	BuildSKMannequinRegionDefinitions(MannequinRegions);
	for (const FSovDismembermentRegionDefinition& MannequinDefinition :
		MannequinRegions)
	{
		FSovDismembermentRegionDefinition* ExistingDefinition =
			Regions.FindByPredicate(
				[&MannequinDefinition](
					const FSovDismembermentRegionDefinition& Definition)
				{
					return Definition.Region == MannequinDefinition.Region;
				});
		if (ExistingDefinition == nullptr)
		{
			Regions.Add(MannequinDefinition);
			continue;
		}

		ExistingDefinition->HitBoneRoots = MannequinDefinition.HitBoneRoots;
		ExistingDefinition->BoneToHide = MannequinDefinition.BoneToHide;
		ExistingDefinition->StumpAttachBone = MannequinDefinition.StumpAttachBone;
		ExistingDefinition->bHideHeadPresentation |=
			MannequinDefinition.bHideHeadPresentation;
	}

	MarkPackageDirty();
}

bool USovDismembermentProfile::GetRegionDefinition(
	const ESovDismembermentRegion Region,
	FSovDismembermentRegionDefinition& OutDefinition) const
{
	if (const FSovDismembermentRegionDefinition* Definition = Regions.FindByPredicate(
		[Region](const FSovDismembermentRegionDefinition& Candidate)
		{
			return Candidate.Region == Region;
		}))
	{
		OutDefinition = *Definition;
		return true;
	}

	return false;
}
