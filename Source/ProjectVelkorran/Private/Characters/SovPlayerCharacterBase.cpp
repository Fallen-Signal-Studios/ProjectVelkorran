// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovPlayerCharacterBase.h"

#include "Character/CharacterDefinition.h"
#include "Components/SovCorruptionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovStatusComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Sovereign/SovGameplayTags.h"

ASovPlayerCharacterBase::ASovPlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EchoComponent = CreateDefaultSubobject<USovEchoComponent>(TEXT("SovEchoComponent"));
	ShieldComponent = CreateDefaultSubobject<USovShieldComponent>(TEXT("SovShieldComponent"));
	HealthRechargeComponent = CreateDefaultSubobject<USovHealthRechargeComponent>(
		TEXT("SovHealthRechargeComponent"));
	PoiseComponent = CreateDefaultSubobject<USovPoiseComponent>(TEXT("SovPoiseComponent"));
	StatusComponent = CreateDefaultSubobject<USovStatusComponent>(TEXT("SovStatusComponent"));
	CorruptionComponent = CreateDefaultSubobject<USovCorruptionComponent>(
		TEXT("SovCorruptionComponent"));
}

void ASovPlayerCharacterBase::HandleAbilitySystemReady(
	UNarrativeAbilitySystemComponent* ReadyAbilitySystem)
{
	Super::HandleAbilitySystemReady(ReadyAbilitySystem);

	if (EchoComponent)
	{
		EchoComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (ShieldComponent)
	{
		ShieldComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (HealthRechargeComponent)
	{
		HealthRechargeComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (PoiseComponent)
	{
		PoiseComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (StatusComponent)
	{
		StatusComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (CorruptionComponent)
	{
		CorruptionComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
}

FGameplayTag ASovPlayerCharacterBase::GetProtagonistIdentityTag() const
{
	return FGameplayTag();
}

bool ASovPlayerCharacterBase::AreAdditionalCharacterSystemsReady() const
{
	return Super::AreAdditionalCharacterSystemsReady()
		&& IsValid(EchoComponent) && EchoComponent->IsInitialized()
		&& IsValid(ShieldComponent) && ShieldComponent->IsInitialized()
		&& IsValid(HealthRechargeComponent) && HealthRechargeComponent->IsInitialized()
		&& IsValid(PoiseComponent) && PoiseComponent->IsInitialized()
		&& IsValid(StatusComponent) && StatusComponent->IsInitialized()
		&& IsValid(CorruptionComponent) && CorruptionComponent->IsInitialized();
}

void ASovPlayerCharacterBase::OnDefinitionSet_Implementation(
	UCharacterDefinition* NewDefinition)
{
	Super::OnDefinitionSet_Implementation(NewDefinition);

	const FGameplayTag ProtagonistTag = GetProtagonistIdentityTag();
	if (!IsValid(NewDefinition) || !ProtagonistTag.IsValid())
	{
		return;
	}

	UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!IsValid(NarrativeAbilitySystem))
	{
		return;
	}

	FGameplayTagContainer DefinitionTags = NewDefinition->DefaultOwnedTags;
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	DefinitionTags.RemoveTag(SovTags.Character_Player_Tarrik);
	DefinitionTags.RemoveTag(SovTags.Character_Player_Selene);
	DefinitionTags.AddTag(ProtagonistTag);
	NarrativeAbilitySystem->SetDefinitionOwnedTags(DefinitionTags);
}
