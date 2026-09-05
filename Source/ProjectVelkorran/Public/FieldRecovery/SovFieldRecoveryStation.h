// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Progression/SovTechniqueSafePoint.h"
#include "Interaction/InteractableComponent.h"
#include "SovFieldRecoveryStation.generated.h"

UCLASS()
class PROJECTVELKORRAN_API USovFieldRecoveryStationInteraction : public UNarrativeInteractableComponent
{
	GENERATED_BODY()
public:
	USovFieldRecoveryStationInteraction();
protected:
	virtual bool CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error) override;
	virtual bool Interact(APawn* Player, UNarrativeInteractionComponent* Interaction) override;
private:
	friend struct FSovFieldRecoveryTestAccess;
	bool ValidatePlayer(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error) const;
	bool bRefilling = false;
};

/** Uses the established physical safe-point admission and Narrative's real completed interaction route. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovFieldRecoveryStation : public ASovTechniqueSafePoint
{
	GENERATED_BODY()
public:
	ASovFieldRecoveryStation();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Field Recovery") TObjectPtr<USovFieldRecoveryStationInteraction> Interaction;
};
