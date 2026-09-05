// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "NarrativeInputSettings.generated.h"

/**
 * Subclassed in order to add aim sensitivity
 */
UCLASS(config=GameUserSettings, DisplayName="Narrative Input Settings", Category="Enhanced Input|User Settings")
class NARRATIVEARSENAL_API UNarrativeInputSettings : public UEnhancedInputUserSettings
{
	GENERATED_BODY()
	
public:

	UNarrativeInputSettings();
	UFUNCTION(BlueprintCallable, Category=Settings) void SetCameraSensitivity(float Value);
	UFUNCTION(BlueprintPure, Category=Settings) float GetCameraSensitivity() const;
	UFUNCTION(BlueprintCallable, Category=Settings) void SetGamepadDeadZone(float Value);
	UFUNCTION(BlueprintPure, Category=Settings) float GetGamepadDeadZone() const;
	UFUNCTION(BlueprintCallable, Category=Settings) void SetGamepadAccelerationSeconds(float Value);
	UFUNCTION(BlueprintPure, Category=Settings) float GetGamepadAccelerationSeconds() const;

		
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetAimSensitivity(const float NewAimSensitivity);

	UFUNCTION(BlueprintCallable, Category = Settings)
	float GetAimSensitivity() const;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetInvertVertical(const bool NewInvertVertical);

	UFUNCTION(BlueprintCallable, Category = Settings)
	bool GetInvertVertical() const;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetInvertHorizontal(const bool NewInvertHorizontal);

	UFUNCTION(BlueprintCallable, Category = Settings)
	bool GetInvertHorizontal() const;
protected:
	UPROPERTY(config, SaveGame) float CameraSensitivity = 1.f;
	/** Zero preserves existing authored Enhanced Input dead-zone modifiers. */
	UPROPERTY(config, SaveGame) float GamepadDeadZone = 0.f;
	UPROPERTY(config, SaveGame) float GamepadAccelerationSeconds = 0.f;

	UPROPERTY(config, SaveGame)
	float AimSensitivity;

	UPROPERTY(config, SaveGame)
	bool bInvertVertical;

	UPROPERTY(config, SaveGame)
	bool bInvertHorizontal;
};
