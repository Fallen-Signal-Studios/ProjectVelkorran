// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovCampaignHandoffAnchor.generated.h"
class ASovPlayerController;

/** Explicit physical transition, authored only for a mission's required handoff beat. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovCampaignHandoffAnchor : public AActor
{
	GENERATED_BODY()
public:
	ASovCampaignHandoffAnchor();
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName AnchorId;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName MissionId;
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Campaign") FName HandoffBeat;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1")) float RequestRange = 250.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign") TObjectPtr<class USceneComponent> Destination;
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Campaign") bool RequestHandoff(ASovPlayerController* Controller, FString& OutError);
	bool ValidateRequest(const ASovPlayerController* Controller, FTransform& OutDestination, FString& OutError) const;
};
