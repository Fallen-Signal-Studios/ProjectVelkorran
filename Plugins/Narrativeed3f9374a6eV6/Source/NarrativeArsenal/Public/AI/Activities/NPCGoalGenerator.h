// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NPCGoalItem.h"
#include "NPCGoalGenerator.generated.h"

//A saved NPC Activity - which consists of the class and the uint8 data to serialize
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSavedNPCGoalGenerator
{

	GENERATED_USTRUCT_BODY()

	FSavedNPCGoalGenerator(){};

	UPROPERTY(SaveGame)
	TSubclassOf<UNPCGoalGenerator> Class;

	UPROPERTY(SaveGame)
	TArray<uint8> Data;
};


/**
 * Goal generators generate goals for your NPCs to do. They provide a bit of optimization in that you can only add the generators your 
 * NPC requires so this will remove lots of unnesseccary processing time. They also allow for configuration, ie your legendary enemy could 
 * have further distance for its GoalGenerator_FindAttackTarget, whereas your novice enemy might have a shorter distance. 
 * 
 * Goal generators should be added to your NPCs definition. They will then be registered with the NPCs Activity Component. 
 * For more information, please check out the docs: https://docs.narrativetools.io/
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVEARSENAL_API UNPCGoalGenerator : public UObject
{
	GENERATED_BODY()
	
public:
	
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


	UNPCGoalGenerator(const FObjectInitializer& ObjectInitializer);

	virtual void Initialize(class ANarrativeNPCController* OwnerController, class UNPCActivityComponent* OwnerComp);

	//Use this event to set your generator up, bind it to an event, etc.
	UFUNCTION(BlueprintNativeEvent, Category = "Goal Generator")
	void InitializeGoalGenerator();
	virtual void InitializeGoalGenerator_Implementation();

	/**
	 * Reconsider the actors this NPC ALREADY perceives, because an input to the decision
	 * changed rather than the perception itself.
	 *
	 * Generators decide from more than the perception event: faction membership, attitude and
	 * their own configuration all feed the same predicate. When one of those changes after an
	 * actor was perceived, no new perception event arrives - UE suppresses the same-state
	 * notification - so the stale decision stands forever unless something asks for a recheck.
	 * This is that ask.
	 *
	 * Implementations must re-run their EXISTING predicate over currently perceived actors.
	 * They must not force a perception refresh, and they must be idempotent: this can fire
	 * repeatedly for an actor already handled, and must not produce a duplicate goal or
	 * restart work that is already running.
	 *
	 * The default implementation forwards to a generator's authored "RefreshPerceivedActors"
	 * pass when it defines one, so generators written before this hook existed participate
	 * without duplicating their predicate in native code. Override it to do something else.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Goal Generator")
	void ReevaluatePerceivedActors();
	virtual void ReevaluatePerceivedActors_Implementation();

	////Use this to tell the NPC activity component to add the goal. 
	UFUNCTION(BlueprintCallable, Category = "Activities") 
	UNPCGoalItem* AddGoalItem(class UNPCGoalItem* Goal, const bool bTriggerReselect=false);

	////Use this to tell the NPC activity component to remove a goal we've made 
	UFUNCTION(BlueprintCallable, Category = "Activities") 
	void RemoveGoalItem(class UNPCGoalItem* Goal);

	//Whether this generator should be saved to disk or not
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NPC Activity")
	bool bSaveGoalGenerator;

protected:

	//We cache the AI controller 
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Goal Generator")
	TObjectPtr<ANarrativeNPCController> OwnerController;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Goal Generator")
	TObjectPtr<UNPCActivityComponent> OwnerActivityComponent;

};


