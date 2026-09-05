// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "NarrativeActivatableWidget.h"
#include "NarrativeMenu.generated.h"

/**
 * Base class for menus - screens that can be activated and show some content, and closed later. Added to the gameplay HUD. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeMenu : public UNarrativeActivatableWidget
{
	GENERATED_BODY()

public:
	UNarrativeMenu();

	/** Wrap at this menu's outer boundary; explicit child navigation remains authoritative. */
	UFUNCTION(BlueprintCallable, Category = "Narrative Menu|Accessibility")
	void SetMenuNavigationWrap(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Narrative Menu|Accessibility")
	bool IsMenuNavigationWrapEnabled() const { return bMenuNavigationWrap; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Menu|Accessibility")
	bool bMenuNavigationWrap = true;

private:
	void RefreshMenuNavigation();
	void ReleaseMenuNavigation();

	TWeakObjectPtr<UWidget> NavigationRoot;
	TWeakObjectPtr<class UWidgetNavigation> OwnedNavigation;
	uint8 OwnedWrapDirections = 0;
};
