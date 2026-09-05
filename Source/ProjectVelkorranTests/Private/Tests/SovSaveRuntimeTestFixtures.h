// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "NarrativeSave.h"
#include "NarrativeSavableActor.h"
#include "NarrativeSavableComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Save/SovCampaignSaveGame.h"
#include "Save/SovSaveSubsystem.h"
#include "Framework/SovPlayerController.h"
#include "SovSaveRuntimeTestFixtures.generated.h"
UCLASS()
class USovSaveLoadCompletionProbe : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<class USovSaveSubsystem> Subsystem;
    int32 Notifications = 0;
    ESovSaveResult LastResult = ESovSaveResult::MissingSave;
    bool bObservedReleasedOwnership = false;
    FString LastMessage;
    UFUNCTION() void OnCompleted(ESovSaveResult Result, const FSovSaveSlotHeader& Header, const FString& Message);
};
UCLASS()
class USovSaveRuntimeSubclass : public UNarrativeSave
{
    GENERATED_BODY()
public:
    UPROPERTY() FString CreatorMarker;
};
UCLASS()
class USovSaveReceiptTestSubsystem : public USovSaveSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient) TObjectPtr<ASovPlayerController> TestController;
protected:
    virtual ASovPlayerController* Controller() const override { return TestController; }
};
UCLASS()
class ASovSaveReceiptTestController : public ASovPlayerController
{
    GENERATED_BODY()
protected:
    virtual void OnPossess(APawn* Pawn) override { APlayerController::OnPossess(Pawn); }
};

UCLASS()
class USovSavePhaseProbeComponent : public UActorComponent, public INarrativeSavableComponent
{
    GENERATED_BODY()
public:
    static TArray<FName> RestoreOrder;
    ENarrativeRestorePhase Phase = ENarrativeRestorePhase::Interactables;
    UPROPERTY(SaveGame) int32 SavedValue = 17;
    virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return Phase; }
    virtual void Load_Implementation() override { RestoreOrder.Add(GetFName()); }
};
UCLASS()
class ASovSaveRuntimeActor : public AActor, public INarrativeSavableActor
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) int32 SavedValue = 5;
    bool bRejectSerialization = false;
    FGuid Guid = FGuid::NewGuid();
    virtual FGuid GetActorGUID_Implementation() const override { return Guid; }
    virtual void SetActorGUID_Implementation(const FGuid& Value) override { Guid = Value; }
    virtual bool ShouldRespawn_Implementation() const override { return false; }
    virtual void Serialize(FArchive& Ar) override
    { Super::Serialize(Ar); if (Ar.IsSaving() && Ar.ArIsSaveGame && bRejectSerialization) { Ar.SetError(); } }
};
