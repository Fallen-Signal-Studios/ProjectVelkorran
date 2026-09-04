// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCorruptionRuntimeTestFixtures.h"
#include "Components/CapsuleComponent.h"
#include "Components/SovCorruptionComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"

ASovCorruptionRuntimeTestPawn::ASovCorruptionRuntimeTestPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilitySystemComponent = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("CorruptionTestASC"));
	AttributeSetBase = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("CorruptionTestAttributes"));
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
}
void ASovCorruptionRuntimeTestPawn::InitializeCombat()
{
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.0f);
	AbilitySystemComponent->AddLooseGameplayTag(TestHero);
}
void ASovCorruptionRuntimeTestPawn::SaveOnBandChanged(ESovCorruptionBand Previous, ESovCorruptionBand Current)
{
	++SaveCallbacks;
	GetCorruptionComponent()->PrepareForSave_Implementation();
}
