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

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCorruptionMissionPermission
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MissionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) ESovCorruptionBand MaximumBand = ESovCorruptionBand::Trace;
};

/** Explicit source contract; an unpermissioned asset is dormant, including in standalone maps. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovCorruptionProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Corruption") FName SourceId;
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
