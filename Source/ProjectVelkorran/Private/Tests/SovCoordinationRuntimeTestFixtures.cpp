// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCoordinationRuntimeTestFixtures.h"
#include "Components/CapsuleComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"

ASovCoordinationTestNPC::ASovCoordinationTestNPC(const FObjectInitializer& Initializer)
	: Super(Initializer.SetDefaultSubobjectClass<USovCoordinationTestASC>(TEXT("AbilitySystemComponent")))
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ASovCoordinationTestNPC::InitializeTestCombat()
{
	AbilitySystemComponent->AddAttributeSetSubobject(AttributeSetBase.Get());
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetMaxHealthAttribute(), 100.f);
	AbilitySystemComponent->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetHealthAttribute(), 100.f);
	NativeSaveGuid = FGuid::NewGuid();
	// NPC promotion waits on this readiness flag after the fixture initializes its ASC and attributes.
	bEncounterSnapshotReady = true;
}

ETeamAttitude::Type ASovCoordinationTestNPC::GetTeamAttitudeTowards(const AActor& Other) const
{
	return &Other == this || Other.IsA<ASovCoordinationTestNPC>() ? ETeamAttitude::Friendly : ETeamAttitude::Hostile;
}

void ASovCoordinationTestNPC::GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const
{
	Location = GetActorLocation(); Rotation = GetActorRotation();
}
