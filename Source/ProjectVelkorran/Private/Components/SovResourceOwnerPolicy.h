// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AbilitySystemBlueprintLibrary.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

namespace SovResourceOwner
{
inline bool IsCurrent(const UAbilitySystemComponent* ASC, AActor* Owner)
{
	return IsValid(ASC) && IsValid(Owner) && !Owner->IsActorBeingDestroyed()
		&& ASC->GetAvatarActor() == Owner
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) == ASC;
}
inline bool CanSimulate(const UAbilitySystemComponent* ASC, AActor* Owner)
{
	if (!IsCurrent(ASC, Owner) || !Owner->HasAuthority() || !ASC->GetSet<UNarrativeAttributeSetBase>()) { return false; }
	const float Health = ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	const auto* Narrative = Cast<UNarrativeAbilitySystemComponent>(ASC);
	return FMath::IsFinite(Health) && Health > 0.f && (!Narrative || !Narrative->IsDead())
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
}
}
