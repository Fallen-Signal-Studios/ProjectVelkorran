// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "TP_NarrativeNPCCharacter.generated.h"

/**
 * Default NPC Character for your Narrative Game Project.
 *
 *
 * If you wish to use this, simply reparent BP_NarrativeNPC to your ATP_NarrativeNPCCharacter base class.
 */
UCLASS()
class TP_NARRATIVE_API ATP_NarrativeNPCCharacter : public ANarrativeNPCCharacter
{
	GENERATED_BODY()
	
public:

	ATP_NarrativeNPCCharacter(const class FObjectInitializer& ObjectInitializer);

};
