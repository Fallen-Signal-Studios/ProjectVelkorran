// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Items/NarrativeItem.h"
#include "PoisonableFragment.generated.h"

/**
 * Add this to any item that needs to be poisonable - usually weapons, could be ammo like arrows, etc. 
 */
UCLASS()
class NARRATIVEARSENAL_API UPoisonableFragment : public UNarrativeItemFragment
{
	GENERATED_BODY()
	
public:

	UPoisonableFragment(const FObjectInitializer& ObjectInitializer);

	//Override if your game needs certain items to accept/deny certain poisons. 
	virtual bool CanBePoisonedBy(const class UPoisonItem* Poison) const;

	//Set the item as being poisoned 
	UFUNCTION(BlueprintCallable, Category = "Poison")
	virtual void SetPoison(const TSubclassOf<UGameplayEffect>& Poison);

	//Get the poison effect, and also empty out the applied poison, consuming it. 
	TSubclassOf<UGameplayEffect> ConsumePoison();

	//The poison that has been applied to this. 
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Poison")
	TSubclassOf<UGameplayEffect> AppliedPoison;

};
