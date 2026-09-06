// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovPassiveDefenseTestFixtures.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"

ASovPassiveDefenseTestActor::ASovPassiveDefenseTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	OwnedASC = CreateDefaultSubobject<USovPassiveDefenseTestASC>(TEXT("PassiveASC"));
	ActiveASC = OwnedASC;
	Attributes = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("PassiveAttributes"));
	Shield = CreateDefaultSubobject<USovPassiveDefenseTestShield>(TEXT("PassiveShield"));
	Poise = CreateDefaultSubobject<USovPassiveDefenseTestPoise>(TEXT("PassivePoise"));
}

void ASovPassiveDefenseTestActor::InitializeCombat(const bool bInitializeComponents)
{
	OwnedASC->AddAttributeSetSubobject(Attributes.Get());
	OwnedASC->InitAbilityActorInfo(this, this);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxStaminaAttribute(), 100.f);
	OwnedASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetStaminaAttribute(), 100.f);
	Shield->UseFastTimers(); Poise->UseFastTimers();
	Shield->OnShieldChanged.AddUniqueDynamic(this, &ThisClass::ObserveShield);
	Shield->OnShieldBroken.AddUniqueDynamic(this, &ThisClass::ObserveShieldBreak);
	Poise->OnPoiseStateChanged.AddUniqueDynamic(this, &ThisClass::ObservePoise);
	Poise->OnPoiseBroken.AddUniqueDynamic(this, &ThisClass::ObservePoiseBreak);
	Poise->OnPoiseRecovered.AddUniqueDynamic(this, &ThisClass::ObservePoiseRecovery);
	if (bInitializeComponents)
	{
		Shield->InitializeWithAbilitySystem(ActiveASC);
		Poise->InitializeWithAbilitySystem(ActiveASC);
	}
}

UAbilitySystemComponent* ASovPassiveDefenseTestActor::GetAbilitySystemComponent() const { return ActiveASC; }

void ASovPassiveDefenseTestActor::ObserveShield(float OldShield, float NewShield, float MaxShield)
{
	if (OnNextShieldChange) { TFunction<void()> Callback = MoveTemp(OnNextShieldChange); Callback(); }
}

void ASovPassiveDefenseTestActor::ObservePoise(ESovPoiseState Previous, ESovPoiseState Current)
{
	if (OnNextPoiseStateChange) { TFunction<void()> Callback = MoveTemp(OnNextPoiseStateChange); Callback(); }
}

void ASovPassiveDefenseTestActor::InitializeResourcesOnRevive(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, const bool bDead)
{
	if (!bDead && Actor == this && ASC == ActiveASC)
	{
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetShieldAttribute(), 25.f);
		ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 25.f);
	}
}
