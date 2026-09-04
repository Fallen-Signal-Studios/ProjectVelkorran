// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Status/SovStatusDefinition.h"

#include "Sovereign/SovGameplayTags.h"

namespace
{
	const FPrimaryAssetType StatusDefinitionAssetType(TEXT("SovStatusDefinition"));
}

FPrimaryAssetId USovStatusDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(StatusDefinitionAssetType, GetFName());
}

bool USovStatusDefinition::IsStructurallyValid() const
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	const bool bValidRequestTag = RequestTag.IsValid()
		&& RequestTag != Tags.Status_Apply
		&& RequestTag.MatchesTag(Tags.Status_Apply);
	const bool bValidStateTag = StateTag.IsValid()
		&& StateTag != Tags.State_Status
		&& StateTag.MatchesTag(Tags.State_Status);
	bool bValidCleanseTags = !CleanseTags.IsEmpty();
	for (const FGameplayTag& CleanseTag : CleanseTags)
	{
		bValidCleanseTags = bValidCleanseTags
			&& CleanseTag != Tags.Status_Cleanse
			&& CleanseTag.MatchesTag(Tags.Status_Cleanse);
	}

	return SchemaVersion > 0
		&& bValidRequestTag
		&& bValidStateTag
		&& bValidCleanseTags
		&& UIPriority > 0
		&& PresentationTag.IsValid()
		&& AccessibilityPresentationTag.IsValid()
		&& MaximumStacks > 0
		&& FMath::IsFinite(DefaultDuration)
		&& FMath::IsFinite(Period)
		&& FMath::IsFinite(ResistantDurationMultiplier)
		&& FMath::IsFinite(ResistantMagnitudeMultiplier)
		&& FMath::IsFinite(RecoveryImmunityDuration)
		&& DefaultDuration >= 0.0f
		&& Period >= 0.0f
		&& ResistantDurationMultiplier >= 0.0f
		&& ResistantDurationMultiplier <= 1.0f
		&& ResistantMagnitudeMultiplier >= 0.0f
		&& ResistantMagnitudeMultiplier <= 1.0f
		&& RecoveryImmunityDuration >= 0.0f
		&& (DurationPolicy == ESovStatusDurationPolicy::Infinite
			|| DefaultDuration > KINDA_SMALL_NUMBER)
		&& (!bHardCrowdControl
			|| (RecoveryImmunityTag.IsValid()
				&& RecoveryImmunityDuration > KINDA_SMALL_NUMBER));
}
