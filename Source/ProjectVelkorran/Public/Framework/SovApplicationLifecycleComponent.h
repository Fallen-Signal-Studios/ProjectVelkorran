// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "Framework/SovLifecyclePolicy.h"
#include "SovApplicationLifecycleComponent.generated.h"

class APlayerController;
class USovApplicationInterruptionMenu;

/** Local application/controller interruption ownership; no suspend-time disk writes. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovApplicationLifecycleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USovApplicationLifecycleComponent();
    UFUNCTION(BlueprintPure, Category="Platform") bool IsGameplayInterrupted() const { return State.IsInterrupted(); }
    UFUNCTION(BlueprintPure, Category="Platform") bool CanResumeGameplay() const;
    UFUNCTION(BlueprintCallable, Category="Platform") bool ResumeGameplay();
    UFUNCTION(BlueprintPure, Category="Platform") FText GetInterruptionMessage() const;
    /** Monotonic deadline clock excludes background, overlay, missing input/account and resume prompt time. */
    static double ActiveTimeSeconds(const APlayerController* Player);
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Tick) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    friend class USovApplicationLifecycleSubsystem;
    friend struct FSovLifecycleTestAccess;
    friend struct FSovCinematicInterruptionTestAccess;
    void RefreshOwnership();
    void SetReason(SovLifecyclePolicy::Reason Reason, bool bActive);
    void ApplyInterruption();
    void RefreshPlatformServices();
    bool HasLocalViewport() const;
    bool HasOwningInputDevice() const;
    bool HasStorageOwner() const;
    bool IsApplicationUnavailable() const { return State.IsApplicationUnavailable(); }
    SovLifecyclePolicy::State State;
    TSet<FInputDeviceId> KnownOwnedDevices;
    FPlatformUserId ObservedUser = PLATFORMUSERID_NONE;
    FDelegateHandle ConnectionHandle, PairingHandle;
    bool bEnding = false;
    bool bBound = false;
    bool bOwnInputLock = false;
    bool bOwnPause = false;
    bool bHadStorageOwner = false;
    bool bApplyingInterruption = false;
    bool bRefreshingOwnership = false;
    UPROPERTY(Transient) TObjectPtr<USovApplicationInterruptionMenu> InterruptionMenu;
};
