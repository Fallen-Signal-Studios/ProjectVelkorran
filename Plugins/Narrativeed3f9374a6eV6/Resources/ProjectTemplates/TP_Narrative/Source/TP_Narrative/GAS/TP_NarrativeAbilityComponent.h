// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "TP_NarrativeAbilityComponent.generated.h"

/**
 * Default AbilitySystemComponent for your Narrative Game Project.
 *
 * PlayerState & NPC are already setup to override this, check out UTP_NarrativePlayerState/UTP_NarrativeNPCCharacter constructors. 
 */
UCLASS()
class TP_NARRATIVE_API UTP_NarrativeAbilityComponent : public UNarrativeAbilitySystemComponent
{
	GENERATED_BODY()
	
};
