// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractionComponent.h"
#include "PlayerInteractionComponent.generated.h"

/**
 * Interaction component that exists on the player controller, and contains all the interaction tracing stuff NPCs dont need
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractRefused, UNarrativeInteractableComponent*, Interactable, const FText&, Reason);

UCLASS( ClassGroup=(Narrative), DisplayName = "Narrative Player Interaction", meta=(BlueprintSpawnableComponent) )
class NARRATIVEARSENAL_API UPlayerInteractionComponent : public UNarrativeInteractionComponent
{
	GENERATED_BODY()
	
public: 

	UPlayerInteractionComponent();

	//[local + server] Called when we find a new interactable object 
	UPROPERTY(EditDefaultsOnly, BlueprintAssignable, Category = "Interaction")
	FOnFoundInteractable OnFoundInteractable;

	//[local + server] Called when we've lost our interactable
	UPROPERTY(EditDefaultsOnly, BlueprintAssignable, Category = "Interaction")
	FOnLostInteractable OnLostInteractable;

	//[local + server] Called when we interact with an interactable - replaced with OnBeginUseInteractable in base class 
	//UPROPERTY(EditDefaultsOnly, BlueprintAssignable, Category = "Interaction")
	//FOnInteracted OnInteracted;

	//[local + server] Called when we start holding the interact key 
	UPROPERTY(EditDefaultsOnly, BlueprintAssignable, Category = "Interaction")
	FOnInteractPressed OnInteractPressed;

	//[local + server] Called when we release the interact key 
	UPROPERTY(EditDefaultsOnly, BlueprintAssignable, Category = "Interaction")
	FOnInteractReleased OnInteractReleased;

	/**
	 * [local + server] The player pressed interact and the interactable said no, with its reason.
	 *
	 * A refused press produced nothing at all before this: no sound, no message, no state change.
	 * That is indistinguishable from the input being dropped, and it is how a working refusal reads
	 * as a broken game. The reasons already existed; nothing carried them anywhere.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintAssignable, Category = "Interaction")
	FOnInteractRefused OnInteractRefused;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void Load_Implementation() override;
	virtual void Deactivate() override;

	virtual void PerformInteractionCheck(float DeltaTime);
	/** Actual pawn range and unoccluded view, rechecked at interaction completion. */
	bool IsInteractableInReach(UNarrativeInteractableComponent* Interactable) const;
	
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ClearViewedInteractable();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetViewedInteractable(UNarrativeInteractableComponent* Interactable);

	UFUNCTION(Server, Reliable)
	void ServerBeginInteract();

	UFUNCTION(Server, Reliable)
	void ServerEndInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual void BeginInteract();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	virtual void EndInteract();

protected:
	void CompletePendingInteraction();

	//The current interactable component we're viewing, if there is one
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	class UNarrativeInteractableComponent* ViewedInteractable;

	//The time when we last checked for an interactable
	UPROPERTY()
	float LastInteractionCheckTime;

	//Whether the local player is holding the interact key
	UPROPERTY()
	bool bInteractHeld;

	/**All of the input actions that should instigate an interaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	TArray<TObjectPtr<class UInputAction>> InteractionInputs;

	/**The amount of time before interaction completes on our current interactable.This is stored per interactor and not per interactable
	because in a networked game we may want to support multiple players interacting with something and each will have their own time*/
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float RemainingInteractTime;

	//How often in seconds to check for an interactable object. Set this to zero if you want to check every tick.
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionCheckFrequency;

	//How far we'll trace when we check if the player is looking at an interactable object
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionCheckDistance;

	//If greater than zero we'll use a sphere trace over an interaction trace 
	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionCheckSphereRadius;
};
