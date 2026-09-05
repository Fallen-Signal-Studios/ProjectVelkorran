// Copyright Narrative Tools. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "NarrativeBotAttackSelection.generated.h"

class UNarrativeCombatAbility;
class ANarrativeNPCController;
class UNarrativeAbilitySystemComponent;

/** Snapshot only: handles, availability and targets must be revalidated on activation. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FNarrativeBotAttackCandidate
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	FGameplayAbilitySpecHandle Handle;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	TSubclassOf<UNarrativeCombatAbility> AbilityClass;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	FGameplayTag InputTag;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	float MinimumRange = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	float MaximumRange = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	float PreferredRange = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	float Frequency = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	float Priority = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	bool bInRange = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	bool bHasLineOfSight = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	bool bCanActivate = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	bool bRequiresAttackToken = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	bool bManagesAttackToken = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Bot Combat")
	FGameplayTagContainer FailureTags;
	// Authority-local deterministic tie breakers, not a persistent gameplay identity.
	uint64 LastUsedSerial = 0;
	int32 GrantOrder = 0;
};

/** A lease owned by one exact activation. Never releases another behavior's token. */
struct FNarrativeBotAttackLease
{
	TWeakObjectPtr<ANarrativeNPCController> Controller;
	TWeakObjectPtr<UNarrativeAbilitySystemComponent> Target;
	uint64 Serial = 0;
	bool bNewlyAcquired = false;
	TWeakObjectPtr<UObject> Coordinator;
	FGuid CoordinationReservation;
};
