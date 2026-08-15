// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NarrativeCommonHUD.h"
#include "NarrativeMenu.h"
#include "NarrativeGameplayHUD.generated.h"

/**
 * Base class for gameplay HUD with narrative. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeGameplayHUD : public UCommonUserWidget
{
	GENERATED_BODY()
	
public:

	UNarrativeGameplayHUD();

	virtual void NativeConstruct() override;

	/*Set whether the hide should be hidden or not
	@param bHideEvenEssentialWidgets whether this should effect even essential widgets, like the dialogue options etc. */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Notifications")
	void SetHUDHidden(const bool bHideHUD, const bool bHideEvenEssentialWidgets);
	
	//Show a notification - the UMG widget handles implementing this. bOverrideCurrentNotification is only relevant for major notifications - minors are shown in a stack. 
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Notifications")
	void ShowNotification(const FText& NotificationText, const float Duration = 5.f);

	//Show a major notification
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Notifications")
	void ShowMajorNotification(const FText& NotificationText, const FText& MajorNotificationSubtext, const float Duration = 5.f, const bool bOverrideCurrentNotification = true);

	//Open a menu of some kind.  
	UFUNCTION(BlueprintCallable, Category = "Notifications", meta = (DeterminesOutputType="MenuClass"))
	virtual UNarrativeMenu* OpenMenu(TSubclassOf<UNarrativeMenu> MenuClass, UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag);

	/** grab a layers container. */
	UFUNCTION(BlueprintPure, Category="Layer")
	UCommonActivatableWidgetContainerBase* GetLayerContainer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag);

protected:

	/** Register a layer that widgets can be pushed onto. */
	UFUNCTION(BlueprintCallable, Category="Layer")
	void RegisterLayer(UPARAM(meta = (Categories = "UI.Layer")) FGameplayTag LayerTag, UCommonActivatableWidgetContainerBase* LayerWidget);

	////stack used for showing prompts to the user
	//UPROPERTY(BlueprintReadOnly, Category = "Common UI", meta = (BindWidget))
	//TObjectPtr<UCommonActivatableWidgetStack> PromptStack;

	////stack used for showing widget menus to the user
	//UPROPERTY(BlueprintReadOnly, Category = "Common UI", meta = (BindWidget))
	//TObjectPtr<UCommonActivatableWidgetStack> WidgetStack;

	//The narrative player controller that owns this widget
	UPROPERTY(BlueprintReadOnly, Category = "Common UI")
	TObjectPtr<class ANarrativePlayerController> OwnerPC;

	//Widgets in here will not be hidden when we ask the UI to hide, usually during cinematics. 
	//TODO possibly use a tag mapping to widget so we can hide various layers 
	UPROPERTY(BlueprintReadWrite, Category = "Common UI")
	TArray<TObjectPtr<class UWidget>> EssentialWidgets;

	// The registered layers for the primary layout. This technique is used in lyra and nicely layers our UI for easy management. 
	UPROPERTY(Transient, meta = (Categories = "UI.Layer"))
	TMap<FGameplayTag, TObjectPtr<UCommonActivatableWidgetContainerBase>> Layers;

};
