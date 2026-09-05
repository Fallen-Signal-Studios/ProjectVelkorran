// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "UObject/Interface.h"
#include "SovExertionProvider.generated.h"

/** Native resource contract; Narrative attacks do not own a second Stamina pool. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USovExertionProvider : public UInterface
{
	GENERATED_BODY()
};

class NARRATIVEARSENAL_API ISovExertionProvider
{
	GENERATED_BODY()
public:
	virtual bool CanSpendExertion(float Cost) const = 0;
	virtual bool TrySpendExertion(float Cost) = 0;
};
