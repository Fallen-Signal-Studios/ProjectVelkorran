// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NarrativeThreatMemory.generated.h"

UENUM(BlueprintType)
enum class ENarrativeThreatSource : uint8
{
	Sight, Hearing, Damage, AllyAlert, NetworkSensor, EchoCorruption, Command, ViewmakerSpoof
};

/** A finite observation, not a continuously tracked actor position or saved aggro score. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FNarrativeThreatMemory
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Threat") TWeakObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") ENarrativeThreatSource Source = ENarrativeThreatSource::Sight;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") float Strength = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") float Confidence = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") FVector LastKnownPosition = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") double ObservedAt = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") double ExpiresAt = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") bool bDirectObservation = false;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") TWeakObjectPtr<AActor> SharedBy;
	UPROPERTY(BlueprintReadOnly, Category = "Threat") FGameplayTagContainer SharedFactions;
	// Local throttling metadata, deliberately neither saved nor replicated.
	double LastSharedAt = -2.0;
};
