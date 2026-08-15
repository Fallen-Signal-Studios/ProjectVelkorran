// Copyright Narrative Tools 2025.

#pragma once

#include "IPropertyTypeCustomization.h"

class STimeRuler;

/// IPropertyTypeCustomization for FTimeOfDay
class FTimeOfDayPropertyTypeCustomization : public IPropertyTypeCustomization 
{
	TSharedPtr<IPropertyHandle> TimePropertyHandle;
	const float SliderStepValue = 100.0f;
	// widget padding common value
	const float WidgetPadding = 5.0f;
	// min width for the popup
	const float MinDesiredWidth = 200.0f;
	// min / max size the numeric entry can be
	const float MinMaxNumericEntryBoxWidth = 55.0f;
	// time ruler with range display
	TSharedPtr<STimeRuler> TimeRuler;
	
public:

	FTimeOfDayPropertyTypeCustomization() {}
	
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FTimeOfDayPropertyTypeCustomization);
	}

	/* IPropertyTypeCustomization */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	/* IPropertyTypeCustomization */

private:

	// returns 0.0 if TimePropertyHandle is invalid
	float GetTimeValue() const;
	void SetTimePropertyValue(float InValue);
	void DecrementValue();
	void IncrementValue();
	FText GetButtonTooltipText() const;
	
};
