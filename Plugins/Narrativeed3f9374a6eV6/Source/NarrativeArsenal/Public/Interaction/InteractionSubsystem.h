// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InteractionSubsystem.generated.h"

/**
 * Right now just serves as a place we can cache interactables so the gameplay debugger category can easily access
 * Might want to add more advanced features in future such as the spatial searching we see in SmartObjects 
 */
UCLASS()
class NARRATIVEARSENAL_API UInteractionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:

	UInteractionSubsystem();

	void CacheInteractable(class UNarrativeInteractableComponent* Interactable);
	void UncacheInteractable(class UNarrativeInteractableComponent* Interactable);

	FORCEINLINE TSet<TObjectPtr<class UNarrativeInteractableComponent>> GetInteractableActors() const {return InteractableActors; };

protected:

	UPROPERTY()
	TSet<TObjectPtr<class UNarrativeInteractableComponent>> InteractableActors;

};
