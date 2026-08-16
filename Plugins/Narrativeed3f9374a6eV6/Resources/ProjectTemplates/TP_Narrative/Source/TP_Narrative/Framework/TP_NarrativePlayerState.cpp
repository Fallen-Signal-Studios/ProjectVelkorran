// Fill out your copyright notice in the Description page of Project Settings.


#include "TP_NarrativePlayerState.h"
#include "../GAS/TP_NarrativeAbilityComponent.h"

ATP_NarrativePlayerState::ATP_NarrativePlayerState(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTP_NarrativeAbilityComponent>("AbilitySystemComponent"))
{

}
