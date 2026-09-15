// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SovReplicationReadinessTestFixtures.generated.h"

/** Receives presentation delegates the way an owning client's HUD would. */
UCLASS(Transient, NotBlueprintable)
class USovReplicationReadinessObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 ChargeNotifications = 0;
	int32 LastCharges = -1;
	int32 LastCapacity = -1;
	UFUNCTION(CallInEditor) void ObserveCharges(int32 Charges, int32 Capacity)
	{ ++ChargeNotifications; LastCharges = Charges; LastCapacity = Capacity; }
};
