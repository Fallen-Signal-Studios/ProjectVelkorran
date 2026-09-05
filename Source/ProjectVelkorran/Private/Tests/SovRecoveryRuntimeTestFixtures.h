// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Recovery/SovFatalRecoveryComponent.h"
#include "SovRecoveryRuntimeTestFixtures.generated.h"

UCLASS()
class USovRecoveryReentryProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<USovFatalRecoveryComponent> Recovery;
	bool bDetachOnResolving = false;
	int32 Notifications = 0;
	UFUNCTION() void OnChanged(ESovRecoveryState State, const FString& Reason)
	{
		++Notifications;
		if (bDetachOnResolving && State == ESovRecoveryState::ResolvingFatal && Recovery)
		{ bDetachOnResolving = false; Recovery->InitializeWithAbilitySystem(nullptr); }
	}
};
