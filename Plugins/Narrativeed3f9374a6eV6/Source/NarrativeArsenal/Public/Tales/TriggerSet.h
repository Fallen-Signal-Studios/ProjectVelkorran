// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TriggerSet.generated.h"

/**
 * Contains an array of instanced triggers to use as a template 
 */
UCLASS()
class NARRATIVEARSENAL_API UTriggerSet : public UDataAsset
{
	GENERATED_BODY()
	
public: 

	/** The triggers and their events to run */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Triggers")
	TArray<class UNarrativeTrigger*> Triggers;
};
