// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovPlayerCharacterBase.h"

#include "Components/SovEchoComponent.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"

ASovPlayerCharacterBase::ASovPlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EchoComponent = CreateDefaultSubobject<USovEchoComponent>(TEXT("SovEchoComponent"));
	ShieldComponent = CreateDefaultSubobject<USovShieldComponent>(TEXT("SovShieldComponent"));
	PoiseComponent = CreateDefaultSubobject<USovPoiseComponent>(TEXT("SovPoiseComponent"));
	GuardComponent = CreateDefaultSubobject<USovGuardComponent>(TEXT("SovGuardComponent"));
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
	if (PoiseComponent)
	{
		PoiseComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
	if (GuardComponent)
	{
		GuardComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
}

bool ASovPlayerCharacterBase::AreAdditionalCharacterSystemsReady() const
{
	return Super::AreAdditionalCharacterSystemsReady()
		&& IsValid(EchoComponent) && EchoComponent->IsInitialized()
		&& IsValid(ShieldComponent) && ShieldComponent->IsInitialized()
		&& IsValid(PoiseComponent) && PoiseComponent->IsInitialized()
		&& IsValid(GuardComponent) && GuardComponent->IsInitialized();
}
