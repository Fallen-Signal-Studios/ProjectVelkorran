// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovNPCCharacterBase.h"

#include "Components/SovDismembermentComponent.h"

ASovNPCCharacterBase::ASovNPCCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DismembermentComponent = CreateDefaultSubobject<USovDismembermentComponent>(
		TEXT("SovDismembermentComponent"));
}
