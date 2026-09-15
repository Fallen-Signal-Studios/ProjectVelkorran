// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Cinematics/SovAurelionStorySequenceActor.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GameFramework/Controller.h"
#include "SovAurelionRequestRuntimeTestFixtures.generated.h"

/** A competing prompt whose own admission rule is controlled by the test, as a hostile's refusal is. */
UCLASS(Transient, NotBlueprintable)
class USovRefusingTestInteractable : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    bool bAdmit = false;
    virtual bool CanInteract_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText) override
    {
        if (!bAdmit) { OutErrorText = FText::FromString(TEXT("This prompt refuses interaction")); }
        return bAdmit;
    }
};

/** Only the owning controller/pawn are supplied; the native focus selection and reach remain active. */
UCLASS(Transient, NotBlueprintable)
class USovAurelionFocusTestInteraction : public UPlayerInteractionComponent
{
    GENERATED_BODY()
public:
    void Configure(AController* Controller)
    { OwningController = Controller; OwningPawn = Cast<ANarrativeCharacter>(Controller->GetPawn()); }
    const UNarrativeInteractableComponent* Viewed() const { return ViewedInteractable; }
};
UCLASS()
class ASovAurelionStoryTestActor : public ASovAurelionStorySequenceActor
{
    GENERATED_BODY()
public:
    ASovAurelionStoryTestActor(const FObjectInitializer& Initializer) : Super(Initializer) {}
    void InitializeTestSequence(ULevelSequence* Sequence) { SetSequence(Sequence); InitializePlayer(); }
};
