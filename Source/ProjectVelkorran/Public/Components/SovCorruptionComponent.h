// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Corruption/SovCorruptionProfile.h"
#include "GameplayEffectTypes.h"
#include "NarrativeSavableComponent.h"
#include "SovCorruptionComponent.generated.h"

class ASovCorruptionSourceVolume;
class UAbilitySystemComponent;
class USovCampaignStateComponent;
class USovCampaignDefinition;

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionSourceHandle
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid Id;
	bool IsValid() const { return Id.IsValid(); }
};
USTRUCT()
struct FSovCorruptionExposureRecord
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TObjectPtr<USovCorruptionProfile> Profile;
	UPROPERTY(SaveGame) FName MissionId;
	UPROPERTY(SaveGame) float Exposure = 0.0f;
	UPROPERTY(SaveGame) bool bContactAtCheckpoint = false;
};
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionReplicatedState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float Exposure = 0.0f;
	UPROPERTY(BlueprintReadOnly) ESovCorruptionBand Band = ESovCorruptionBand::Clear;
	UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<USovCorruptionProfile>> Profiles;
};
/** Mechanical information is identical in reduced-effects mode; presentation never drives input. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionPresentationRequest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) ESovCorruptionBand Band = ESovCorruptionBand::Clear;
	UPROPERTY(BlueprintReadOnly) float Exposure = 0.0f;
	UPROPERTY(BlueprintReadOnly) bool bReducedEffects = false;
	UPROPERTY(BlueprintReadOnly) float SuggestedIntensity = 0.0f;
	UPROPERTY(BlueprintReadOnly) TArray<FName> ProfileIds;
	UPROPERTY(BlueprintReadOnly) TArray<FText> RemedyTexts;
	UPROPERTY(BlueprintReadOnly) TArray<FText> InformationTexts;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovCorruptionPresentation, const FSovCorruptionPresentationRequest&, Request);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSovCorruptionBandChanged, ESovCorruptionBand, Previous, ESovCorruptionBand, Current);

/** Mission-permitted gameplay exposure; Narrative remains the disk and story-state authority. */
UCLASS(ClassGroup=(Sovereign), meta=(BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCorruptionComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
public:
	USovCorruptionComponent();
	FSovCorruptionSourceHandle AcquireSource(ASovCorruptionSourceVolume* Source);
	void ReleaseSource(FSovCorruptionSourceHandle Handle, ASovCorruptionSourceVolume* Source);
	/** Explicit remedy removes gameplay exposure only. SourceId None targets all current profiles. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Corruption")
	bool CleanseExposure(float Amount, FName SourceId = NAME_None);
	UFUNCTION(BlueprintCallable, Category="Corruption|Accessibility") void SetReducedEffects(bool bEnabled);
	UFUNCTION(BlueprintPure, Category="Corruption") FSovCorruptionReplicatedState GetCorruptionState() const { return State; }
	UFUNCTION(BlueprintPure, Category="Corruption|Accessibility") FSovCorruptionPresentationRequest GetPresentationRequest() const;
	UFUNCTION(BlueprintPure, Category="Corruption") bool HasExposureSource() const { return !Sources.IsEmpty(); }
	bool ValidateSourcePermission(ASovCorruptionSourceVolume* Source, ESovCorruptionBand& OutCap, FName& OutMission) const;
	/** Restore barrier: call after the destination campaign record is authoritative, before character readiness. */
	bool FinishCampaignRestore(const USovCampaignDefinition* Destination, FString& OutError);
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(BlueprintAssignable, Category="Corruption") FSovCorruptionBandChanged OnBandChanged;
	UPROPERTY(BlueprintAssignable, Category="Corruption|Presentation") FSovCorruptionPresentation OnPresentationRequested;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(EditDefaultsOnly, Category="Corruption|Bands") float TraceThreshold = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category="Corruption|Bands") float IntrusionThreshold = 25.0f;
	UPROPERTY(EditDefaultsOnly, Category="Corruption|Bands") float ContestThreshold = 50.0f;
	UPROPERTY(EditDefaultsOnly, Category="Corruption|Bands") float OverwriteThreshold = 80.0f;
	UPROPERTY(EditDefaultsOnly, Category="Corruption|Bands") float BandHysteresis = 5.0f;
private:
	friend struct FSovCorruptionTestAccess;
	struct FSourceEntry
	{
		TWeakObjectPtr<ASovCorruptionSourceVolume> Source;
		TWeakObjectPtr<USovCorruptionProfile> Profile;
		FName MissionId;
	};
	UFUNCTION() void OnRep_State(FSovCorruptionReplicatedState Previous);
	bool ValidOwner() const;
	bool ValidBandTuning() const;
	USovCampaignStateComponent* Campaign() const;
	float ExposureCap(ESovCorruptionBand Cap) const;
	float TotalExposure() const;
	void Accumulate(USovCorruptionProfile* Profile, FName MissionId, float Amount, ESovCorruptionBand Cap);
	void RefreshState(bool bCommitConsequences = false);
	void RemoveOwnedBandEffect();
	void CommitAuthoredConsequences();
	void RefreshSavedSnapshot();
	void PruneRestoredContacts();
	UPROPERTY(ReplicatedUsing=OnRep_State) FSovCorruptionReplicatedState State;
	UPROPERTY(Transient) TArray<FSovCorruptionExposureRecord> Records;
	UPROPERTY(SaveGame) int32 SavedSchemaVersion = 1;
	UPROPERTY(SaveGame) TArray<FSovCorruptionExposureRecord> SavedRecords;
	UPROPERTY(SaveGame) ESovCorruptionBand SavedBand = ESovCorruptionBand::Clear;
	TMap<FGuid, FSourceEntry> Sources;
	TSet<FName> RestoredContactProfiles;
	TWeakObjectPtr<UAbilitySystemComponent> EffectASC;
	FActiveGameplayEffectHandle BandEffect;
	ESovCorruptionBand AppliedEffectBand = ESovCorruptionBand::Clear;
	bool bReducedEffects = false;
	bool bMutating = false;
	bool bEnding = false;
	bool bWaitingForMissionRestore = false;
	bool bRestoredDataValid = true;
	bool bForceBandEffectRefresh = false;
	ESovCorruptionBand RestoreBandSeed = ESovCorruptionBand::Clear;
};
