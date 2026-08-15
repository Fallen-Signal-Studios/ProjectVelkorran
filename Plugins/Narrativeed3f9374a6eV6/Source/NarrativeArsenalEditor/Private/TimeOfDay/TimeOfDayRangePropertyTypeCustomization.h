// Copyright Narrative Tools 2025.

#pragma once

#include "IPropertyTypeCustomization.h"

class STimeRuler;
class SRadialSlider;

/// IPropertyTypeCustomization for FTimeOfDayRange
class FTimeOfDayRangePropertyTypeCustomization : public IPropertyTypeCustomization 
{
	TSharedPtr<IPropertyHandle> TimeMinPropertyHandle;
	TSharedPtr<IPropertyHandle> TimeMaxPropertyHandle;
	const float SliderStepValue = 100.0f;
	// widget common padding value
	const float WidgetPadding = 5.0f;
	// min / max size the numeric entry can be
	const float MinMaxNumericEntryBoxWidth = 55.0f;
	// radial slider range curves
	FRuntimeFloatCurve MinSliderRange;
	FKeyHandle MinSliderMinKeyHandle;
	FKeyHandle MinSliderMaxKeyHandle;
	FRuntimeFloatCurve MaxSliderRange;
	FKeyHandle MaxSliderMinKeyHandle;
	FKeyHandle MaxSliderMaxKeyHandle;
	// time min radial
	TSharedPtr<SRadialSlider> MinRadial;
	// time max radial
	TSharedPtr<SRadialSlider> MaxRadial;
	// time ruler with range display
	TSharedPtr<STimeRuler> TimeRuler;
		
public:

	FTimeOfDayRangePropertyTypeCustomization();
	
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FTimeOfDayRangePropertyTypeCustomization);
	}

	/* IPropertyTypeCustomization */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}
	/* IPropertyTypeCustomization */
	
private:
	
	float GetTimeMinValue() const;
	float GetTimeMaxValue() const;
	void SetTimePropertyValue(TSharedPtr<IPropertyHandle>& Property, const float InValue);	
};
