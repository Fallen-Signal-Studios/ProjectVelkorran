// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GAS/SovCombatTypes.h"
#include "Settings/SovGameUserSettings.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "SovDiagnosticsSubsystem.generated.h"

class UNarrativeAbilitySystemComponent;
class USovEchoComponent;
class UGameplayAbility;
struct FAbilityEndedData;
struct FOnAttributeChangeData;
UENUM(BlueprintType)
enum class ESovDiagnosticKind : uint8
{
	Settings, Damage, Fatal, ShieldBreak, PoiseBreak, HealthRecovery, EchoGain, EchoSpend, EchoThreshold,
	AbilityBegin, AbilityCommit, AbilityEnd, AbilityCancel, EncounterState, MissionState, BeatCommit, Evidence,
	CompanionRescue, CoAction, CameraFailure, LockFailure, ResourceStarvation, Interaction, Cinematic
};
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovDiagnosticRecord
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ESovDiagnosticKind Kind = ESovDiagnosticKind::Settings;
	UPROPERTY(BlueprintReadOnly) float Seconds = 0.f;
	/** Authored semantic ID or ability class name only. Never actor names, dialogue text, file paths or account IDs. */
	UPROPERTY(BlueprintReadOnly) FName SourceId;
	UPROPERTY(BlueprintReadOnly) FName ContextId;
	UPROPERTY(BlueprintReadOnly) float Amount = 0.f;
	UPROPERTY(BlueprintReadOnly) float Remaining = 0.f;
	UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
};

/** Bounded, opt-in local design diagnostics. No network, automatic file export, user IDs or shipping collection. */
UCLASS()
class PROJECTVELKORRAN_API USovDiagnosticsSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USovDiagnosticsSubsystem, STATGROUP_Tickables); }
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	static void Record(UWorld* World, ESovDiagnosticKind Kind, FName SourceId, FName ContextId = NAME_None,
		float Amount = 0.f, float Remaining = 0.f, bool bSucceeded = false);
	UFUNCTION(BlueprintPure, Category="Sovereign|Diagnostics") TArray<FSovDiagnosticRecord> GetRecentRecords() const { return Records; }
	/** Explicit QA action writes a compact report under Saved/Diagnostics. No arbitrary destination input. */
	UFUNCTION(BlueprintCallable, Category="Sovereign|Diagnostics") bool ExportLocalReport(FString& OutRelativePath, FString& Error) const;
	UFUNCTION(BlueprintCallable, Category="Sovereign|Diagnostics") void ClearRecords();
	static bool IsSafeDebugId(FName Id);
private:
	void RefreshBindings();
	void UnbindPlayer();
	void Append(FSovDiagnosticRecord Value);
	bool IsRecordingEnabled() const;
	UFUNCTION() void HandleDamage(const FSovDamageResult& Result);
	UFUNCTION() void HandleEchoGain(FGameplayTag Source, float Amount, float Remaining);
	UFUNCTION() void HandleEchoSpend(FGameplayTag Source, float Amount, float Remaining);
	UFUNCTION() void HandleEchoThreshold(bool bActive, float Current);
	UFUNCTION() void HandleSettings(const FSovUserSettingsSnapshot& Value);
	UFUNCTION() void HandleMission(FName MissionId, bool bSucceeded);
	UFUNCTION() void HandleBeat(const FSovCampaignJournalEntry& Entry);
	UFUNCTION() void HandleEvidence(const FSovEvidenceAcquisition& Evidence);
	void HandleAbilityBegin(UGameplayAbility* Ability);
	void HandleAbilityCommit(UGameplayAbility* Ability);
	void HandleAbilityEnd(const FAbilityEndedData& Data);
	void HandleHealth(const FOnAttributeChangeData& Change);
	UPROPERTY(Transient) TArray<FSovDiagnosticRecord> Records;
	UPROPERTY(Transient) TWeakObjectPtr<UNarrativeAbilitySystemComponent> BoundASC;
	UPROPERTY(Transient) TWeakObjectPtr<USovEchoComponent> BoundEcho;
	UPROPERTY(Transient) TWeakObjectPtr<USovCampaignStateComponent> BoundCampaign;
	UPROPERTY(Transient) TWeakObjectPtr<USovGameUserSettings> BoundSettings;
	TArray<FGuid> RecentDamageIds;
	FDelegateHandle AbilityBeginHandle, AbilityCommitHandle, AbilityEndHandle, HealthHandle;
	float BindingElapsed = 0.f;
};
