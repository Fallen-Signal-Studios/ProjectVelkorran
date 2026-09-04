// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SovAttackReceiptSource.generated.h"

/** Native authority producer of a stable identity shared by the hits of one attack. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USovAttackReceiptSource : public UInterface
{
	GENERATED_BODY()
};

class NARRATIVEARSENAL_API ISovAttackReceiptSource
{
	GENERATED_BODY()
public:
	/** Must reject wrong source, ended activation and stale receipt; never mint on this read. */
	virtual bool GetSovAttackIdentity(const AActor* ExpectedSource, FGuid& OutAttackId) const = 0;
};
