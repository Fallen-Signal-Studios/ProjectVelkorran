// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCompanionCommandTestFixtures.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "GAS/NarrativeAttributeSetBase.h"

void ASovCompanionCommandTestProxy::InitializeCommandCombat()
{
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
}

ETeamAttitude::Type ASovCompanionCommandTestProxy::GetTeamAttitudeTowards(const AActor& Other) const
{
	const auto* Target = Cast<ASovBotTestCharacter>(&Other);
	return Target && Target->TestTeam == 1 ? ETeamAttitude::Hostile : ETeamAttitude::Friendly;
}

void USovCompanionCommandTestDefense::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* Event)
{
	++ActivationCount;
	EndAbility(Handle, Info, ActivationInfo, false, false);
}
