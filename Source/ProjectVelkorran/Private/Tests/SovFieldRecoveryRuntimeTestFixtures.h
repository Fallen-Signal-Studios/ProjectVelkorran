// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Tests/SovExertionRuntimeTestFixtures.h"
#include "SovFieldRecoveryRuntimeTestFixtures.generated.h"
class ASovPlayerState;

UCLASS(Transient, NotBlueprintable, NotPlaceable)
class ASovFieldRecoveryTestCharacter : public ASovExertionRuntimeTestCharacter
{
	GENERATED_BODY()
public:
	ASovFieldRecoveryTestCharacter(const FObjectInitializer& Initializer) : Super(Initializer) {}
	void InitializeSharedState(ASovPlayerState* State, bool bSelene);
};
UCLASS(Transient, NotBlueprintable)
class USovFieldRecoveryTestObserver : public UObject
{
	GENERATED_BODY()
public:
	int32 Notifications = 0;
	UFUNCTION() void Changed(int32 Charges, int32 Capacity) { static_cast<void>(Charges); static_cast<void>(Capacity); ++Notifications; }
};
