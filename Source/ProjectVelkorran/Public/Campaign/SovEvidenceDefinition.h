// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Campaign/SovNarrativeTypes.h"
#include "SovEvidenceDefinition.generated.h"

/** Canonical information and permitted provenance chain. Acquisition remains in CampaignState's existing record sequence. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovEvidenceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EvidenceId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CanonicalContentId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Summary;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText FullText;
	/** Optional editorial separation. Empty legacy records remain explicitly unclassified, never asserted as fact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText ObservedFacts;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Interpretation;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<class USoundBase> OptionalAudio;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName OriginalCustodian;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> SourceCustodians;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> AuthenticationAuthorities;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> CopyDestinations;
	/** Explicit control domain for each custodian; distributing inside the original institution is not independent distribution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, FName> CustodianInstitutions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> SupportingEvidenceIds;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Claims;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> Contradictions;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> KnownAlterations;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> RelevantMissions;
	/** Required records must use mandatory native beat acquisition; optional discoveries cannot secretly gate canon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bCriticalPath = false;
	UFUNCTION(BlueprintPure, Category="Evidence") bool ValidateDefinition(FString& OutError) const;
};
