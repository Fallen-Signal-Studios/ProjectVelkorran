// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "TP_NarrativePlayerState.generated.h"

/**
 * Default Player state for your Narrative Game Project.
 *
 *
 * If you wish to use this, simply reparent BP_NarrativePlayerState to your ATP_NarrativePlayerState base class.
 */
UCLASS()
class TP_NARRATIVE_API ATP_NarrativePlayerState : public ANarrativePlayerState
{
	GENERATED_BODY()
	
public:

	ATP_NarrativePlayerState(const FObjectInitializer& ObjectInitializer);

};
