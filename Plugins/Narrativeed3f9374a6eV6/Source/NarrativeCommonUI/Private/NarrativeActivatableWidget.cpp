// Copyright Narrative Tools 2022. 


#include "NarrativeActivatableWidget.h"
#include "Input/CommonUIInputTypes.h"

UNarrativeActivatableWidget::UNarrativeActivatableWidget()
{
	bFocusDesiredTargetOnActivate = true; 
	bDeactivateOnBack = false; 
}

void UNarrativeActivatableWidget::NativeDestruct()
{
	++ActivationGeneration;
	for (FUIActionBindingHandle Handle : BindingHandles)
	{
		if (Handle.IsValid())
		{
			Handle.Unregister();
		}
	}
	BindingHandles.Empty();

	Super::NativeDestruct();
}

TOptional<FUIInputConfig> UNarrativeActivatableWidget::GetDesiredInputConfig() const
{
	//BP can override if required 
	// Check if there is a BP implementation for input configs
	if (GetClass()->IsFunctionImplementedInScript(GET_FUNCTION_NAME_CHECKED(UNarrativeActivatableWidget, BP_GetDesiredInputConfig)))
	{
		return BP_GetDesiredInputConfig();
	}

	switch (InputConfig)
	{
	case ENarrativeWidgetInputMode::GameAndMenu:
		return FUIInputConfig(ECommonInputMode::All, GameMouseCaptureMode);
	case ENarrativeWidgetInputMode::Game:
		return FUIInputConfig(ECommonInputMode::Game, GameMouseCaptureMode);
	case ENarrativeWidgetInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case ENarrativeWidgetInputMode::Default:
	default:
		return TOptional<FUIInputConfig>();
	}
}

void UNarrativeActivatableWidget::RegisterBinding(FDataTableRowHandle InputAction, const FInputActionExecutedDelegate& Callback, FInputActionBindingHandle& BindingHandle, FText OverrideDisplayName, const bool bShouldDisplayInActionBar/*=true*/)
{
	FBindUIActionArgs BindArgs(InputAction, FSimpleDelegate::CreateLambda([InputAction, Callback]()
		{
			Callback.ExecuteIfBound(InputAction.RowName);
		}));
	BindArgs.bDisplayInActionBar = bShouldDisplayInActionBar;
	BindArgs.OverrideDisplayName = OverrideDisplayName;

	BindingHandle.Handle = RegisterUIActionBinding(BindArgs);
	BindingHandles.Add(BindingHandle.Handle);
}

void UNarrativeActivatableWidget::UnregisterBinding(FInputActionBindingHandle BindingHandle)
{
	RemoveActionBinding(BindingHandle.Handle);

	if (BindingHandle.Handle.IsValid())
	{
		BindingHandles.Remove(BindingHandle.Handle);
		BindingHandle.Handle.Unregister();
	}
}

void UNarrativeActivatableWidget::UnregisterAllBindings()
{
	for (FUIActionBindingHandle Handle : BindingHandles)
	{
		RemoveActionBinding(Handle);

		if (Handle.IsValid())
		{
			Handle.Unregister();
		}
	}

	BindingHandles.Empty();
}

void UNarrativeActivatableWidget::SetBindingDisplayName(FInputActionBindingHandle BindingHandle, FText NewDisplayName)
{
	if (BindingHandle.Handle.IsValid())
	{
		BindingHandle.Handle.SetDisplayName(NewDisplayName);
	}
}

void UNarrativeActivatableWidget::SetBindingShowOnActionBar(FInputActionBindingHandle BindingHandle,
	const bool bShowOnActionBar)
{
	if (BindingHandle.Handle.IsValid())
	{
		BindingHandle.Handle.SetDisplayInActionBar(bShowOnActionBar);
	}
}

void UNarrativeActivatableWidget::NativeOnActivated()
{
	const uint64 ExpectedGeneration = ++ActivationGeneration;
	Super::NativeOnActivated();

	if (!IsActivated() || ActivationGeneration != ExpectedGeneration)
	{
		return;
	}

	NativeRegisterActions();
	if (!IsActivated() || ActivationGeneration != ExpectedGeneration)
	{
		return;
	}
	RegisterActions();
	if (IsActivated() && ActivationGeneration == ExpectedGeneration && bFocusDesiredTargetOnActivate)
	{
		//Do not call NativeGetDesiredFocusTarget/SetFocus directly: that bypasses
		//CommonUI's cached restoration target and can steal focus from a newer modal.
		//Refresh after callbacks so dynamically populated controls are available.
		RequestRefreshFocus();
	}
}

void UNarrativeActivatableWidget::NativeOnDeactivated()
{
	++ActivationGeneration;
	UnregisterAllBindings();

	Super::NativeOnDeactivated();
}

void UNarrativeActivatableWidget::NativeRegisterActions()
{

}
