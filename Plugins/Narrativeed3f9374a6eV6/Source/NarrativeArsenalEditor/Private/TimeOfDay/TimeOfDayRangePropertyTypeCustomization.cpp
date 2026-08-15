// Copyright Narrative Tools 2025.

#include "TimeOfDayRangePropertyTypeCustomization.h"
#include "DetailWidgetRow.h"
#include "TimeOfDayPropertyTypeUtils.h"
#include "TimeRuler.h"
#include "TimeOfDay/TimeOfDay.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "TimeOfDay_PropertyTypeUtils"

FTimeOfDayRangePropertyTypeCustomization::FTimeOfDayRangePropertyTypeCustomization()
{
	// add keys to the range so that the slider can be used
	// must be 0 - 1 as that is all the slider accepts
	MinSliderMinKeyHandle = MinSliderRange.GetRichCurve()->AddKey(0.0f, 0.0f);
	MinSliderMaxKeyHandle = MinSliderRange.GetRichCurve()->AddKey(1.0f, 1.0f);
	MaxSliderMinKeyHandle = MaxSliderRange.GetRichCurve()->AddKey(0.0f, 0.0f);
	MaxSliderMaxKeyHandle = MaxSliderRange.GetRichCurve()->AddKey(1.0f, 1.0f);

}

void FTimeOfDayRangePropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TimeMinPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTimeOfDayRange, TimeMin));
	TimeMaxPropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTimeOfDayRange, TimeMax));
	
	HeaderRow
	.WholeRowWidget
	[
		SNew(SHorizontalBox)
		// row value
		+SHorizontalBox::Slot()
		.FillWidth(1)
		.Padding(WidgetPadding)
		.MaxWidth(500)
		[
			SNew(SVerticalBox)
			
			// property titles
			+SVerticalBox::Slot()
			.FillContentHeight(1)
			.Padding(0.0f, 0.0f, 0.0f, WidgetPadding)
			[
				// min property title
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Left)
				[
					TimeMinPropertyHandle->CreatePropertyNameWidget()
				]

				// variable name
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Center)
				[
					PropertyHandle->CreatePropertyNameWidget()
				]

				// max property title
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Right)
				[
					TimeMaxPropertyHandle->CreatePropertyNameWidget()
				]
			]
			
			/* range min and max editor values */
			+SVerticalBox::Slot()
			.FillContentHeight(1)
			.Padding(0.0f, 0.0f, 0.0f, WidgetPadding)
			[
				SNew(SHorizontalBox)
				// range min
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SBox)
					.MaxDesiredWidth(MinMaxNumericEntryBoxWidth)
					.MinDesiredWidth(MinMaxNumericEntryBoxWidth)
					.WidthOverride(MinMaxNumericEntryBoxWidth)
					[
						SNew(SNumericEntryBox<float>)
						.MinValue(FTimeOfDayRange::RangeMin)
						.MaxValue(FTimeOfDayRange::RangeMax)
						.MaxFractionalDigits(2)
						.Value_Lambda([this]()
						{
							return GetTimeMinValue();
						})
						.OnValueCommitted_Lambda([this](const float NewValue, ETextCommit::Type CommitType)
						{
							SetTimePropertyValue(TimeMinPropertyHandle, NewValue);
							
							if (TimeRuler.IsValid())
							{
								TimeRuler->SetMinValue(NORMALISE_TO_MAX_TIME_RANGE(GetTimeMinValue()));
							}
						})
					]
				]

				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(TimeRuler, STimeRuler)
					.StepSize(NORMALISE_TO_MAX_TIME_RANGE(100.0f))
					.RangeMinValue_Lambda([this]()
					{
						return NORMALISE_TO_MAX_TIME_RANGE(GetTimeMinValue());
					})
					.OnMinValueChanged_Lambda([this](const float NewValue)
					{
						SetTimePropertyValue(TimeMinPropertyHandle, TIME_FROM_NORMALISED_VALUE(NewValue));
					})
					.RangeMaxValue_Lambda([this]()
					{
						return NORMALISE_TO_MAX_TIME_RANGE(GetTimeMaxValue());
					})
					.OnMaxValueChanged_Lambda([this](const float NewValue)
					{
						SetTimePropertyValue(TimeMaxPropertyHandle, TIME_FROM_NORMALISED_VALUE(NewValue));
					})
				]
				
				// range max
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(SBox)
					.MaxDesiredWidth(MinMaxNumericEntryBoxWidth)
					.MinDesiredWidth(MinMaxNumericEntryBoxWidth)
					.WidthOverride(MinMaxNumericEntryBoxWidth)
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							// time max value entry box
							SNew(SNumericEntryBox<float>)
							.MinValue(FTimeOfDayRange::RangeMin)
							.MaxValue(FTimeOfDayRange::RangeMax)
							.MaxFractionalDigits(2)
							.Value_Lambda([this]()
							{
								return GetTimeMaxValue();
							})
							.OnValueCommitted_Lambda([this](const float NewValue, ETextCommit::Type CommitType)
							{
								SetTimePropertyValue(TimeMaxPropertyHandle, NewValue);
	
								if (TimeRuler.IsValid())
								{
									TimeRuler->SetMaxValue(NORMALISE_TO_MAX_TIME_RANGE(GetTimeMaxValue()));
								}
							})
						]
					]	
				]
			]
			/* range min and max editor values */
		]
	];
}

float FTimeOfDayRangePropertyTypeCustomization::GetTimeMinValue() const
{
	float TimeValue = 0.0f;
	if (TimeMinPropertyHandle.IsValid())
	{
		TimeMinPropertyHandle->GetValue(TimeValue);
	}
	return TimeValue;
}

float FTimeOfDayRangePropertyTypeCustomization::GetTimeMaxValue() const
{
	float TimeValue = 0.0f;
	if (TimeMaxPropertyHandle.IsValid())
	{
		TimeMaxPropertyHandle->GetValue(TimeValue);
	}
	return TimeValue;
}

void FTimeOfDayRangePropertyTypeCustomization::SetTimePropertyValue(TSharedPtr<IPropertyHandle>& Property, const float InValue)
{
	if (Property.IsValid())
	{
		Property->SetValue(FMath::Clamp(InValue, FTimeOfDayRange::RangeMin, FTimeOfDayRange::RangeMax));
	}
}

#undef LOCTEXT_NAMESPACE
