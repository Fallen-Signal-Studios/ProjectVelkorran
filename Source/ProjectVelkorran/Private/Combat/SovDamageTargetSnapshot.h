// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"

/** A committed radial candidate belongs to one avatar/readiness generation. */
struct FSovDamageTargetSnapshot
{
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<AActor> Avatar;
	uint64 ActorInfoEpoch = 0;
	int32 ReadyEpoch = 0;
	TWeakObjectPtr<const UNarrativeAttributeSetBase> Attributes;
	uint64 LifeEpoch = 0;

	explicit FSovDamageTargetSnapshot(UAbilitySystemComponent* ASC) : AbilitySystem(ASC)
	{
		Avatar = IsValid(ASC) ? ASC->GetAvatarActor() : nullptr;
		const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
		ActorInfoEpoch = NarrativeASC ? NarrativeASC->GetCombatActorInfoEpoch() : 0;
		ReadyEpoch = NarrativeASC ? NarrativeASC->GetCharacterReadyEpoch() : 0;
		Attributes = IsValid(ASC) ? ASC->GetSet<UNarrativeAttributeSetBase>() : nullptr;
		LifeEpoch = Attributes.IsValid() ? Attributes->GetCombatLifeEpoch() : 0;
	}
	bool IsCurrent() const
	{
		const auto* ASC = AbilitySystem.Get();
		const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
		return IsValid(ASC) && Avatar.IsValid() && ASC->GetAvatarActor() == Avatar.Get()
			&& Attributes.IsValid() && ASC->GetSet<UNarrativeAttributeSetBase>() == Attributes.Get()
			&& Attributes->GetCombatLifeEpoch() == LifeEpoch
			&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == ActorInfoEpoch
				&& NarrativeASC->GetCharacterReadyEpoch() == ReadyEpoch));
	}
};
