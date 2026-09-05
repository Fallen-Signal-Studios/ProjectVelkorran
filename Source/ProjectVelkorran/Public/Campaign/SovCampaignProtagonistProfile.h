// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SovCampaignProtagonistProfile.generated.h"
class ASovPlayerCharacterBase;
class UPlayerDefinition;
class ASovProtagonistCompanionCharacter;
class UNPCDefinition;
class UGameplayAbility;

/** An explicitly authored second kit for a controlled convergence handoff. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignProtagonistProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag Protagonist;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftClassPtr<ASovPlayerCharacterBase> PawnClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UPlayerDefinition> PlayerDefinition;
};

USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovCampaignCompanionProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag Protagonist;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CompanionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftClassPtr<ASovProtagonistCompanionCharacter> CompanionClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UNPCDefinition> CompanionDefinition;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TSubclassOf<UGameplayAbility>> CuratedCompanionAbilities;
	/** Exactly one actor in the convergence map carries this tag for initial companion entry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName EntryAnchorTag;
};
