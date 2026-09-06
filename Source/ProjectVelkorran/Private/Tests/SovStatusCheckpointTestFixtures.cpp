// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovStatusCheckpointTestFixtures.h"

#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"

USovStatusCheckpointSafePeriodicEffect::USovStatusCheckpointSafePeriodicEffect()
{
	Period = FScalableFloat(1.f);
	bExecutePeriodicEffectOnApplication = false;
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UNarrativeAttributeSetBase::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(-1.f);
	Modifiers.Add(Modifier);
}

USovStatusCheckpointUnsafePeriodicEffect::USovStatusCheckpointUnsafePeriodicEffect()
{
	Period = FScalableFloat(1.f);
	bExecutePeriodicEffectOnApplication = true;
}

USovStatusCheckpointAggregateEffect::USovStatusCheckpointAggregateEffect()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	StackingType = EGameplayEffectStackingType::AggregateByTarget;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

USovStatusCheckpointContinuousResourceEffect::USovStatusCheckpointContinuousResourceEffect()
{
	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UNarrativeAttributeSetBase::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(-10.f);
	Modifiers.Add(Modifier);
}

ASovStatusCheckpointTestActor::ASovStatusCheckpointTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	ASC = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("CheckpointASC"));
	Attributes = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("CheckpointAttributes"));
	Status = CreateDefaultSubobject<USovStatusCheckpointTestComponent>(TEXT("CheckpointStatus"));
}

void ASovStatusCheckpointTestActor::InitializeCombat(const bool bInitializeStatus)
{
	ASC->AddAttributeSetSubobject(Attributes.Get());
	ASC->InitAbilityActorInfo(this, this);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 100.f);
	ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	if (bInitializeStatus) { Status->InitializeWithAbilitySystem(ASC); }
}

UAbilitySystemComponent* ASovStatusCheckpointTestActor::GetAbilitySystemComponent() const
{
	return ASC;
}

void USovStatusCheckpointObserver::OnStatusChanged(const FGameplayTag RequestTag, const FGameplayTag StateTag,
	const ESovStatusChangeReason Reason, const int32 StackCount, AActor* SourceActor)
{
	if (Reason == ESovStatusChangeReason::Restored)
	{
		++RestoredCount;
		RestoredTags.Add(RequestTag);
		if (bReplaceOnFirstRestore && RestoredCount == 1 && ASC && OwnerActor && ReplacementAvatar)
		{
			// Narrative intentionally ignores an in-place non-character avatar swap.
			// Exercise the real teardown/rebind path for this content-free actor.
			ASC->ClearActorInfo();
			ASC->InitAbilityActorInfo(OwnerActor, ReplacementAvatar);
		}
	}
	if (Reason == ESovStatusChangeReason::Expired) { ++ExpiredCount; }
}
