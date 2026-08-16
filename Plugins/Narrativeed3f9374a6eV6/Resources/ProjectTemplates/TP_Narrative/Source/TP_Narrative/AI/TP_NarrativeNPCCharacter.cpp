// Fill out your copyright notice in the Description page of Project Settings.


#include "TP_NarrativeNPCCharacter.h"
#include "../GAS/TP_NarrativeAbilityComponent.h"

ATP_NarrativeNPCCharacter::ATP_NarrativeNPCCharacter(const class FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTP_NarrativeAbilityComponent>("AbilitySystemComponent"))
{

}
