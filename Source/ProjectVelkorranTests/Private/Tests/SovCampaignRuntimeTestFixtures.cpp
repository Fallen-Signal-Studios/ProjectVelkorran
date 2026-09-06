// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sovereign/SovGameplayTags.h"
ASovCampaignRuntimeTestPawn::ASovCampaignRuntimeTestPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TestHero = FSovGameplayTags::Get().Character_Player_Tarrik;
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->SetComponentTickEnabled(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
ASovCampaignRuntimeTestController::ASovCampaignRuntimeTestController()
{
	State = CreateDefaultSubobject<USovCampaignStateComponent>(TEXT("CampaignState"));
}
void ASovCampaignRuntimeTestController::ObserveBeat(const FSovCampaignJournalEntry& Entry)
{
	++BeatEvents;
	if (!ReentrantBeat.IsNone()) { NestedResult = State->CompleteBeat(ReentrantBeat); }
}
void ASovCampaignRuntimeTestController::ObserveEvidence(const FSovEvidenceAcquisition& Acquisition)
{
	++EvidenceEvents;
}
