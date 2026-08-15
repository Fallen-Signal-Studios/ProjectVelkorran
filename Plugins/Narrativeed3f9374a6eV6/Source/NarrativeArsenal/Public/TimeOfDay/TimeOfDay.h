// Copyright Narrative Tools 2025.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimeOfDay.generated.h"

/// Time of day value with Property Customization, 0.0 - 2400.0
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FTimeOfDay
{
	GENERATED_BODY()

	/// current time of day
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category="TimeOfDay")
	float Time;
	
	// implemented in TimeOfDay.cpp to avoid includes
	FTimeOfDay();
	FTimeOfDay(const float InTime) : Time(InTime) {}

	// allow implicit value get
	operator float() { return Time; }
	operator float() const { return Time; }
};

/// Time of day Range with Property Customization
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FTimeOfDayRange
{
	GENERATED_BODY()

	/// minimum time for this range
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category="TimeOfDayRange")
	float TimeMin;

	/// maximum time for this range
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadOnly, Category="TimeOfDayRange")
	float TimeMax;
	
	FTimeOfDayRange() : TimeMin(RangeMin), TimeMax(RangeMax) {}
	FTimeOfDayRange(const float InTimeMin, const float InTimeMax) : TimeMin(InTimeMin), TimeMax(InTimeMax) {}

	/// min range possible for time value
	static constexpr float RangeMin = 0.0f;
	/// max range possible for time value
	static constexpr float RangeMax = 2400.0f;
	
};

/// blueprint compatible functions for FTimeOfDay and FTimeOfDayRange
UCLASS()
class UTimeOfDayStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/// returns a random value from in the range from Time Of Day Range Min and Time Of Day Range Max
	UFUNCTION(BlueprintCallable, Category="Narrative | Time Of Day")
	static NARRATIVEARSENAL_API float GetRandomTimeInRange(const FTimeOfDayRange& TimeOfDay)
	{
		return FMath::RandRange(TimeOfDay.TimeMin, TimeOfDay.TimeMax);
	}

	/// returns a random value from in the range from Time Of Day Range Min and Time Of Day Range Max using the given Stream
	UFUNCTION(BlueprintCallable, Category="Narrative | Time Of Day")
	static NARRATIVEARSENAL_API float GetRandomTimeInRangeFromStream(const FTimeOfDayRange& TimeOfDay, const FRandomStream& Stream)
	{
		return Stream.FRandRange(TimeOfDay.TimeMin, TimeOfDay.TimeMax);
	}

	/// returns TimeOfDay as double
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Double (TimeOfDay)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static NARRATIVEARSENAL_API double Conv_TimeOfDayToDouble(FTimeOfDay InTimeOfDay) { return InTimeOfDay; }

	/// returns TimeOfDay as float
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Float (TimeOfDay)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static NARRATIVEARSENAL_API float Conv_TimeOfDayToFloat(FTimeOfDay InTimeOfDay) { return InTimeOfDay; }

	/// returns double as TimeOfDay
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Double (TimeOfDay)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static NARRATIVEARSENAL_API FTimeOfDay Conv_DoubleToTimeOfDay(double InFloat) { return InFloat; }

	/// returns float as TimeOfDay
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To Float (TimeOfDay)", CompactNodeTitle = "->", Keywords = "cast convert", BlueprintAutocast), Category = "Math|Conversions")
	static NARRATIVEARSENAL_API FTimeOfDay Conv_FloatToTimeOfDay(float InDouble) { return InDouble; }

	/// returns the 12-hour value for a given time
	UFUNCTION(BlueprintPure, meta = (DisplayName = "To 12 Hour (TimeOfDay)", CompactNodeTitle = "12 Hour", Keywords = "To 12 Hour Get", BlueprintAutocast), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API void To12Hour(const FTimeOfDay& InTimeOfDay, float& TimeOut, bool& bPM)
	{
		if (InTimeOfDay < 1)
		{
			TimeOut = 12;
			return;
		}

		bPM = InTimeOfDay > 12;
		if (bPM)
		{
			TimeOut = InTimeOfDay - 12;
			return;
		}

		TimeOut = InTimeOfDay;
	}

	
	/* FTimeOfDay & FTimeOfDay */
	/// returns true if A is Less than B (A < B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay < TimeOfDay", CompactNodeTitle = "<", Keywords = "< Less"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API bool Less_TimeOfDayTimeOfDay          (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A < B; }
	
	/// returns true if A is Greater than B (A > B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay < TimeOfDay", CompactNodeTitle = ">", Keywords = "> Greater"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API bool Greater_TimeOfDayTimeOfDay       (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A > B; }
	
	/// returns true if A is Less than or equal to B (A <= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay <= TimeOfDay", CompactNodeTitle = "<=", Keywords = "< Less"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API bool LessEqual_TimeOfDayTimeOfDay     (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A <= B; }

	/// returns true if A is Greater than or equal to B (A >= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay >= TimeOfDay", CompactNodeTitle = ">=", Keywords = ">= Greater"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API bool GreaterEqual_TimeOfDayTimeOfDay  (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A >= B; }

	/// returns true if A is exactly equal to B (A == B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay == TimeOfDay", CompactNodeTitle = "==", Keywords = "== Equal"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API bool EqualEqual_TimeOfDayTimeOfDay    (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A == B; }

	/// returns true if A does not equal B (A != B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay != TimeOfDay", CompactNodeTitle = "!=", Keywords = "!= Equal"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API bool NotEqual_TimeOfDayTimeOfDay      (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A != B; }
	
	/// Subtraction (A - B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay - TimeOfDay", CompactNodeTitle = "-", Keywords = "- Subtract"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API FTimeOfDay Subtract_TimeOfDayTimeOfDay(const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A - B; }
	
	/// Addition (A + B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay + TimeOfDay", CompactNodeTitle = "+", Keywords = "+ Add"), Category = "Math|TimeOfDay")
	static NARRATIVEARSENAL_API FTimeOfDay Add_TimeOfDayTimeOfDay     (const FTimeOfDay& A, const FTimeOfDay& B)
	{ return A + B; }
	/* FTimeOfDay & FTimeOfDay */

	
	/* FTimeOfDay & double */
	/// returns true if A is Less than B (A < B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay < Double", CompactNodeTitle = "<", Keywords = "< Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Less_TimeOfDayDouble          (const FTimeOfDay& A, const double& B)
	{ return A < B; }

	/// returns true if A is Greater than B (A > B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay > Double", CompactNodeTitle = ">", Keywords = "> Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Greater_TimeOfDayDouble       (const FTimeOfDay& A, const double& B)
	{ return A > B; }

	/// returns true if A is Less than or equal to B (A <= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay <= Double", CompactNodeTitle = "<=", Keywords = "<= Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool LessEqual_TimeOfDayDouble     (const FTimeOfDay& A, const double& B)
	{ return A <= B; }
	
	/// returns true if A is Greater than or equal to B (A >= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay >= Double", CompactNodeTitle = ">=", Keywords = ">= Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool GreaterEqual_TimeOfDayDouble  (const FTimeOfDay& A, const double& B)
	{ return A >= B; }
	
	/// returns true if A is exactly equal to B (A == B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay == Double", CompactNodeTitle = "==", Keywords = "== Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool EqualEqual_TimeOfDayDouble    (const FTimeOfDay& A, const double& B)
	{ return A == B; }

	/// returns true if A does not equal B (A != B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay != Double", CompactNodeTitle = "!=", Keywords = "!= Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool NotEqual_TimeOfDayDouble      (const FTimeOfDay& A, const double& B)
	{ return A != B; }

	/// Subtraction (A - B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay - Double", CompactNodeTitle = "-", Keywords = "- Subtract"), Category = "Math|Float")
	static NARRATIVEARSENAL_API FTimeOfDay Subtract_TimeOfDayDouble(const FTimeOfDay& A, const double& B)
	{ return A - B; }
	
	/// Addition (A + B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay + Double", CompactNodeTitle = "+", Keywords = "+ Add"), Category = "Math|Float")
	static NARRATIVEARSENAL_API FTimeOfDay Add_TimeOfDayDouble     (const FTimeOfDay& A, const double& B)
	{ return A + B; }
	/* FTimeOfDay & double */

	
	/* double & FTimeOfDay */
	/// returns true if A is Less than B (A < B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double < TimeOfDay", CompactNodeTitle = "<", Keywords = "< Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Less_DoubleTimeOfDay        (const double& B, const FTimeOfDay& A)
	{ return A < B; }

	/// returns true if A is Greater than B (A > B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double > TimeOfDay", CompactNodeTitle = ">", Keywords = "> Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Greater_DoubleTimeOfDay     (const double& B, const FTimeOfDay& A)
	{ return A > B; }

	/// returns true if A is Less than or equal to B (A <= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double <= TimeOfDay", CompactNodeTitle = "<=", Keywords = "<= Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool LessEqual_DoubleTimeOfDay   (const double& B, const FTimeOfDay& A)
	{ return A <= B; }

	/// returns true if A is Greater than or equal to B (A >= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double >= TimeOfDay", CompactNodeTitle = ">=", Keywords = ">= Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool GreaterEqual_DoubleTimeOfDay(const double& B, const FTimeOfDay& A)
	{ return A >= B; }

	/// returns true if A is exactly equal to B (A == B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double == TimeOfDay", CompactNodeTitle = "==", Keywords = "== Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool EqualEqual_DoubleTimeOfDay  (const double& B, const FTimeOfDay& A)
	{ return A == B; }

	/// returns true if A does not equal B (A != B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double != TimeOfDay", CompactNodeTitle = "!=", Keywords = "!= Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool NotEqual_DoubleTimeOfDay    (const double& B, const FTimeOfDay& A)
	{ return A != B; }

	/// Subtraction (A - B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double - TimeOfDay", CompactNodeTitle = "-", Keywords = "- Subtract"), Category = "Math|Float")
	static NARRATIVEARSENAL_API double Subtract_DoubleTimeOfDay  (const double& B, const FTimeOfDay& A)
	{ return A - B; }

	/// Addition (A + B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Double + TimeOfDay", CompactNodeTitle = "+", Keywords = "+ Add"), Category = "Math|Float")
	static NARRATIVEARSENAL_API double Add_DoubleTimeOfDay       (const double& B, const FTimeOfDay& A)
	{ return A + B; }
	/* double & FTimeOfDay */
	
	
	/* FTimeOfDay & float */
	/// returns true if A is Less than B (A < B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay < Float", CompactNodeTitle = "<", Keywords = "< Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Less_TimeOfDayFloat          (const FTimeOfDay& A, const float& B)
	{ return A < B; }
	
	/// returns true if A is Greater than B (A > B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay > Float", CompactNodeTitle = ">", Keywords = "> Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Greater_TimeOfDayFloat       (const FTimeOfDay& A, const float& B)
	{ return A > B; }

	/// returns true if A is Less than or equal to B (A <= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay <= Float", CompactNodeTitle = "<=", Keywords = "<= Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool LessEqual_TimeOfDayFloat     (const FTimeOfDay& A, const float& B)
	{ return A <= B; }
	
	/// returns true if A is Greater than or equal to B (A >= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay >= Float", CompactNodeTitle = ">=", Keywords = ">= Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool GreaterEqual_TimeOfDayFloat  (const FTimeOfDay& A, const float& B)
	{ return A >= B; }
	
	/// returns true if A is exactly equal to B (A == B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay == Float", CompactNodeTitle = "==", Keywords = "== Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool EqualEqual_TimeOfDayFloat    (const FTimeOfDay& A, const float& B)
	{ return A == B; }

	/// returns true if A does not equal B (A != B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay != Float", CompactNodeTitle = "!=", Keywords = "!= Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool NotEqual_TimeOfDayFloat      (const FTimeOfDay& A, const float& B)
	{ return A != B; }

	/// Subtraction (A - B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay - Float", CompactNodeTitle = "-", Keywords = "- Subtract"), Category = "Math|Float")
	static NARRATIVEARSENAL_API FTimeOfDay Subtract_TimeOfDayFloat(const FTimeOfDay& A, const float& B)
	{ return A - B; }

	/// Addition (A + B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "TimeOfDay + Float", CompactNodeTitle = "+", Keywords = "+ Add"), Category = "Math|Float")
	static NARRATIVEARSENAL_API FTimeOfDay Add_TimeOfDayFloat     (const FTimeOfDay& A, const float& B)
	{ return A + B; }
	/* FTimeOfDay & float */

	
	/* float & FTimeOfDay */	
	/// returns true if A is Less than B (A < B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float < TimeOfDay", CompactNodeTitle = "<", Keywords = "< Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Less_FloatTimeOfDay        (const float& B, const FTimeOfDay& A)
	{ return A < B; }
	
	/// returns true if A is Greater than B (A > B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float > TimeOfDay", CompactNodeTitle = ">", Keywords = "> Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool Greater_FloatTimeOfDay     (const float& B, const FTimeOfDay& A)
	{ return A > B; }

	/// returns true if A is Less than or equal to B (A <= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float <= TimeOfDay", CompactNodeTitle = "<=", Keywords = "<= Less"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool LessEqual_FloatTimeOfDay   (const float& B, const FTimeOfDay& A)
	{ return A <= B; }

	/// returns true if A is Greater than or equal to B (A >= B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float >= TimeOfDay", CompactNodeTitle = ">=", Keywords = ">= Greater"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool GreaterEqual_FloatTimeOfDay(const float& B, const FTimeOfDay& A)
	{ return A >= B; }
	
	/// returns true if A is exactly equal to B (A == B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float == TimeOfDay", CompactNodeTitle = "==", Keywords = "== Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool EqualEqual_FloatTimeOfDay  (const float& B, const FTimeOfDay& A)
	{ return A == B; }
	
	/// returns true if A does not equal B (A != B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float != TimeOfDay", CompactNodeTitle = "!=", Keywords = "!= Equal"), Category = "Math|Float")
	static NARRATIVEARSENAL_API bool NotEqual_FloatTimeOfDay    (const float& B, const FTimeOfDay& A)
	{ return A != B; }

	/// Subtraction (A - B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float - TimeOfDay", CompactNodeTitle = "-", Keywords = "- Subtract"), Category = "Math|Float")
	static NARRATIVEARSENAL_API float Subtract_FloatTimeOfDay   (const float& B, const FTimeOfDay& A)
	{ return A - B; }

	/// Addition (A + B)
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Float + TimeOfDay", CompactNodeTitle = "+", Keywords = "+ Add"), Category = "Math|Float")
	static NARRATIVEARSENAL_API float Add_FloatTimeOfDay        (const float& B, const FTimeOfDay& A)
	{ return A + B; }
	/* float & FTimeOfDay */
	
};