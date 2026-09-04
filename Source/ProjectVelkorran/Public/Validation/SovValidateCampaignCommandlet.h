// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "SovValidateCampaignCommandlet.generated.h"

/** Explicit mission-manifest preflight. Does not certify maps, dialogue, performance or playability. */
UCLASS()
class PROJECTVELKORRAN_API USovValidateCampaignCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	USovValidateCampaignCommandlet();
	virtual int32 Main(const FString& Params) override;
};
