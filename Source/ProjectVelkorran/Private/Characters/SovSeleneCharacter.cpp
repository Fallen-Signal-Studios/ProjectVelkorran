// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovSeleneCharacter.h"

#include "Sovereign/SovGameplayTags.h"

ASovSeleneCharacter::ASovSeleneCharacter(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FGameplayTag ASovSeleneCharacter::GetProtagonistIdentityTag() const
{
	return FSovGameplayTags::Get().Character_Player_Selene;
}
