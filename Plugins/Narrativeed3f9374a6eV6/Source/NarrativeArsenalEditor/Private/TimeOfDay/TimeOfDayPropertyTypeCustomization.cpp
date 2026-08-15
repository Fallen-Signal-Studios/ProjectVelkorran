// Copyright Narrative Tools 2025.

#include "TimeOfDayPropertyTypeCustomization.h"
#include "DetailWidgetRow.h"
#include "TimeRuler.h"
#include "TimeOfDay/TimeOfDay.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "TimeOfDay/TimeOfDayPropertyTypeUtils.h"

#define LOCTEXT_NAMESPACE "TimeOfDay_PropertyTypeCustomization"

void FTimeOfDayPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TimePropertyHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTimeOfDay, Time));
	
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
				PropertyHandle->CreatePropertyNameWidget()
			]
			
			/* value and time slider */
			+SVerticalBox::Slot()
			.FillContentHeight(1)
			.Padding(0.0f, 0.0f, 0.0f, WidgetPadding)
			[
				SNew(SHorizontalBox)
				
				// time value
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
							return GetTimeValue();
						})
						.OnValueCommitted_Lambda([this](const float NewValue, ETextCommit::Type CommitType)
						{
							SetTimePropertyValue(NewValue);
							
							if (TimeRuler.IsValid())
							{
								TimeRuler->SetMinValue(NORMALISE_TO_MAX_TIME_RANGE(GetTimeValue()));
							}
						})
					]
				]

				// time ruler
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(TimeRuler, STimeRuler)
					.StepSize(NORMALISE_TO_MAX_TIME_RANGE(SliderStepValue))
					.Value_Lambda([this]()
					{
						return NORMALISE_TO_MAX_TIME_RANGE(GetTimeValue());
					})
					.OnValueChanged_Lambda([this](const float NewValue)
					{
						SetTimePropertyValue(TIME_FROM_NORMALISED_VALUE(NewValue));
					})
				]
				
			]
			/* value and time slider */
		]
	];	
	
}

float FTimeOfDayPropertyTypeCustomization::GetTimeValue() const
{
	float TimeValue = 0.0f;
	if (TimePropertyHandle.IsValid())
	{
		TimePropertyHandle->GetValue(TimeValue);
	}
	return TimeValue;
}

void FTimeOfDayPropertyTypeCustomization::SetTimePropertyValue(const float InValue)
{
	if (TimePropertyHandle.IsValid())
	{
		TimePropertyHandle->SetValue(FMath::Clamp(InValue, FTimeOfDayRange::RangeMin, FTimeOfDayRange::RangeMax));
	}
}

void FTimeOfDayPropertyTypeCustomization::DecrementValue()
{
	if (TimePropertyHandle.IsValid())
	{
		SetTimePropertyValue(GetTimeValue() - SliderStepValue);
	}
}

void FTimeOfDayPropertyTypeCustomization::IncrementValue()
{
	if (TimePropertyHandle.IsValid())
	{
		SetTimePropertyValue(GetTimeValue() + SliderStepValue);
	}
}

FText FTimeOfDayPropertyTypeCustomization::GetButtonTooltipText() const
{
	return GetFormattedTime(GetTimeValue(), false);
}

#undef LOCTEXT_NAMESPACE
