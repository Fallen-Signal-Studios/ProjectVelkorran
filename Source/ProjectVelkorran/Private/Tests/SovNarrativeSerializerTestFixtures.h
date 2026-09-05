// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include "NarrativeStableActor.h"
#include "SovNarrativeSerializerTestFixtures.generated.h"

UCLASS()
class ASovSerializerMovableActor : public AActor, public INarrativeSavableActor
{
    GENERATED_BODY()
public:
    ASovSerializerMovableActor()
    {
        auto* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("Scene"));
        Scene->SetMobility(EComponentMobility::Movable);
        SetRootComponent(Scene);
    }
    UPROPERTY(SaveGame) int32 SavedValue = 17;
    bool bRejectLoading = false;
    FGuid Guid = FGuid::NewGuid();
    virtual FGuid GetActorGUID_Implementation() const override { return Guid; }
    virtual void SetActorGUID_Implementation(const FGuid& Value) override { Guid = Value; }
    virtual bool ShouldRespawn_Implementation() const override { return false; }
    virtual void Serialize(FArchive& Ar) override
    {
        Super::Serialize(Ar);
        if (Ar.IsLoading() && Ar.ArIsSaveGame && bRejectLoading) { Ar.SetError(); }
    }
};

/** Referenced by GUID, without opting into the Savable world-record contract. */
UCLASS()
class ASovSerializerStableActor : public AActor, public INarrativeStableActor
{
    GENERATED_BODY()
public:
    FGuid Guid = FGuid::NewGuid();
    virtual FGuid GetActorGUID_Implementation() const override { return Guid; }
};
