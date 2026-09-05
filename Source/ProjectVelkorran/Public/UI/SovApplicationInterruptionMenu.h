// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/NarrativeMenu.h"
#include "SovApplicationInterruptionMenu.generated.h"
class UTextBlock;
class USovAccessibilityNativeButton;

/** Explicit resume within the existing CommonUI modal layer, safe for controller-only play. */
UCLASS()
class PROJECTVELKORRAN_API USovApplicationInterruptionMenu : public UNarrativeMenu
{
    GENERATED_BODY()
public:
    USovApplicationInterruptionMenu();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeTick(const FGeometry& Geometry, float Delta) override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual bool NativeOnHandleBackAction() override { return true; }
private:
    void RefreshMessage(bool bAnnounce);
    UFUNCTION() void Resume();
    UFUNCTION() void ReturnToTitle();
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Message;
    UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> ResumeButton;
    UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> TitleButton;
    FText LastMessage;
    FText RecoveryError;
    bool bConfirmTitle = false;
};
