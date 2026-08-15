// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractionComponent.h"
#include "NPCInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTargetedInteractionSlotChanged, UNarrativeInteractableComponent*, InteractableComponent, int32, NewSlotIdx);

/**
 * Interaction component used by NPCCharacters. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNPCInteractionComponent : public UNarrativeInteractionComponent
{
	GENERATED_BODY()
	
public: 

	UNPCInteractionComponent();

	//Start targeting an interaction slot 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	bool TargetBestInteractionSlot(class UNarrativeInteractableComponent* Interactable, const bool bFindNewSlotIfSlotTaken);

	//Start targeting an interaction slot 
	//@param  bAutoUpdateIfSlotStolen Will automatically target a new slot if our one is lost. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	bool TargetInteractionSlot(class UNarrativeInteractableComponent* Interactable, int32 Index, const bool bAutoUpdateIfSlotStolen);

	//Called when our targeted slot gets invalidated 
	UFUNCTION()
	void OnTargetSlotTaken(int32 Slot, class UNarrativeInteractionComponent* StealerComp, class UNarrativeInteractableComponent* InteractableComp);

	//We broadcast this whenever our targeted slot changes so interact activity can change its BB values. 
	UPROPERTY(BlueprintAssignable, Category = "Narrative|NarrativeCharacter|Interaction")
	FOnTargetedInteractionSlotChanged OnTargetedInteractionSlotChanged;

	//Whether to automatically target a new slot if our one is lost. 
	bool bFindNewSlotIfSlotTaken;

};
