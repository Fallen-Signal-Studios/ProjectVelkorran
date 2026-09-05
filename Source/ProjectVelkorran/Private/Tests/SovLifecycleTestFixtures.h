// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Framework/SovApplicationLifecycleSubsystem.h"
#include "SovLifecycleTestFixtures.generated.h"

UCLASS()
class ASovLifecycleTestGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    bool bAcceptPause = true;
    virtual bool AllowPausing(APlayerController* PC) override { return bAcceptPause && Super::AllowPausing(PC); }
};

UCLASS()
class USovLifecycleTestApplicationSubsystem : public USovApplicationLifecycleSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return false; }
    int32 DisplayReversions = 0;
protected:
    virtual void RevertDisplayPreview() override { ++DisplayReversions; }
};
