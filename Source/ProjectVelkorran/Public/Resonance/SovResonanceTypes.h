// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "SovResonanceTypes.generated.h"

UENUM(BlueprintType)
enum class ESovResonanceType : uint8 { SupportSever, FormationBreach, TerminalRelease, AdvanceCorridor };
UENUM(BlueprintType)
enum class ESovResonanceState : uint8 { Idle, Offered, Committed, Succeeded, Canceled };

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovResonanceInteraction
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid InteractionId;
	UPROPERTY(BlueprintReadOnly) ESovResonanceType Type = ESovResonanceType::SupportSever;
	UPROPERTY(BlueprintReadOnly) ESovResonanceState State = ESovResonanceState::Idle;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Partner;
	UPROPERTY(BlueprintReadOnly) float ExpiresAt = 0.f;
	UPROPERTY(BlueprintReadOnly) float Pressure = 0.f;
	UPROPERTY(BlueprintReadOnly) FString Reason;
};
