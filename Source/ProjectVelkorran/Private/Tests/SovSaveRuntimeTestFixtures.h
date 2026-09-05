// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "NarrativeSave.h"
#include "NarrativeSavableActor.h"
#include "NarrativeSavableComponent.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SovSaveRuntimeTestFixtures.generated.h"
UCLASS()
class USovSaveRuntimeSubclass : public UNarrativeSave
{
    GENERATED_BODY()
public:
    UPROPERTY() FString CreatorMarker;
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
