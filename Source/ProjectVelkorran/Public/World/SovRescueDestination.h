// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableComponent.h"
#include "SovRescueDestination.generated.h"
class UBoxComponent;
class ASovPlayerCharacterBase;
class USovCarryTargetComponent;

UCLASS()
class PROJECTVELKORRAN_API USovRescueDestinationInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error) override;
protected:
    virtual bool Interact(APawn* Player, UNarrativeInteractionComponent* Interaction) override;
};

/** Spatial proof owner for an authored rescue beat using the existing campaign journal. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovRescueDestination : public AActor
{
    GENERATED_BODY()
public:
    ASovRescueDestination();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> EntryBounds;
    /** Ground mark; native code adds the carried body's half-height. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> ReleaseMark;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovRescueDestinationInteractable> Interactable;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Rescue") FName DestinationId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rescue") FName MissionId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rescue") FName RequiredCarryTargetId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rescue") FName CompletionBeat;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Rescue") bool RequestRescue(APawn* Player, FText& Error);
    bool ValidateRescue(const APawn* Player, USovCarryTargetComponent*& Target, FVector& ReleaseCenter, FText& Error) const;
private:
    bool bMutating = false;
};
