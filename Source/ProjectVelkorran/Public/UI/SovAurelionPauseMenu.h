// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/NarrativeMenu.h"
#include "Save/SovCampaignSaveGame.h"
#include "SovAurelionPauseMenu.generated.h"

class ASovPlayerController;
class USovSaveSubsystem;
class USovAccessibilityNativeButton;
class UTextBlock;

/** The owned Aurelion controller's pause surface. Campaign slots never enter the legacy Narrative selector. */
UCLASS()
class PROJECTVELKORRAN_API USovAurelionPauseMenu : public UNarrativeMenu
{
    GENERATED_BODY()
public:
    USovAurelionPauseMenu();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual void NativeDestruct() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;
    virtual bool NativeOnHandleBackAction() override;
private:
    friend struct FSovAurelionPauseMenuTestAccess;
    static bool IsAurelionCheckpoint(const FSovSaveSlotHeader& Header, const FString& Account);
    static bool SameCheckpoint(const FSovSaveSlotHeader& A, const FSovSaveSlotHeader& B);
    bool IsCurrent(uint64 Generation, const ASovPlayerController* PC, const USovSaveSubsystem* Save) const;
    void RefreshCheckpoint();
    void Present(const FText& Text);
    void Retire();
    UFUNCTION() void Resume();
    UFUNCTION() void LoadCheckpoint();
    UFUNCTION() void Settings();
    UFUNCTION() void Quit();
    UFUNCTION() void OnLoadResult(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Error);
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Message;
    UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> ResumeButton;
    UPROPERTY(Transient) TObjectPtr<USovAccessibilityNativeButton> LoadButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LoadLabel;
    UPROPERTY(Transient) TArray<TObjectPtr<USovAccessibilityNativeButton>> Buttons;
    TWeakObjectPtr<ASovPlayerController> PausedController;
    TWeakObjectPtr<USovSaveSubsystem> BoundSave;
    FSovSaveSlotHeader DisplayedCheckpoint;
    FName PauseOwner;
    uint64 MenuGeneration = 0;
    bool bOwnPause = false;
    bool bHasCheckpoint = false;
    bool bAcceptRecovery = false;
    bool bSubmitting = false;
};
