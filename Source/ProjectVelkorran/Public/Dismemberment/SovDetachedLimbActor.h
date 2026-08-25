// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovDetachedLimbActor.generated.h"

class USkeletalMeshComponent;

/** Cosmetic, locally simulated base actor for authored severed-limb Blueprints. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovDetachedLimbActor : public AActor
{
	GENERATED_BODY()

public:
	ASovDetachedLimbActor();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Dismemberment")
	void InitializeDetachedLimb(FVector InInitialImpulse, AActor* InSourceCharacter);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dismemberment")
	USkeletalMeshComponent* GetLimbMesh() const { return LimbMesh; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dismemberment")
	TObjectPtr<USkeletalMeshComponent> LimbMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dismemberment", meta = (ClampMin = "0.0"))
	float CosmeticLifeSeconds = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dismemberment")
	bool bSimulatePhysics = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dismemberment")
	FName CollisionProfileName = TEXT("Ragdoll");

	UPROPERTY(BlueprintReadOnly, Category = "Dismemberment")
	FVector InitialImpulse = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Dismemberment")
	TObjectPtr<AActor> SourceCharacter = nullptr;

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dismemberment", meta = (DisplayName = "Detached Limb Initialized"))
	void ReceiveDetachedLimbInitialized(FVector AppliedImpulse, AActor* SeveredFromCharacter);
};
