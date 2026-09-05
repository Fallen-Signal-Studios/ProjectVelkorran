// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include <Delegates/DelegateCombinations.h>
#include "GameplayTagContainer.h"
#include "NarrativeActivatableWidget.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FInputActionExecutedDelegate, FName, ActionName);

//We use a strategy similar to lyra, but allow for an override if BP wants. 
UENUM(BlueprintType)
enum class ENarrativeWidgetInputMode : uint8
{
	Default,
	GameAndMenu,
	Game,
	Menu
};

USTRUCT(BlueprintType)
struct FInputActionBindingHandle
{
	GENERATED_BODY()
	
public:
	struct FUIActionBindingHandle Handle;
};

UCLASS(meta = (DisableNativeTick))
class NARRATIVECOMMONUI_API UNarrativeActivatableWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:

	friend class UNarrativeGameplayHUD;

	UNarrativeActivatableWidget();

	virtual void NativeDestruct() override;

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Narrative Activatable Widget")
	void RegisterBinding(FDataTableRowHandle InputAction, const FInputActionExecutedDelegate& Callback, FInputActionBindingHandle& BindingHandle, FText OverrideDisplayName, const bool bShouldDisplayInActionBar=true);

	UFUNCTION(BlueprintCallable, Category = "Narrative Activatable Widget")
	void UnregisterBinding(FInputActionBindingHandle BindingHandle);

	UFUNCTION(BlueprintCallable, Category = "Narrative Activatable Widget")
	void UnregisterAllBindings();

	UFUNCTION(BlueprintCallable, Category = "Narrative Activatable Widget")
	void SetBindingDisplayName(FInputActionBindingHandle BindingHandle, FText NewDisplayName);

	UFUNCTION(BlueprintCallable, Category = "Narrative Activatable Widget")
	void SetBindingShowOnActionBar(FInputActionBindingHandle BindingHandle, const bool bShowOnActionBar);

	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UFUNCTION(BlueprintImplementableEvent)
	void RegisterActions();

	virtual void NativeRegisterActions();
	
protected:

	//If owner has any of these tags we'll block the widget from being added to the GameplayHUD. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Activatable Widget")
	FGameplayTagContainer BlockTags;

	//Should we deactivate when back key is pressed. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Activatable Widget")
	bool bDeactivateOnBack;

	//Ask CommonUI to restore its cached focus, or the authored desired target, on activation.
	//CommonUI remains responsible for deciding which active layer may receive focus.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Activatable Widget")
	bool bFocusDesiredTargetOnActivate;

	//Useful if this menu is being used in a tab switcher and you need a name ID to pass the switcher. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Activatable Widget")
	FName OptionalNameID;

	//Useful if this menu is being used in a tab switcher and you need a display name
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Activatable Widget")
	FText OptionalDisplayName;

protected:

	/** The desired input mode to use while this UI is activated, for example do you want key presses to still reach the game/player controller? */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	ENarrativeWidgetInputMode InputConfig = ENarrativeWidgetInputMode::Default;

	/** The desired mouse behavior when the game gets input. */
	UPROPERTY(EditDefaultsOnly, Category = Input)
	EMouseCaptureMode GameMouseCaptureMode = EMouseCaptureMode::CapturePermanently;

private:

	TArray<struct FUIActionBindingHandle> BindingHandles;

	//Activation callbacks may deactivate/reactivate the same instance synchronously.
	uint64 ActivationGeneration = 0;

};
