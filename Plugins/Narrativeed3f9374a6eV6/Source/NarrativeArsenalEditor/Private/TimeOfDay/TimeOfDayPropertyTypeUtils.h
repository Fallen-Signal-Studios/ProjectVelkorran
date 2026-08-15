// Copyright Narrative Tools 2025.

#pragma once

#define NORMALISE_TO_MAX_TIME_RANGE(Value) (Value / FTimeOfDayRange::RangeMax)
#define TIME_FROM_NORMALISED_VALUE(Value) (Value * FTimeOfDayRange::RangeMax)

#define LOCTEXT_NAMESPACE "TimeOfDay_PropertyTypeUtils"

static FText GetFormattedTime(float TimeValue, bool b24Hour, bool bDisplayFormatTag = true)
{
	// ensure that we always display in the format of 00, 01, etc...
	FNumberFormattingOptions Options;
	Options.MaximumIntegralDigits = 2;
	Options.MinimumIntegralDigits = 2;

	// get hours and minutes
	const int32 Hours24 = static_cast<int32>(TimeValue / 100);
	const int32 Mins = static_cast<int32>(TimeValue) % 100;

	if (b24Hour)
	{
		return FText::Format(LOCTEXT("TimeOfDayUtils_Get24H_Format", "{0}:{1} {2}"),
			FText::AsNumber(Hours24, &Options), FText::AsNumber(Mins, &Options), bDisplayFormatTag? LOCTEXT("TimeOfDayUtils_24H", "[12H]") : INVTEXT(""));
	}

	// convert to 12 hour
	int32 Hours12 = Hours24 % 12;
	if (Hours12 == 0)
	{
		Hours12 = 12;
	}
	FText AmPm = Hours24 < 12? LOCTEXT("TimeOfDayUtils_Get12H_AM", "AM") : LOCTEXT("TimeOfDayUtils_Get12H_PM", "PM");
	return FText::Format(LOCTEXT("TimeOfDayUtils_Get12H_Format", "{0}:{1} {2} {3}"),
		FText::AsNumber(Hours12, &Options), FText::AsNumber(Mins, &Options), AmPm, bDisplayFormatTag? LOCTEXT("TimeOfDayUtils_12H", "[12H]") : INVTEXT(""));
}

#undef LOCTEXT_NAMESPACE