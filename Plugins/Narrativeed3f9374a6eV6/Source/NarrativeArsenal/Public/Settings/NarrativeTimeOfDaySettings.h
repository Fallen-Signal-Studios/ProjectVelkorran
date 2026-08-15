// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TimeOfDay/TimeOfDay.h"
#include "NarrativeTimeOfDaySettings.generated.h"

/**
 * Settings for the Narrative time of day system in the game state. 
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Narrative Time Of Day Settings"))
class NARRATIVEARSENAL_API UNarrativeTimeOfDaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UNarrativeTimeOfDaySettings();

	/** DEPRECATED - Moved to NarrativeGameState settings. If true the time of day will be updated on tick using the Day/NightLengthMinutes values */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Narrative Sky")
	bool bDynamicTimeOfDay;

	/** DEPRECATED - Moved to NarrativeGameState settings.Default time of day we'll start the time at when the game state is initialized.  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Narrative Sky")
	FTimeOfDay DefaultTimeOfDay;

	/** DEPRECATED - Moved to NarrativeGameState settings.Day length in minutes, if a BP_NarrativeSky is in the level. */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Narrative Sky", meta = (EditCondition=bDynamicTimeOfDay, EditConditionHides, ClampMin=0.01))
	float DayLengthMinutes;

	/** DEPRECATED - Moved to NarrativeGameState settings.Night length in minutes, if a BP_NarrativeSky is in the level. */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Narrative Sky", meta = (EditCondition=bDynamicTimeOfDay, EditConditionHides, ClampMin=0.01))
	float NightLengthMinutes;

	/** DEPRECATED - Moved to NarrativeGameState settings.The time the sun should rise - if you have a Narrative sky in the level it will use this */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Narrative Sky", meta = (EditCondition=bDynamicTimeOfDay, EditConditionHides))
	FTimeOfDay SunriseTime;

	/** DEPRECATED - Moved to NarrativeGameState settings.The time the sun should set - if you have a Narrative sky in the level it will use this */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Narrative Sky", meta = (EditCondition=bDynamicTimeOfDay, EditConditionHides))
	FTimeOfDay SunsetTime;
};
