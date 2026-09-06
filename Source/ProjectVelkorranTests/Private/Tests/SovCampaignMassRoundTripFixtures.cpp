// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCampaignMassRoundTripFixtures.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "UObject/ConstructorHelpers.h"

ASovCampaignMassRoundTripNPC::ASovCampaignMassRoundTripNPC(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	auto* Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MassFixtureVisual"));
	Visual->SetupAttachment(GetRootComponent()); Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Visual->SetStaticMesh(Cube.Object);
}

void ASovCampaignMassRoundTripNPC::SetNPCDefinition(UNPCDefinition* Definition)
{
	const FGuid Identity = GetActorGUID_Implementation();
	NPCDefinition = Definition; InitializeTestCombat(); SetActorGUID_Implementation(Identity);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxShieldAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxPoiseAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetPoiseAttribute(), 100.f);
}
