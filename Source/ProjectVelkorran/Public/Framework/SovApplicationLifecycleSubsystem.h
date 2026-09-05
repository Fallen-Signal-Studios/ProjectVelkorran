// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Framework/SovLifecyclePolicy.h"
#include "SovApplicationLifecycleSubsystem.generated.h"

/** Platform event owner survives non-seamless controller/world replacement. */
UCLASS()
class PROJECTVELKORRAN_API USovApplicationLifecycleSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    bool HasReason(SovLifecyclePolicy::Reason Reason) const { return State.HasReason(Reason); }
    bool IsAwaitingResume() const { return State.IsInterrupted(); }
    bool IsApplicationUnavailable() const { return State.IsApplicationUnavailable(); }
    void AcknowledgeResume();
protected:
    /** Test seam keeps lifecycle regressions from touching the user's real monitor preview. */
    virtual void RevertDisplayPreview();
private:
    friend struct FSovLifecycleTestAccess;
    void SetReason(SovLifecyclePolicy::Reason Reason, bool bActive);
    UPROPERTY(Transient) TObjectPtr<class USovSaveSubsystem> Saves;
    SovLifecyclePolicy::State State;
    FDelegateHandle BackgroundHandle, ForegroundHandle, DeactivateHandle, ReactivateHandle, OverlayHandle;
    bool bEnding = false;
    uint64 Generation = 0;
};
