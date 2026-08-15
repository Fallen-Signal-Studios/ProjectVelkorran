// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeCondition.generated.h"

//How do we handle running this condition on a party dialogue?
UENUM(BlueprintType)
enum class EPartyConditionPolicy : uint8
{

	/**Dont run this condition on our party. */
	None, 
	
	/**The condition is run on all of your party members - if anyone in the party passes the condition, it passes. */
	AnyPlayerPasses UMETA(DisplayName = "Any Party Member Passes"),

	/**The condition is run on your party itself - your parties narrative component. If it passes, the condition passes. */
	PartyPasses UMETA(DisplayName="Party Passes"),

	/**The condition is run on all of your party members - everyone needs to pass for the condition to pass. */
	AllPlayersPass UMETA(DisplayName = "All Party Members Pass"),

	/**The condition is run on the party leader - if they pass the condition, the condition passes. */
	PartyLeaderPasses UMETA(DisplayName = "Party Leader Passes")
};

/**
* Used for picking who we can run the conditions on 
*/
UENUM(BlueprintType)
enum class EConditionFilter : uint8
{
	//Select this if your condition isn't supposed to run on a given character - ie checking time of day or something that doesn't need a character
	CF_DontTarget UMETA(DisplayName="No Target"),
	//Select this if your condition can be ran on any character
	CF_AnyCharacter UMETA(DisplayName="Anyone"),
	//Select this if your condition can be ran only on NPCs, ie checking some AI related state. 
	CF_OnlyNPCs UMETA(DisplayName="Only NPCs"),
	//Select this if your condition can only be ran on player characters, and not AI. 
	CF_OnlyPlayers UMETA(DisplayName="Only Players")
};


/**
 * Narrative Conditions allow you to make conditions that dialogues and quests can then use to conditionally include/exclude nodes.
 * 
 * For example, you could make a condition "HasItem" that checks if the player has a certain amount of an item. Then, you could use that
 * condition on a dialogue node so that the player can only say "I'll buy it!" if they actually have 500 coins in their inventory. 
 * 
 * Currently quests do not support NarrativeConditions, only NarrativeEvents. Dialogues support both. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVEARSENAL_API UNarrativeCondition : public UObject
{
	GENERATED_BODY()

public:

	UNarrativeCondition();

	// Allows the Object to get a valid UWorld from it's outer.
	virtual UWorld* GetWorld() const override
	{
		if (HasAllFlags(RF_ClassDefaultObject))
		{
			// If we are a CDO, we must return nullptr instead of calling Outer->GetWorld() to fool UObject::ImplementsGetWorld.
			return nullptr;
		}

		UObject* Outer = GetOuter();

		while (Outer)
		{
			UWorld* World = Outer->GetWorld();
			if (World)
			{
				return World;
			}

			Outer = Outer->GetOuter();
		}

		return nullptr;
	}

	/** Check whether this condition is true or false
	
	@param Target This is the target we want to check the condition on . 
	@param Controller The controller of the player we're running the condition on - will be null if running the condition against a party
	@param NarrativeComponent The narrative component we're running the condition on 
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Conditions")
	bool CheckCondition(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent);
	virtual bool CheckCondition_Implementation(APawn* Target, APlayerController* Controller, class UTalesComponent* NarrativeComponent);

	/**Define the text that will show up on a node if this condition is added to it */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Conditions")
	FString GetGraphDisplayText();
	virtual FString GetGraphDisplayText_Implementation();

	//Set this to true to flip the result of this condition
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conditions")
	bool bNot = false;
	
	/**
	Defines what types of characters the event should be allowed to run on 
	*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition")
	EConditionFilter ConditionFilter;
	
	/**
	Defines how the condition should be ran against a party that is doing this dialogue. If we're not in a party, the condition will be ran on the owning player and the party will be ignored. 

	Imagine if you want to check if your party has completed a quest. By default narrative will check if anyone in the party had completed 
	the quest on their own narrative component, but if you wanted to check if the party itself had completed the quest before you'd check this box.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parties")
	EPartyConditionPolicy PartyConditionPolicy = EPartyConditionPolicy::AnyPlayerPasses;
	
	//return the characters to run the event on 
	UFUNCTION(BlueprintCallable, Category = "Event")
	TArray<class UCharacterDefinition*> GetCharacterTargets() const;

protected:

	/**Characters we should run the Condition on. If empty, we'll run on the character that owns the quest/dialogue.*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (EditCondition = "ConditionFilter == EConditionFilter::CF_AnyCharacter", EditConditionHides))
	TArray<TObjectPtr<class UCharacterDefinition>> CharacterTargets;

	/** NPCs we should run the Condition on. If empty, we'll run the Condition on all NPCs in the dialogue */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (EditCondition = "ConditionFilter == EConditionFilter::CF_OnlyNPCs", EditConditionHides))
	TArray<TObjectPtr<class UNPCDefinition>> NPCTargets;

	/** Players we should run the Condition on. If empty, we'll run on the character that owns the quest/dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition", meta = (EditCondition = "ConditionFilter == EConditionFilter::CF_OnlyPlayers", EditConditionHides))
	TArray<TObjectPtr<class UPlayerDefinition>> PlayerTargets;
	

};
