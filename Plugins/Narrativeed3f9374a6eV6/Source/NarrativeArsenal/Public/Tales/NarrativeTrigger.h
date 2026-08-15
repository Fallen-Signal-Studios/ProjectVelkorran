// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "NarrativeTrigger.generated.h"

/**
 * Triggers can be activated,or deactivated at any time for any reason you like. They can fire events when began or ended,
 * or do any number of other things by overidding activate/deactivate.
 
 * A great example usage of these is making an NPC do a certain activity like sleep in a bed during a certain time of day. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Default"))
class NARRATIVEARSENAL_API UNarrativeTrigger : public UObject
{
	GENERATED_BODY()
	
public:

	UNarrativeTrigger();

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

	/** Events that this trigger should activate/deactivate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Narrative Trigger")
	TArray<class UNarrativeEvent*> TriggerEvents;

	UPROPERTY(BlueprintReadOnly, Category = "Narrative Trigger")
	TObjectPtr<class ANarrativeCharacter> OwnerCharacter; 

	//Bind delegates in here to activate or deactivate your trigger, set a timer, etc. 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative Trigger")
	void Initialize();
	virtual void Initialize_Implementation();

	//Return whether the trigger is active or not
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative Trigger")
	bool IsActive();
	virtual bool IsActive_Implementation();

	bool GetActive() const; 

	/** Return a string saying what this trigger is */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Event")
	FString GetDescription() const;
	virtual FString GetDescription_Implementation() const;

protected:

	/** Whether the trigger is active right now */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative Trigger")
	bool bIsActive;

	UFUNCTION(BlueprintCallable, Category = "Narrative Trigger")
	virtual void Activate();

	UFUNCTION(BlueprintCallable, Category = "Narrative Trigger")
	virtual void Deactivate();

	UFUNCTION(BlueprintImplementableEvent, Category = "Narrative Trigger")
	void OnActivate();

	UFUNCTION(BlueprintImplementableEvent, Category = "Narrative Trigger")
	void OnDeactivate();

};
