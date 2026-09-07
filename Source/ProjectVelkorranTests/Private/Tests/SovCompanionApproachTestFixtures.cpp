// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCompanionApproachTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"

FGameplayTag ASovCompanionApproachTestSelene::GetProtagonistIdentityTag() const
{
	return FSovGameplayTags::Get().Character_Player_Selene;
}
void ASovCompanionApproachTestProxy::OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition)
{
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->AddLooseGameplayTag(GetCompanionIdentity());
	// The production PollStaged call must still copy the kit and every resource.
	bEncounterSnapshotReady = true;
}
