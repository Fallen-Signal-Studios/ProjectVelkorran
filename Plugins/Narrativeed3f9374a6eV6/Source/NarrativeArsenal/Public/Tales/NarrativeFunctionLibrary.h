// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeNodeSelector.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NarrativeFunctionLibrary.generated.h"

/**
 * General functions used by narrative 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	/**
	* Grab the narrative component from the local pawn or player controller, whichever it exists on. 
	* 
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UTalesComponent* GetTalesComponent(const UObject* WorldContextObject);

	/**
	* Find the narrative component from the supplied target object. 
	*
	* @return The narrative component.
	*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (DefaultToSelf = "Target"))
	static class UTalesComponent* GetTalesComponentFromTarget(AActor* Target);

	/**
	* Calls CompleteNarrativeTask on the narrative component
	*
	* @return Whether the task updated a quest 
	*/
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative", meta = (DisplayName = "Complete Narrative Data Task", BlueprintInternalUseOnly = "true"))
	static bool CompleteNarrativeDataTask(class UTalesComponent* Target, const class UNarrativeDataTask* Task, const FString& Argument, const int32 Quantity = 1);

	/**
	Use this when you want to log a data task, but don't need a data task asset. For example if you tracked player finding items you'd create a "FindItem" data task asset,
	but sometimes you just want to track something super random and creating a whole task asset is overkill and just storing the argument is good enough */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative", meta = (DisplayName = "Complete Loose Narrative Data Task"))
	static bool CompleteLooseNarrativeDataTask(class UTalesComponent* Target, const FString & Argument, const int32 Quantity = 1);

	//Grab a narrative task by its name. Try use asset references instead of this if possible, since an task being renamed will break your code
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta = (WorldContext = "WorldContextObject"))
	static class UNarrativeDataTask* GetTaskByName(const UObject* WorldContextObject, const FString& EventName);

	//Just used by narrative UI, BP exposed FName::NameToDisplayString
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative")
	static FString MakeDisplayString(const FString& String);

	/* dialogue selector */
	// makes a dialogue node selector, guaranteeing the use of the node selection list
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(Keywords="Dialogue Make Selector"))
	static FDialogueNodeSelector MakeDialogueNodeSelector(FDialogueNodeSelector Selector);
	
	// makes a dialogue node selector from a specific node ID
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(CompactNodeTitle="From ID", Keywords="Dialogue Make From ID Selector"))
	static FDialogueNodeSelector MakeDialogueNodeSelectorFromID(FName NodeID);
	
	// takes a dialogue selector, and get the node ID from it
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(CompactNodeTitle="Get ID", Keywords="Dialogue Get ID Selector"))
	static void BreakDialogueNodeSelector(const FDialogueNodeSelector& Selector, FName& NodeID);

	// Gets the node ID for a given dialogue node selector
	UFUNCTION(BlueprintPure, DisplayName="Dialogue Node Selector To Name", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FName Conv_DialogueNodeSelectorToName(const FDialogueNodeSelector& Selector);

	// Gets a dialogue node selector from a given node ID
	UFUNCTION(BlueprintPure, DisplayName="Name To Dialogue Node Selector", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FDialogueNodeSelector Conv_NameToDialogueNodeSelector(const FName& NodeID);
	/* dialogue selector */
	
	/* quest state selector */
	// makes a quest node selector, guaranteeing the use of the node selection list
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(Keywords="Quest Make Selector"))
	static FQuestStateSelector MakeQuestStateSelector(FQuestStateSelector Selector);
	
	// makes a quest state selector from a specific node ID
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(CompactNodeTitle="From ID", Keywords="Quest Make From ID Selector"))
	static FQuestStateSelector MakeQuestStateSelectorFromID(FName NodeID);
	
	// takes a quest state selector, and get the node ID from it
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(CompactNodeTitle="Get ID", Keywords="Quest Get ID Selector"))
	static void BreakQuestStateSelector(const FQuestStateSelector& Selector, FName& NodeID);

	// Gets the state ID for a given quest node selector
	UFUNCTION(BlueprintPure, DisplayName="Quest State Selector To Name", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FName Conv_QuestStateSelectorToName(const FQuestStateSelector& Selector);

	// Gets a quest state selector from a given node ID
	UFUNCTION(BlueprintPure, DisplayName="Name To Quest State Selector", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FQuestStateSelector Conv_NameToQuestStateSelector(const FName& NodeID);
	/* quest state selector */

	/* quest branch selector */
	// makes a quest branch selector, guaranteeing the use of the node selection list
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(Keywords="Quest Make Selector"))
	static FQuestBranchSelector MakeQuestBranchSelector(FQuestBranchSelector Selector);
	
	// makes a quest branch selector from a specific node ID
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(CompactNodeTitle="From ID", Keywords="Quest Make From ID Selector"))
	static FQuestBranchSelector MakeQuestBranchSelectorFromID(FName NodeID);
	
	// takes a quest branch selector, and get the node ID from it
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative", meta=(CompactNodeTitle="Get ID", Keywords="Quest Get ID Selector"))
	static void BreakQuestBranchSelector(const FQuestBranchSelector& Selector, FName& NodeID);

	// Gets the branch ID for a given quest node selector
	UFUNCTION(BlueprintPure, DisplayName="Quest Branch Selector To Name", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FName Conv_QuestBranchSelectorToName(const FQuestBranchSelector& Selector);

	// Gets a quest branch selector from a given node ID
	UFUNCTION(BlueprintPure, DisplayName="Name To Quest Branch Selector", Category = "Utilities", meta=(CompactNodeTitle = "->", BlueprintThreadSafe, BlueprintAutocast))
	static FQuestBranchSelector Conv_NameToQuestBranchSelector(const FName& NodeID);
	/* quest branch selector */
	
};
