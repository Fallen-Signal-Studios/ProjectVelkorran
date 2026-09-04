// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovNPCCharacterBase.h"

#include "Components/SovCombatSustainDropComponent.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovStatusComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"

ASovNPCCharacterBase::ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DismembermentComponent = CreateDefaultSubobject<USovDismembermentComponent>(
		TEXT("SovDismembermentComponent"));
	CombatSustainDropComponent = CreateDefaultSubobject<USovCombatSustainDropComponent>(
		TEXT("SovCombatSustainDropComponent"));
	StatusComponent = CreateDefaultSubobject<USovStatusComponent>(TEXT("SovStatusComponent"));
}

void ASovNPCCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Narrative initializes an NPC's pawn-owned ASC during its BeginPlay. The
	// component also retains its OnASCInitialized fallback for unusual ordering.
	if (StatusComponent)
	{
		StatusComponent->InitializeWithAbilitySystem(
			GetNarrativeAbilitySystemComponent());
	}
}
