// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TriggerSchedule.generated.h"

/**
 * Contains a set of Narrative Triggers and their Events to activate/deactivate
 */
UCLASS()
class NARRATIVEARSENAL_API UTriggerSchedule : public UDataAsset
{
	GENERATED_BODY()
	
public: 

	UTriggerSchedule();

	/** The triggers that make up this schedule */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Triggers")
	TArray<TSoftObjectPtr<class UTriggerSet>> Triggers;

};
