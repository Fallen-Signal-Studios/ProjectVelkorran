// Copyright Narrative Tools 2025.


#include "GAS/NarrativeGASStatics.h"
#include "GameplayEffect.h"


FGameplayTagContainer UNarrativeGASStatics::GetDynamicGrantedTagsFromEffectSpec(const FGameplayEffectSpec& Spec)
{
	return Spec.DynamicGrantedTags;
}

FGameplayTagContainer UNarrativeGASStatics::GetDynamicAssetTagsFromEffectSpec(const FGameplayEffectSpec& Spec)
{
	return Spec.GetDynamicAssetTags();
}

FGameplayTagContainer UNarrativeGASStatics::GetAllAssetTagsFromEffectSpec(const FGameplayEffectSpec& Spec)
{
	FGameplayTagContainer C;
	Spec.GetAllAssetTags(C);
	return C;
}

bool UNarrativeGASStatics::IsEffectHandleValid(const FActiveGameplayEffectHandle& Handle)
{
	return Handle.IsValid();
}
