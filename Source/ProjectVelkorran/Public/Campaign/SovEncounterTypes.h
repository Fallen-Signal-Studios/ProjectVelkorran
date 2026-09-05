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

/** Values are captured for inspection. Restore clamps currents to the current definition's maxima. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCombatResourceSnapshot
{
	GENERATED_BODY()
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

UENUM(BlueprintType)
enum class ESovEncounterState : uint8
{
	Inactive, Active, Succeeded, Failed, Restoring
};
