// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "NarrativeCheatManager.generated.h"

/**
 * Contains Narrative pro console commands that will be stripped from packaged game.
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeCheatManager : public UCheatManager
{
	GENERATED_BODY()
	
public:

	//Ragdoll for the specified number of seconds. 
	UFUNCTION(Exec)
	void Ragdoll(const float Duration=5.f);
	
	//Gives the specified amount of skill points to the player
	UFUNCTION(Exec)
	void GiveSkillPoints(int32 Points=1);

	//Gives the specified amount of currency to the player
	UFUNCTION(Exec)
	void GiveCurrency(int32 Currency=1);

	//Make our character in/vulnerable
	UFUNCTION(Exec)
	void SetInvulnerable(const bool bIsInvulnerable);
 
	//Advance the ingame time by a certain amount, where 100 = 1 hour 
	UFUNCTION(Exec)
	void AdvanceTime(const float Amount);

	//Advance the ingame time by however many hours is needed to reach the specified time. 
	UFUNCTION(Exec)
	void AdvanceToTime(const float Time);

	//Manually set a given attribute to the given value using its name. 
	UFUNCTION(Exec)
	void SetGameplayAttribute(FString AttributeName, float NewValue);
	
	//Add a gameplay tag to the character 
	UFUNCTION(Exec)
	void AddGameplayTag(FString TagName);

	//Remove a gameplay tag from the character 
	UFUNCTION(Exec)
	void RemoveGameplayTag(FString TagName);

	//Ask the server to change our username. This is what server currently uses as a save game identifier. 
	UFUNCTION(Exec)
	void ChangeUsername(FString NewUsername);
	
	//Add an item to our character
	UFUNCTION(Exec)
	void GiveItem(FString ItemSearchString, int32 Quantity=1);

	/* Sets the given quest to the chosen state. Useful for debugging quests. 
	 * @param bPartyQuests Whether to update a party quest or just a local ones
	 * @param QuestIndex Index of the quest to update (in chronological order ie 0 = first quest we've started)
	 * @param NewState The ID of the new quest state to send the quest to - if none we'll just go to the first state off first current branch 
	 */
	UFUNCTION(Exec)
	void SetQuestState(bool bPartyQuests, const int32 QuestIndex, FName NewState);
	
	//Ask the server to save our player to its own file. 
	UFUNCTION(Exec)
	void MultiplayerSaveWorld();
	
	//Ask the server to save our player to its own file. 
	UFUNCTION(Exec)
	void MultiplayerLoadWorld();
	
	//Ask the server to save our player to its own file. 
	UFUNCTION(Exec)
	void MultiplayerSave();
	
	//Ask the server to save our player to its own file. 
	UFUNCTION(Exec)
	void MultiplayerLoad();

	//Ask the server to save our player to its own file. 
	UFUNCTION(Exec)
	void MultiplayerDeleteSave();
};
