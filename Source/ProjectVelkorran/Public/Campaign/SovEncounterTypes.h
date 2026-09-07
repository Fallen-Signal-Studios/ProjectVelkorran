// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NarrativeSave.h"
#include "SovEncounterTypes.generated.h"

class UPlayerDefinition;
class APawn;
class UGameplayAbility;

/** Read-only kit evidence used to curate an inactive protagonist's separate companion ASC. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovProtagonistAbilitySnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) TSubclassOf<UGameplayAbility> AbilityClass;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 Level = 1;
};

/** Resolved values support UI/legacy saves; schema 2 separately stores underlying GAS bases. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCombatResourceSnapshot
{
	GENERATED_BODY()
	/** Default 1 admits records written before explicit bases existed. Capture writes version 2. */
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 SchemaVersion = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseHealth = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseShield = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseStamina = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BasePoise = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseEcho = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseMaxHealth = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseMaxShield = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseMaxStamina = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseMaxPoise = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float BaseMaxEcho = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Health = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Shield = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Stamina = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Poise = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float Echo = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float MaxHealth = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float MaxShield = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float MaxStamina = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float MaxPoise = 0.f;
	UPROPERTY(SaveGame, BlueprintReadOnly) float MaxEcho = 0.f;
	bool IsValid() const;
};

/** One authored protagonist, stored within Narrative's existing PlayerState record. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovProtagonistSnapshot
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 SchemaVersion = 1;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTag ProtagonistTag;
	UPROPERTY(SaveGame, BlueprintReadOnly) TSoftClassPtr<APawn> PawnClass;
	UPROPERTY(SaveGame, BlueprintReadOnly) TSoftObjectPtr<UPlayerDefinition> PlayerDefinition;
	UPROPERTY(SaveGame, BlueprintReadOnly) FSovCombatResourceSnapshot Resources;
	UPROPERTY(SaveGame) FNarrativeActorRecord PawnRecord;
	UPROPERTY(SaveGame) FNarrativeSaveComponent SkillTreeRecord;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTagContainer Factions;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTagContainer WieldEquipSlots;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTagContainer WieldSlots;
	UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FSovProtagonistAbilitySnapshot> GrantedAbilities;
	bool IsValid() const;
};

/** The native source of an encounter objective, persisted with its campaign receipt. */
UENUM(BlueprintType)
enum class ESovEncounterProofType : uint8
{
	RequiredDefeats, AurelionLinks, AurelionThermalFracture
};

UENUM(BlueprintType)
enum class ESovEncounterState : uint8
{
	Inactive, Active, Succeeded, Failed, Restoring
};
