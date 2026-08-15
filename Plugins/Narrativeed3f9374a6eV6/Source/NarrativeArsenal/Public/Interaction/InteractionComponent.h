// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InteractableComponent.h"
#include "NarrativeSavableComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "Abilities/GameplayAbilityTypes.h"

#include "InteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFoundInteractable,class  UNarrativeInteractableComponent*, Interactable);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLostInteractable, class  UNarrativeInteractableComponent*, Interactable);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteracted, class  UNarrativeInteractionComponent*, Interaction, class  UNarrativeInteractableComponent*, Interactable);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractPressed, class  UNarrativeInteractionComponent*, Interaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractReleased, class  UNarrativeInteractionComponent*, Interaction);

//Called when we interact with an interactable object - this works for both Slot based interaction and quick interaction. 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUseInteractable, AActor*, InteractableActor, UNarrativeInteractableComponent*, InteractableComponent);

/** Added to both Player and NPC controllers. Allows both NPCs and Players to interact with the world. 
See UPlayer/NPCInteractionComponents for speciaized versions.  */
UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Interaction", meta=(BlueprintSpawnableComponent) )
class NARRATIVEARSENAL_API UNarrativeInteractionComponent : public UActorComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()


protected:

	UNarrativeInteractionComponent();

	virtual void BeginPlay() override;
	virtual void Load_Implementation() override;
	virtual void SetOccupiedInteractable(class UNarrativeInteractableComponent* Interactable, const int32 SlotIdx);

protected:

	friend class UNarrativeInteractAbility; 

	//Our owning character
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	class ANarrativeCharacter* OwningPawn;

	//Our controller owner
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	class AController* OwningController;

	//Claims an interaction slot on the given Interactable
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	bool ClaimInteractionSlot(class UNarrativeInteractableComponent* Interactable, const int32 SlotIdx);

	//Releases our claimed interactable slot if we have one 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	void ReleaseInteractionSlot();

	//Our interaction slot claim handle, if we've claimed an interactables slot. 
	//Originally was going to be an actual handle, however this is more just a reference to our interactable, needs a rename. 
	UPROPERTY(BlueprintReadWrite, Category = "Narrative|NarrativeCharacter|Interaction")
	FInteractionSlotClaimHandle InteractionSlotClaimHandle;

	//The interactable we've claimed a slot on and are currently interacting with
	UPROPERTY(BlueprintReadWrite, Category = "Narrative|NarrativeCharacter|Interaction")
	UNarrativeInteractableComponent* OccupiedInteractable;

	//Soft ref of the owner so we can restore when the game loads back in. For NPCs this is done by the interact goal. 
	//Currently players dont restore this, and saving is preventing whilst player is interacting with something. 
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Narrative|NarrativeCharacter|Interaction")
	TSoftObjectPtr<class AActor> OccupiedInteractableSoftOwner;

	//The index of the slot we're occupying 
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Narrative|NarrativeCharacter|Interaction")
	int32 OccupiedInteractableSlotIdx;

	//Handle to current interaction ability 
	UPROPERTY(BlueprintReadWrite, Category = "Narrative|NarrativeCharacter|Interaction")
	FGameplayAbilitySpecHandle CurrentInteractAbilityHandle;

	//Called when we interact with an interactable object - this works for both Slot based interaction and quick interaction. 
	UPROPERTY(BlueprintAssignable, Category = "Narrative|NarrativeCharacter|Interaction")
	FOnUseInteractable OnBeginUseInteractable;

	//Called when the interaction slot we've taken is released as our interaction behavior has finished
	UPROPERTY(BlueprintAssignable, Category = "Narrative|NarrativeCharacter|Interaction")
	FOnUseInteractable OnFinishUseInteractable;

	UFUNCTION(Server, Reliable)
	virtual void ServerStopInteractBehavior(const bool bWasStolen, UNarrativeInteractionComponent* OptionalStealer = nullptr, FGameplayEventData OptionalPayload = FGameplayEventData());

public:

	//Return true if we're occuping an interactable currently, such as a seat. 
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Interaction")
	FORCEINLINE bool HasOccupiedInteractable() const { return OccupiedInteractable != nullptr;};

	/*Begins the interaction ability for our currently claimed slot, return true if worked.
	* 
	@param bIsStealing Whether or not we're stealing the slot from someone  
	@param StealingFrom if we're stealing the slot from someone, this will contain that person, so we can do any logic on them we need. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	bool RunInteractBehavior(const bool bIsStealing, UNarrativeInteractionComponent* StealingFrom = nullptr);

	/*Ends the interaction behavior for our current ability.Can return false if we can't exit the interaction, 
	for example if we tried exiting a car but it was moving too fast to allow an exit. 
	
	@param bWasStolen Whether or not we're ending because someone is stealing the slot 
	@param OptionalStealer points to the person who has stolen the slot and stopped our interact behavior
	@param OptionalPayload additional payload data the stop interact data can use to mo
	*/
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	bool StopInteractBehavior(const bool bWasStolen, UNarrativeInteractionComponent* OptionalStealer = nullptr, FGameplayEventData OptionalPayload = FGameplayEventData());

	/*
	* Return true if the ability is happy for us to exit our interaction 
	*/
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Interaction")
	UNarrativeInteractAbility* GetInteractAbility() const ;

};
