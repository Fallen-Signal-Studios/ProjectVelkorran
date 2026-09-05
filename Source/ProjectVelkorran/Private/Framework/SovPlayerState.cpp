// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Framework/SovPlayerState.h"

#include "GAS/SovCorruptionAttributeSet.h"

ASovPlayerState::ASovPlayerState(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CorruptionAttributeSet = CreateDefaultSubobject<USovCorruptionAttributeSet>(
		TEXT("SovCorruptionAttributeSet"));
}
