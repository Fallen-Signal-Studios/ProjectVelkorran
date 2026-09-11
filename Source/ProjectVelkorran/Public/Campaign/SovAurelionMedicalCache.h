// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "Interaction/InteractableComponent.h"
#include "NarrativeSavableActor.h"
#include "SovAurelionMedicalCache.generated.h"
class ASovAurelionPrioritySupport;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class PROJECTVELKORRAN_API USovAurelionMedicalAid : public UGameplayEffect
{
    GENERATED_BODY()
public:
    USovAurelionMedicalAid();
};

UCLASS()
class PROJECTVELKORRAN_API USovAurelionMedicalInteraction : public UNarrativeInteractableComponent
{
    GENERATED_BODY()
public:
    virtual bool CanInteract_Implementation(APawn* Pawn, UNarrativeInteractionComponent* Interaction, FText& Error) override;
protected:
    virtual bool Interact(APawn* Pawn, UNarrativeInteractionComponent* Interaction) override;
};

/** One physical medical aid; the native save record owns consumption, the campaign owns access. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionMedicalCache : public AActor, public INarrativeSavableActor
{
    GENERATED_BODY()
public:
    ASovAurelionMedicalCache();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USovAurelionMedicalInteraction> Interactable;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly) FName CacheId;
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly) TObjectPtr<ASovAurelionPrioritySupport> Support;
    /** Prototype quantity: one aid restores 35% max health; it never refills scarce charges. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01",ClampMax="1")) float HealthFraction = .35f;
    UFUNCTION(BlueprintPure) bool IsConsumed() const { return bConsumed; }
    bool CanUse(const APawn* Pawn, FText& Error) const;
    bool TryUse(APawn* Pawn, FText& Error);
    virtual FGuid GetActorGUID_Implementation() const override;
    virtual void SetActorGUID_Implementation(const FGuid& Guid) override { SaveGuid = Guid; }
    virtual bool ShouldRespawn_Implementation() const override { return false; }
    virtual void Load_Implementation() override;
    virtual void Serialize(FArchive& Ar) override;
protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    friend struct FSovAurelionSupplyTestAccess;
    void RefreshPresentation();
    UPROPERTY(SaveGame) bool bConsumed = false;
    UPROPERTY(SaveGame) FGuid SaveGuid;
    bool bMutating = false;
};

/** Mirrors existing support barrier collision with visible geometry. Never changes access or rewards. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovAurelionSupportPresentation : public AActor
{
    GENERATED_BODY()
public:
    ASovAurelionSupportPresentation();
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly) TObjectPtr<ASovAurelionPrioritySupport> Support;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> WestBarrierVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> EastBarrierVisual;
protected:
    virtual void Tick(float DeltaSeconds) override;
};
