// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovAurelionThermalTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"

void ASovAurelionThermalTestCompanion::InitializeTestCombat()
{
    AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    AbilitySystemComponent->AddLooseGameplayTag(GetCompanionIdentity());
    AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
    AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
    bEncounterSnapshotReady = true;
}
