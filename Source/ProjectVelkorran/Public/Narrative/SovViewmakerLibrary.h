// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovViewmakerLibrary.generated.h"
class APlayerController;
class USovCompanionComponent;
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovViewmakerScanResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly) bool bLiveSystem = false;
	UPROPERTY(BlueprintReadOnly) bool bEvidenceAcquired = false;
	UPROPERTY(BlueprintReadOnly) bool bWeakPointsRevealed = false;
	UPROPERTY(BlueprintReadOnly) FName LinkId;
	UPROPERTY(BlueprintReadOnly) TArray<TObjectPtr<AActor>> VisibleLinkedActors;
	UPROPERTY(BlueprintReadOnly) TArray<FName> AuthoredTraceIds;
	UPROPERTY(BlueprintReadOnly) FText RouteHint;
};
/** Selene's bounded inspection layer over existing evidence, weak-point, command-link and companion systems. */
UCLASS()
class PROJECTVELKORRAN_API USovViewmakerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Investigation|Viewmaker")
	static bool ScanTarget(APlayerController* Player, AActor* Target, FSovViewmakerScanResult& OutResult);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Investigation|Viewmaker")
	static bool RequestCompanionAnalysis(APlayerController* Player, AActor* Target, USovCompanionComponent* Companion, FString& OutReason);
};
