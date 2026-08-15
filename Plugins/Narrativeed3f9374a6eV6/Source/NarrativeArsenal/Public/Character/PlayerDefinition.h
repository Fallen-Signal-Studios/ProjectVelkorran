// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Character/CharacterDefinition.h"
#include "PlayerDefinition.generated.h"

/**
 * Defines a player character that will be player controlled 
 */
UCLASS()
class NARRATIVEARSENAL_API UPlayerDefinition : public UCharacterDefinition
{
	GENERATED_BODY()

public:

	UPlayerDefinition();
	
	/**The name of this player. Character creator data will override this if player has some. Override ApplyAppearance to change this behavior. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "NPC")
	FText PlayerDisplayName;

};
