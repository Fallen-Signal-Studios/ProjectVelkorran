// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "SovSeleneCharacter.generated.h"

/** Selene's concrete player class and boundary for precision/disruption systems. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovSeleneCharacter : public ASovPlayerCharacterBase
{
	GENERATED_BODY()

public:
	ASovSeleneCharacter(const FObjectInitializer& ObjectInitializer);

	virtual FGameplayTag GetProtagonistIdentityTag() const override;
};
