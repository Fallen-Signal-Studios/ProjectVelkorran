// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "GameFramework/PlayerController.h"
#include "SovCampaignRuntimeTestFixtures.generated.h"

/** A real possessed pawn, excluding Narrative content initialization from state boundary tests. */
UCLASS(Transient, NotBlueprintable)
class ASovCampaignRuntimeTestPawn : public ASovPlayerCharacterBase
{
	GENERATED_BODY()
public:
	ASovCampaignRuntimeTestPawn(const FObjectInitializer& ObjectInitializer);
	FGameplayTag TestHero;
	virtual FGameplayTag GetProtagonistIdentityTag() const override { return TestHero; }
	virtual FVector GetPawnViewLocation() const override { return GetActorLocation(); }
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
};

UCLASS(Transient, NotBlueprintable)
class ASovCampaignRuntimeTestController : public APlayerController
{
	GENERATED_BODY()
public:
	ASovCampaignRuntimeTestController();
	UPROPERTY() TObjectPtr<USovCampaignStateComponent> State;
	UPROPERTY() TArray<TObjectPtr<UObject>> KeepAlive;
	FName ReentrantBeat;
	ESovCampaignResult NestedResult = ESovCampaignResult::Invalid;
	int32 BeatEvents = 0;
	int32 EvidenceEvents = 0;
	UFUNCTION() void ObserveBeat(const FSovCampaignJournalEntry& Entry);
	UFUNCTION() void ObserveEvidence(const FSovEvidenceAcquisition& Acquisition);
};
