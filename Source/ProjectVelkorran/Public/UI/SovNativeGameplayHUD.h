// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "SovNativeGameplayHUD.generated.h"

/** Asset-free default for the existing Narrative HUD owner, not a second UI router. */
UCLASS()
class PROJECTVELKORRAN_API USovNativeGameplayHUD : public UNarrativeGameplayHUD
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<class UCommonActivatableWidgetStack> GameLayer;
    UPROPERTY(Transient) TObjectPtr<class UCommonActivatableWidgetStack> MenuLayer;
    UPROPERTY(Transient) TObjectPtr<class UCommonActivatableWidgetStack> ModalLayer;
};
