// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GameFramework/Controller.h"
#include "SovCampaignTerminalRuntimeTestFixtures.generated.h"

/** Only camera focus discovery is supplied by the fixture; native reach/hold/dispatch remain active. */
UCLASS(Transient, NotBlueprintable)
class USovCampaignTerminalTestInteraction : public UPlayerInteractionComponent
{
    GENERATED_BODY()
public:
    void Configure(AController* Controller)
    { OwningController = Controller; OwningPawn = Cast<ANarrativeCharacter>(Controller->GetPawn()); }
    virtual void PerformInteractionCheck(float DeltaTime) override {}
    FOnUseInteractable& FinishUseEvent() { return OnFinishUseInteractable; }
};

UCLASS(Transient, NotBlueprintable)
class USovCampaignTerminalTestObserver : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void()> Callback;
    int32 InteractedCount = 0;
    UFUNCTION() void OnInteracted(APawn* Pawn, UNarrativeInteractionComponent* Interaction) { ++InteractedCount; }
    UFUNCTION() void OnUse(AActor* Actor, UNarrativeInteractableComponent* Component) { if (Callback) { Callback(); } }
};

/** Records what a refused press would have told the player, without needing a widget to render it. */
UCLASS(Transient, NotBlueprintable)
class USovRefusalProbe : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION() void OnRefused(UNarrativeInteractableComponent* Interactable, const FText& Reason)
    { ++Count; Last = Reason; }
    int32 Count = 0;
    FText Last;
};
