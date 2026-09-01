// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SovCombatSustainPickup.generated.h"

class ASovPlayerCharacterBase;
class FLifetimeProperty;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Transient, server-authoritative combat resource collected by walking over it.
 *
 * The actor intentionally implements no save interface. Encounter/checkpoint
 * logic owns whether another pickup opportunity should be spawned after reset.
 * Blueprint children supply the mesh, idle Niagara/audio, and collection art.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Combat Sustain Pickup"))
class PROJECTVELKORRAN_API ASovCombatSustainPickup : public AActor
{
	GENERATED_BODY()

public:
	ASovCombatSustainPickup();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Optional per-spawn lifetime override. Call before FinishSpawning. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Combat Sustain")
	void InitializePickupLifetime(float InLifetimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat Sustain")
	bool IsClaimed() const { return bClaimed; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Derived pickups perform their authoritative resource transaction here. */
	virtual bool TryGrantTo(ASovPlayerCharacterBase* CollectingPlayer);

	UFUNCTION()
	void HandlePickupOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Claimed();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayCollectionPresentation();

	/** Runs on every relevant machine after an authoritative collection. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Combat Sustain", meta = (DisplayName = "On Pickup Collected"))
	void BP_OnPickupCollected();

	void DisableIdlePresentation();

	/** Small physics body that lets the pickup fall and settle without making the collection trigger block pawns. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Components")
	TObjectPtr<USphereComponent> GroundCollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Components")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Components")
	TObjectPtr<USceneComponent> VisualRoot;

	/** Assign the pickup mesh on a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	/** Optional looping/idle effect configured on a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Components")
	TObjectPtr<UNiagaraComponent> IdleNiagaraComponent;

	/** Optional looping/idle sound configured on a Blueprint child. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Components")
	TObjectPtr<UAudioComponent> IdleAudioComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Collision", meta = (ClampMin = "1.0", Units = "cm"))
	float PickupRadius = 70.0f;

	/** Radius of the physics body that rests on the floor. Match this to the visible pickup's footprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Collision", meta = (ClampMin = "1.0", Units = "cm"))
	float GroundCollisionRadius = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Physics", meta = (ClampMin = "0.0"))
	float GroundLinearDamping = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Physics", meta = (ClampMin = "0.0"))
	float GroundAngularDamping = 5.0f;

	/** Server destroys an unclaimed pickup after this many seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Lifetime", meta = (ClampMin = "0.1", Units = "s"))
	float PickupLifetimeSeconds = 20.0f;

	/** Brief replication window after collection presentation is sent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Lifetime", meta = (ClampMin = "0.05", Units = "s"))
	float CollectionCleanupDelay = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Presentation|Idle", meta = (Units = "deg/s"))
	float RotationRateDegrees = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Presentation|Collected")
	TObjectPtr<UNiagaraSystem> CollectionNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Presentation|Collected")
	FVector CollectionNiagaraScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Presentation|Collected")
	TObjectPtr<USoundBase> CollectionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Presentation|Collected", meta = (ClampMin = "0.0"))
	float CollectionSoundVolume = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Presentation|Collected", meta = (ClampMin = "0.01"))
	float CollectionSoundPitch = 1.0f;

	/** Replication-backed one-shot claim prevents two overlapping pawns collecting it. */
	UPROPERTY(ReplicatedUsing = OnRep_Claimed, VisibleInstanceOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain")
	bool bClaimed = false;
};
