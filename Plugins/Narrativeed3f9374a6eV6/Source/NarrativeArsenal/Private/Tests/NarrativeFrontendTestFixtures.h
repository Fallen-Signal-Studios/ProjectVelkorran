// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/NarrativeMenu.h"
#include "Widgets/NarrativeCommonButtonBase.h"
#include "NarrativeFrontendTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class UNarrativeFrontendTestMenu : public UNarrativeMenu
{
	GENERATED_BODY()
public:
	UWidget* MakeRoot();
	int32 RegistrationCalls = 0;
	mutable int32 DirectFocusQueries = 0;
	bool bDeactivateDuringRegistration = false;
	bool bReactivateDuringRegistration = false;
protected:
	virtual void NativeRegisterActions() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
};

UCLASS(Transient, NotBlueprintable)
class UNarrativeFrontendTestButton : public UNarrativeCommonButtonBase
{
	GENERATED_BODY()
public:
	void BuildNativeButton();
	void SetAuthoredAccessibleText(const FText& Text);
	USlateAccessibleWidgetData* ReadAccessibleData() const { return AccessibleWidgetData; }
};
