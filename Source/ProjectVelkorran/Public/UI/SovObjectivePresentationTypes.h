// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Campaign/SovObjectiveTypes.h"
#include "SovObjectivePresentationTypes.generated.h"

/** Transient, already-authorized view of the current protagonist's actionable goal. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovObjectivePresentationEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName BeatId;
	UPROPERTY(BlueprintReadOnly) FText Text;
	UPROPERTY(BlueprintReadOnly) FText FailureRule;
	UPROPERTY(BlueprintReadOnly) ESovObjectiveState State = ESovObjectiveState::Inactive;
	UPROPERTY(BlueprintReadOnly) bool bOptional = false;
	UPROPERTY(BlueprintReadOnly) bool bCanonGate = false;
};
