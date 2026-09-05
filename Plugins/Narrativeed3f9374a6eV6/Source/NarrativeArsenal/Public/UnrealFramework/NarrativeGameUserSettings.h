// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "GameplayTagContainer.h"
#include "NarrativeGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class ENarrativeGameplayDifficulty : uint8
{
	Easy,
	Medium,
	Hard,
	Insane
};

//Opted for enum so users can add more dialogue levels 
UENUM(BlueprintType)
enum class ENarrativeSubtitleLevel : uint8
{
	Disabled,
	DialogueOnly,
	Enabled
};

/**
 * Since GameUserSettings doesnt support sound class overrides, in Narrative pro we've extended it to do so.
 */
UCLASS(config = GameUserSettings, configdonotcheckdefaults, BlueprintType)
class NARRATIVEARSENAL_API UNarrativeGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()
	
public:

	UNarrativeGameUserSettings();

	/** Project extension seam; neutral defaults preserve other Narrative consumers. */
	static const UNarrativeGameUserSettings* GetSovSettings();
	virtual float GetIncomingDamageScale() const { return 1.f; }
	virtual float GetEnemyRecoveryScale() const { return 1.f; }
	virtual float GetDefenseWindowScale() const { return 1.f; }
	virtual float GetExertionCostScale() const { return 1.f; }
	virtual float GetInputBufferAssistanceSeconds() const { return 0.f; }
	virtual float GetMeleeAimAssistStrength() const { return 0.f; }
	virtual float GetRangedAimAssistStrength() const { return 0.f; }
	virtual bool IsCompanionRescueAllowed() const { return true; }
	virtual float GetInteractionHoldScale() const { return 1.f; }
	virtual bool UseTapInteractions() const { return false; }
	virtual bool ShouldAimToggle() const { return false; }
	virtual bool ShouldGuardToggle() const { return false; }
	virtual bool ShouldSprintToggle() const { return false; }
	virtual bool UseAutomaticSprint() const { return false; }
	virtual bool UseAimSnap() const { return false; }
	virtual bool UseProjectileLead() const { return false; }
	virtual bool ShouldAbilityModifierToggle() const { return false; }
	virtual float GetAutoCameraStrength() const { return 0.f; }
	virtual bool IsCameraShakeDisabled() const { return false; }
	virtual bool IsReducedLensEffectsEnabled() const { return false; }
	virtual bool IsReducedCorruptionEffectsEnabled() const { return false; }


	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void ApplyNonResolutionSettings() override; 

	virtual void ApplySoundSettings();
	virtual void ApplyMonitorSelection();

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetOverallAudioVolume(const float NewOverallAudioVolume);

	UFUNCTION(BlueprintCallable, Category = Settings)
	float GetOverallAudioVolume() const;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetDialogueAudioVolume(const float NewDialogueAudioVolume);

	UFUNCTION(BlueprintCallable, Category = Settings)
	float GetDialogueAudioVolume() const;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetUIAudioVolume(const float NewUIAudioVolume);

	UFUNCTION(BlueprintCallable, Category = Settings)
	float GetUIAudioVolume() const;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetSFXAudioVolume(const float NewSFXAudioVolume);

	UFUNCTION(BlueprintCallable, Category = Settings)
	float GetSFXAudioVolume() const;

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetMusicAudioVolume(const float NewMusicAudioVolume);

	UFUNCTION(BlueprintCallable, Category = Settings)
	float GetMusicAudioVolume() const;

	//Set whether or not crouching is a toggle or whether crouch key requires held. 
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetShouldCrouchToggle(const bool bNewCrouchToggles);

	UFUNCTION(BlueprintCallable, Category = Settings)
	bool ShouldCrouchToggle();

	//Set whether or not inventory menu is set to tile. 
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetInventoryWantsTile(const bool bNewInventoryWantsTile);

	UFUNCTION(BlueprintCallable, Category = Settings)
	bool InventoryWantsTile();

		//Set whether or EnableBloom.
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetEnableBloom(const bool bNewEnableBloom);

	UFUNCTION(BlueprintCallable, Category = Settings)
	bool WantsEnableBloom();

			//Set whether or EnableMotionBlur.
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetEnableMotionBlur(const bool bNewEnableMotionBlur);

	UFUNCTION(BlueprintCallable, Category = Settings)
	bool WantsEnableMotionBlur();

	//Set the current gameplay difficulty
	UFUNCTION(BlueprintCallable, Category = Settings)
	virtual void SetGameplayDifficulty(const ENarrativeGameplayDifficulty NewDifficulty);

	UFUNCTION(BlueprintCallable, Category = Settings)
	ENarrativeGameplayDifficulty GetGameplayDifficulty();
	
	//Set the subtitle level we want. 
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetSubtitleLevel(const ENarrativeSubtitleLevel NewLevel);

	UFUNCTION(BlueprintCallable, Category = Settings)
	ENarrativeSubtitleLevel GetSubtitleLevel();

	UFUNCTION(BlueprintPure, Category = Settings)
	FString GetSelectedMonitor();

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetSelectedMonitor(const FString NewSelectedMonitor);

	UFUNCTION(BlueprintPure, Category = Settings)
	float GetFieldOfView();

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetFieldOfView(const float NewFieldOfView);

	UFUNCTION(BlueprintPure, Category = Settings)
	float GetWeaponFieldOfView();

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetWeaponFieldOfView(const float NewWeaponFieldOfView);

	UFUNCTION(BlueprintPure, Category = Settings)
	float GetGamma();

	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetGamma(const float NewGamma);

	UFUNCTION(BlueprintPure, Category = Settings)
	FString GetOnlineUsername();
	
	UFUNCTION(BlueprintCallable, Category = Settings)
	void SetOnlineUsername(const FString Username);
	
protected:

	UPROPERTY(config)
	float OverallAudioVolume;

	UPROPERTY(config)
	float DialogueAudioVolume;

	UPROPERTY(config)
	float UIAudioVolume;

	UPROPERTY(config)
	float SFXAudioVolume;

	UPROPERTY(config)
	float MusicAudioVolume;

	///**If true, bloom will be allowed in the camera views rendering settings.  */
	UPROPERTY(config)
	bool bEnableBloom;

	///**If true, motion blur will be allowed in the camera views rendering settings.  */
	UPROPERTY(config)
	bool bEnableMotionBlur;

	///**If true, crouch button toggles crouch, otherwise it needs to be held for as long as you require the crouch. */
	UPROPERTY(config)
	bool bCrouchToggles;

	///** Whether the inventory menu wants to display using grid or tile mode */
	UPROPERTY(config)
	bool bInventoryWantsTile;

	//The gameplay difficulty, can be read from the user settings by any gameplay elements that need it. 
	UPROPERTY(config)
	ENarrativeGameplayDifficulty GameplayDifficulty;
	
	//The subtitle level being used in game. 
	UPROPERTY(config)
	ENarrativeSubtitleLevel SubtitleLevel;

	//The monitor we want to use in our video settings. 
	UPROPERTY(config)
	FString SelectedMonitor;

	///** The FOV the default camera mode will use. */
	UPROPERTY(config)
	float FieldOfView;

	///** The Weapon FOV the default camera mode will use. */
	UPROPERTY(config)
	float WeaponFieldOfView;

	//* The Gamma to use */
	UPROPERTY(config)
	float Gamma;

	//* Default username to use online */
	UPROPERTY(config)
	FString OnlineUsername;
};
