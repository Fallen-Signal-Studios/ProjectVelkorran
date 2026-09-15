// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NarrativeSavableActor.h"
#include "Sovereign/SovEnvironmentDamage.h"
#include "SovDestructibleCover.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UNiagaraSystem;
class USoundBase;

/** One authored obstruction group. Structural supports must never opt in. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovDestructibleCover : public AActor, public INarrativeSavableActor, public ISovEnvironmentDamageable
{
    GENERATED_BODY()
public:
    ASovDestructibleCover();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Obstruction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> IntactVisual;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Destruction") TObjectPtr<UGeometryCollection> FracturedAsset;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Destruction") bool bDestructionEnabled = false;
    /** Assign from the placement manifest; never generate identities during play. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, SaveGame, Category="Destruction") FGuid PlacementGuid;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category="Destruction", meta=(ClampMin="1")) float RemainingHealth = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Destruction", meta=(ClampMin="0.1", ClampMax="10")) float DebrisLifetime = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Destruction", meta=(ClampMin="0")) float BreakStrain = 100000000.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Destruction") TObjectPtr<UNiagaraSystem> BreakEffect;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Destruction") TObjectPtr<USoundBase> BreakSound;
    UFUNCTION(BlueprintPure, Category="Destruction") bool IsBroken() const { return bBroken; }
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer) override;
    virtual FGuid GetActorGUID_Implementation() const override { return PlacementGuid; }
    virtual void SetActorGUID_Implementation(const FGuid& Guid) override { PlacementGuid = Guid; }
    virtual bool ShouldRespawn_Implementation() const override { return false; }
    virtual ENarrativeRestorePhase GetSaveRestorePhase() const override { return ENarrativeRestorePhase::Structure; }
    virtual void Load_Implementation() override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UPROPERTY(SaveGame, ReplicatedUsing=OnRep_Broken) bool bBroken = false;
    UPROPERTY(Transient) TObjectPtr<UGeometryCollectionComponent> Debris;
    FTimerHandle FractureTimer;
    FTimerHandle CleanupTimer;
    FVector ImpactDirection = FVector::ForwardVector;
    UFUNCTION() void OnRep_Broken();
    void ReconcileObstruction();
    void SpawnDebris();
    void FractureDebris();
    void ImpulseDebris();
    void ClearDebris();
};
