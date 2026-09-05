// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SovCorruptionProfile.generated.h"

UENUM(BlueprintType)
enum class ESovCorruptionBand : uint8 { Clear, Trace, Intrusion, Contest, OverwriteRisk };
UENUM(BlueprintType)
enum class ESovCorruptionEscape : uint8 { LeaveField, BreakLink, DestroyNode, ProtectSignal, AuthoredCountermeasure, CompleteObjective };

UENUM(BlueprintType)
enum class ESovCorruptionSourceKind : uint8 { Environment, EnemyAttack, CommandLink, ContaminatedAlly, Machinery };

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionMissionPermission
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MissionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovCorruptionBand MaximumBand = ESovCorruptionBand::Trace;
	/** Explicit mission opt-in. Expiry can fail only this encounter while it owns the exposed player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAllowOverwriteEncounterFailure = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName OverwriteEncounterId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="600")) float OverwriteSeconds = 30.0f;
};

/** Explicit source contract; an unpermissioned asset is dormant, including in standalone maps. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovCorruptionProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") FName SourceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") ESovCorruptionSourceKind SourceKind = ESovCorruptionSourceKind::Environment;
	/** Contaminated allies must actually carry this authored state; proximity alone cannot invent contamination. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") FGameplayTag ContaminatedAllyState;
	/** Engineering defaults: percentage points of resistance lost and remaining stamina regeneration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Combat", meta=(ClampMin="0",ClampMax="25")) float IntrusionVulnerability = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Combat", meta=(ClampMin="0.1",ClampMax="1")) float ContestStaminaRegenScale = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption", meta=(ClampMin="0")) float ExposurePerSecond = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption", meta=(ClampMin="0")) float ContactExposure = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption", meta=(ClampMin="1")) float Radius = 500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") bool bLinearFalloff = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") bool bRequiresLineOfSight = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") FGameplayTagContainer AllowedProtagonists;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") ESovCorruptionBand MaximumBand = ESovCorruptionBand::Trace;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") TArray<FSovCorruptionMissionPermission> MissionPermissions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Remedy") ESovCorruptionEscape Escape = ESovCorruptionEscape::LeaveField;
	/** Dissipation applies only after every contact using this profile has ended. Other remedies require explicit cleanse. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Remedy", meta=(ClampMin="0")) float EscapeRecoveryPerSecond = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Remedy") FText RemedyText;
	/** Optional durable remedy already authored in this mission. Completion disables this source and cleanses its gameplay exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Remedy") FName EscapeBeatId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Presentation") FName PresentationProfileId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Presentation") FText InformationText;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Presentation") FText ReducedEffectsSubstitute;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Presentation", meta=(ClampMin="0",ClampMax="1")) float PresentationIntensity = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Save") bool bPersistExposureAtCheckpoint = false;
	/** Separate story consequence: cleansing exposure never changes this existing campaign record. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Consequence") bool bCanonPersistent = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Consequence") FName ConsequenceBeatId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption|Consequence") ESovCorruptionBand ConsequenceBand = ESovCorruptionBand::Contest;
	UFUNCTION(BlueprintPure, Category="Corruption") bool ValidateProfile(FString& OutError) const;
	bool PermissionForMission(FName MissionId, ESovCorruptionBand& OutCap) const;
};
