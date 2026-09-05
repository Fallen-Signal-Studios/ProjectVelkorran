// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "Containers/Ticker.h"
#include "Feedback/SovPlatformOutputTypes.h"
#include "SovGameUserSettings.generated.h"

class USovCampaignStateComponent;
class GenericApplication;
struct FDisplayMetrics;
UENUM(BlueprintType)
enum class ESovDifficultyPreset : uint8 { Story, Standard, Veteran, Sovereign, Custom };
UENUM(BlueprintType)
enum class ESovColorVisionPreset : uint8 { Default, Deuteranopia, Protanopia, Tritanopia };
UENUM(BlueprintType)
enum class ESovDialoguePressureMode : uint8 { Standard, Extended, Disabled };

/** Device-independent gameplay/accessibility values. Cosmetic assets and widget layout remain authored. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovUserSettingsSnapshot
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESovDifficultyPreset Preset = ESovDifficultyPreset::Standard;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float IncomingDamageScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EnemyRecoveryScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowCompanionRescue = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DefenseWindowScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ExertionCostScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InputBufferAssistanceSeconds = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MeleeAimAssistStrength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RangedAimAssistStrength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float InteractionHoldScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTapInteractions = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleAim = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleGuard = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleSprint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutomaticSprint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAimSnap = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bProjectileLead = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleAbilityModifier = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AutoCameraStrength = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bDisableCameraShake = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReduceLensEffects = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReduceCorruptionEffects = false;
	// Local accessibility preferences: never part of the eleven-byte gameplay export.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float UIScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubtitleScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSubtitles = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bClosedCaptions = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubtitleBackgroundOpacity = .85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSubtitleSpeakerNames = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSubtitleDirections = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SubtitleCharactersPerLine = 42;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SubtitleMaximumLines = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHighContrastHUD = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESovColorVisionPreset ColorVisionPreset = ESovColorVisionPreset::Default;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverrideTeamColor = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor TeamColor = FLinearColor(.1f, .65f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOverrideThreatColor = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor ThreatColor = FLinearColor(1.f, .3f, .1f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInteractableOutlines = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bWeakPointOutlines = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float OutlineThickness = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNavigationContrast = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bNavigationPulse = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMenuNavigationWrap = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bMenuNarration = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESovDialoguePressureMode DialoguePressureMode = ESovDialoguePressureMode::Standard;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DialogueMinimumReadSeconds = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DialoguePressureExtension = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ControllerAudioVolume = 1.f;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovUserSettingsChanged, const FSovUserSettingsSnapshot&, Settings);

/** Extends Narrative's existing settings; selected in DefaultEngine.ini before any cinematic. */
UCLASS(config=GameUserSettings, configdonotcheckdefaults, BlueprintType)
class PROJECTVELKORRAN_API USovGameUserSettings : public UNarrativeGameUserSettings
{
	GENERATED_BODY()
public:
	static USovGameUserSettings* Get();
	virtual bool UsesVerifiedAccountPreferences() const override { return true; }
	virtual void RegisterOwnedInputSettings(UNarrativeInputSettings* Input) override;
	virtual bool PersistOwnedInputSettings(UNarrativeInputSettings* Input) override;
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void SaveSettings() override;
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings|Feedback") FSovHapticSettings GetHapticSettings() const { return HapticSettings; }
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings|Feedback") bool ApplyHapticSettings(const FSovHapticSettings& Value, FString& Error);
	UPROPERTY(BlueprintAssignable, Category="Sovereign|Settings|Feedback") FSovHapticSettingsChanged OnHapticSettingsChanged;
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings|HDR") FSovHDROutputStatus GetHDROutputStatus();
	/** No desktop calibration receipt or output override is offered on fixed/fullscreen platforms. */
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings|HDR") virtual bool IsDisplayOutputSystemManaged() const;
	/** Application suspension cancels an unconfirmed display change before handing output to the system. */
	bool RevertUnconfirmedHDRPreview();
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings|HDR") FSovHDRCalibration GetHDRCalibration() const { return DisplayCalibration; }
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings|HDR") bool PreviewHDRDisplay(bool bEnable, int32 PeakNits, const FSovHDRCalibration& Calibration, FGuid& Receipt, FString& Error);
	/** Applies engine output immediately; unconfirmed preview automatically reverts after 15 real seconds. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings|HDR") bool PreviewHDRCalibration(bool bEnable, int32 PeakNits, FGuid& Receipt, FString& Error);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings|HDR") bool ConfirmHDRCalibration(FGuid Receipt, FString& Error);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings|HDR") bool RevertHDRCalibration(FGuid Receipt);
	virtual void SetGameplayDifficulty(const ENarrativeGameplayDifficulty NewDifficulty) override;
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings") FSovUserSettingsSnapshot GetSettingsSnapshot() const { return Settings; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings") bool HasCompletedAccessibilitySetup() const { return bAccessibilitySetupCompleted; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings") bool DidLastPreferenceSaveSucceed() const { return bLastPreferenceSaveSucceeded; }
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings") bool AreAccountPreferencesReady() const { return !bAccountPreferencesManaged || bAccountPreferencesLoaded; }
	/** Explicit Continue in the native first-boot menu, never inferred from a gameplay save. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings") bool CompleteAccessibilitySetup();
	/** Validates the complete transaction before mutation, then persists immediately and emits one typed event. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings") bool ApplySettingsSnapshot(const FSovUserSettingsSnapshot& NewSettings, FString& Error);
	/** Presets preserve explicitly chosen assistance/comfort values. Story only increases assistance. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Settings") bool ApplyDifficultyPreset(ESovDifficultyPreset Preset, FString& Error);
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings") FName GetDifficultyId() const;
	UFUNCTION(BlueprintPure, Category="Sovereign|Settings") bool IsSovereignUnlocked() const { return bCampaignCompleted; }
	/** Completion must come from validated campaign history, never a difficulty menu checkbox. */
	bool UnlockSovereignFromCampaign(const USovCampaignStateComponent* Campaign);
	UFUNCTION(BlueprintCallable, Category="Sovereign|Diagnostics") void SetLocalDiagnosticsEnabled(bool bEnabled);
	UFUNCTION(BlueprintPure, Category="Sovereign|Diagnostics") bool IsLocalDiagnosticsEnabled() const { return bLocalDiagnosticsEnabled; }
	UPROPERTY(BlueprintAssignable, Category="Sovereign|Settings") FSovUserSettingsChanged OnUserSettingsChanged;
	virtual float GetIncomingDamageScale() const override { return Settings.IncomingDamageScale; }
	virtual float GetEnemyRecoveryScale() const override { return Settings.EnemyRecoveryScale; }
	virtual float GetDefenseWindowScale() const override { return Settings.DefenseWindowScale; }
	virtual float GetExertionCostScale() const override { return Settings.ExertionCostScale; }
	virtual float GetInputBufferAssistanceSeconds() const override { return Settings.InputBufferAssistanceSeconds; }
	virtual float GetMeleeAimAssistStrength() const override { return Settings.MeleeAimAssistStrength; }
	virtual float GetRangedAimAssistStrength() const override { return Settings.RangedAimAssistStrength; }
	virtual bool IsCompanionRescueAllowed() const override { return Settings.bAllowCompanionRescue; }
	virtual float GetInteractionHoldScale() const override { return Settings.InteractionHoldScale; }
	virtual bool UseTapInteractions() const override { return Settings.bTapInteractions; }
	virtual bool ShouldAimToggle() const override { return Settings.bToggleAim; }
	virtual bool ShouldGuardToggle() const override { return Settings.bToggleGuard; }
	virtual bool ShouldSprintToggle() const override { return Settings.bToggleSprint; }
	virtual bool UseAutomaticSprint() const override { return Settings.bAutomaticSprint; }
	virtual bool UseAimSnap() const override { return Settings.bAimSnap; }
	virtual bool UseProjectileLead() const override { return Settings.bProjectileLead; }
	virtual bool ShouldAbilityModifierToggle() const override { return Settings.bToggleAbilityModifier; }
	virtual float GetAutoCameraStrength() const override { return Settings.AutoCameraStrength; }
	virtual bool IsCameraShakeDisabled() const override { return Settings.bDisableCameraShake; }
	virtual bool IsReducedLensEffectsEnabled() const override { return Settings.bReduceLensEffects; }
	virtual bool IsReducedCorruptionEffectsEnabled() const override { return Settings.bReduceCorruptionEffects; }
	bool CapturePortableSettings(TArray<uint8>& OutData) const;
	static bool ValidatePortableSettings(const TArray<uint8>& Data, FString& Error);
	/** Explicit opt-in import only: current local accessibility/comfort/consent settings are retained. */
	bool RestorePortableSettings(const TArray<uint8>& Data, FString& Error);
	static bool ValidateSnapshot(const FSovUserSettingsSnapshot& Value, bool bSovereignUnlocked, FString& Error);
protected:
	/** Real engine adapter seams permit deterministic automation without driving the test machine's display. */
	virtual FSovHDROutputStatus ReadHDROutput();
	virtual void WriteHDROutput(bool bEnable, int32 PeakNits);
	virtual bool CanApplyHDROutput() const;
	virtual double HDRTime() const;
	virtual bool ReadDisplayCalibration(FSovHDRCalibration& Out) const;
	virtual bool WriteDisplayCalibration(const FSovHDRCalibration& Value);
	virtual void RestoreDisplayCalibration(const FSovHDRCalibration& Expected, const FSovHDRCalibration& Before, float ExpectedUILevel, float BeforeUILevel);
	virtual class IConsoleVariable* FindDisplayCalibrationVariable(const TCHAR* Name) const;
	virtual float DisplayUIBaseNits() const;
	virtual bool CanApplyDisplayCalibration() const;
	virtual void PersistSettings();
	virtual bool ReadPreferenceBank(const FString& Slot, int32 User, TArray<uint8>& Bytes);
	virtual bool WritePreferenceBank(const FString& Slot, int32 User, const TArray<uint8>& Bytes);
private:
	friend struct FSovPlatformOutputTestAccess;
	friend struct FSovAccountSettingsTestAccess;
	friend class USovSaveSubsystem;
	void ObserveVerifiedSettingsOwner(const FString& Namespace, int32 UserIndex, bool bAuthorized);
	void SetPlatformSettingsSuspended(bool bSuspended);
	bool IsVerifiedSettingsOwnerCurrent(const FString& Namespace, int32 UserIndex, uint64 Generation) const;
	bool SaveAccountPreferences();
	void LoadAccountPreferences();
	void ScheduleAccountPreferenceLoad();
	FTSTicker::FDelegateHandle AccountPreferenceLoadTicker;
	void ResetAccountPreferences();
	void CaptureAccountPreferences(TArray<uint8>& Bytes) const;
	bool ApplyAccountPreferences(const TArray<uint8>& Bytes);
	void PersistDeviceSettings();
	FString AccountSettingsNamespace;
	int32 AccountSettingsUserIndex = INDEX_NONE;
	uint64 AccountSettingsOwnerGeneration = 0;
	uint64 AccountSettingsFileGeneration = 0;
	bool bAccountPreferencesManaged = false;
	bool bAccountPreferencesLoaded = false;
	bool bAccountSettingsSuspended = false;
	bool bAccountSettingsIO = false;
	bool bLastPreferenceSaveSucceeded = true;
	TArray<uint8> CommittedAccountPreferences;
	TArray<uint8> AccountInputProfile;
	TWeakObjectPtr<UNarrativeInputSettings> AccountInputSettings;
	bool ApplyOwnedInputProfile();
	bool TickHDRPreview(float DeltaTime);
	bool BeginHDRPreview(bool bEnable, int32 PeakNits, const FSovHDRCalibration* Calibration, FGuid& Receipt, FString& Error);
	void BeginDisplayObservation();
	void EndDisplayObservation();
	void HandleDisplayMetricsChanged(const FDisplayMetrics& Metrics);
	FDelegateHandle DisplayMetricsHandle;
	TWeakPtr<GenericApplication> ObservedDisplayApplication;
	bool bDisplayMetricsInvalidated = false;
	void ApplyConfirmedDisplayCalibration();
	void RestorePreviewDisplayCalibration();
	float PreviewUIBaseNits = 0.f;
	float PreviewUILevel = 0.f;
	float BeforeRenderUILevel = 0.f;
	UPROPERTY(config) FSovHDRCalibration DisplayCalibration;
	UPROPERTY(config) bool bHasDisplayCalibration = false;
	FSovHDRCalibration BeforeDisplayCalibration;
	FSovHDRCalibration BeforeRenderCalibration;
	FSovHDRCalibration PreviewDisplayCalibration;
	bool bPreviewOwnsCalibration = false;
	bool bHadDisplayCalibration = false;
	FString HDRPreviewDisplayIdentity;
	void RemoveHDRPreviewTicker();
	UPROPERTY(config) FSovHapticSettings HapticSettings;
	FGuid HDRPreviewReceipt;
	FSovHDROutputStatus HDRPreviewOutput;
	FTSTicker::FDelegateHandle HDRPreviewTicker;
	double HDRPreviewDeadline = 0.;
	bool bHDRBeforePreview = false;
	int32 HDRNitsBeforePreview = 1000;
	bool bHDRTransaction = false;
	UPROPERTY(config) int32 SettingsSchemaVersion = 1;
	UPROPERTY(config) FSovUserSettingsSnapshot Settings;
	UPROPERTY(config) bool bCampaignCompleted = false;
	UPROPERTY(config) bool bLocalDiagnosticsEnabled = false;
	UPROPERTY(config) bool bAccessibilitySetupCompleted = false;
	bool bApplying = false;
};
