// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovPlayerCharacterBase.h"

#include "Components/SovEchoComponent.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovHealthRechargeComponent.h"
#include "Components/SovPoiseComponent.h"
#include "Components/SovShieldComponent.h"
#include "Components/SovTarrikEchoGenerationComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"

ASovPlayerCharacterBase::ASovPlayerCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EchoComponent = CreateDefaultSubobject<USovEchoComponent>(TEXT("SovEchoComponent"));
	TarrikEchoGenerationComponent = CreateDefaultSubobject<USovTarrikEchoGenerationComponent>(
		TEXT("SovTarrikEchoGenerationComponent"));
	ShieldComponent = CreateDefaultSubobject<USovShieldComponent>(TEXT("SovShieldComponent"));
	HealthRechargeComponent = CreateDefaultSubobject<USovHealthRechargeComponent>(
		TEXT("SovHealthRechargeComponent"));
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
	if (TarrikEchoGenerationComponent)
	{
		TarrikEchoGenerationComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
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
	if (GuardComponent)
	{
		GuardComponent->InitializeWithAbilitySystem(ReadyAbilitySystem);
	}
}

void ASovPlayerCharacterBase::OnRep_WieldState(
	const FWeaponWieldState& OldWieldState)
{
	Super::OnRep_WieldState(OldWieldState);

	if (TarrikEchoGenerationComponent)
	{
		TarrikEchoGenerationComponent->HandleOwnerWieldStateChanged();
	}
}

bool ASovPlayerCharacterBase::AreAdditionalCharacterSystemsReady() const
{
	return Super::AreAdditionalCharacterSystemsReady()
		&& IsValid(EchoComponent) && EchoComponent->IsInitialized()
		&& IsValid(TarrikEchoGenerationComponent)
		&& TarrikEchoGenerationComponent->IsInitialized()
		&& IsValid(ShieldComponent) && ShieldComponent->IsInitialized()
		&& IsValid(HealthRechargeComponent) && HealthRechargeComponent->IsInitialized()
		&& IsValid(PoiseComponent) && PoiseComponent->IsInitialized()
		&& IsValid(GuardComponent) && GuardComponent->IsInitialized();
}
