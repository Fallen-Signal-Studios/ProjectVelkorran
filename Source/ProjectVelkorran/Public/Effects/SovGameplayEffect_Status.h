// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SovGameplayEffect_Status.generated.h"

/** Safe duration shell for project-owned status tags and authored modifiers. */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Status (Timed)"))
class PROJECTVELKORRAN_API USovGameplayEffect_Status : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_Status();
};

/** Safe infinite shell for explicitly persistent status definitions. */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Status (Infinite)"))
class PROJECTVELKORRAN_API USovGameplayEffect_StatusInfinite : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USovGameplayEffect_StatusInfinite();
};
