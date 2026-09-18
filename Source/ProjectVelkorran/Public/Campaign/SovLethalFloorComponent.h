// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovDamageTargetPolicy.h"
#include "SovLethalFloorComponent.generated.h"

/**
 * Holds its owner above a health floor while a required mechanic is still outstanding.
 *
 * The Aurelion Elite could be killed conventionally before the phase mechanic that is supposed to
 * resolve it, and the encounter then failed and reloaded - the player lost a run for doing too much
 * damage (audit EA2-06). Nothing protected the Elite; the phase directors only noticed afterwards.
 *
 * This is a floor, not invulnerability. The fight still reads normally, hits still land, poise still
 * breaks; the target simply cannot be finished until whoever owns the phase releases the floor.
 */
UCLASS(ClassGroup = (Sovereign), meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovLethalFloorComponent : public UActorComponent, public ISovDamageTargetPolicy
{
	GENERATED_BODY()

public:
	USovLethalFloorComponent();

	/** Health the owner cannot be taken below while the floor is held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Sovereign|Encounter", meta = (ClampMin = "1.0"))
	float MinimumHealth = 1.0f;

	/** Replicated so a client's presentation can read "cannot be finished yet" without guessing. */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FloorHeld, Category = "Sovereign|Encounter")
	bool bFloorHeld = false;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Encounter")
	bool IsFloorHeld() const { return bFloorHeld; }

	/** Authority-only. The phase that requires a mechanic holds the floor until its receipt lands. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Encounter")
	void SetFloorHeld(bool bHeld);

	/** Broadcast on every change, including the replicated one, for a readable cue. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSovLethalFloorChanged, bool, bHeld);
	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Encounter")
	FSovLethalFloorChanged OnLethalFloorChanged;

	virtual bool LimitSovIncomingDamage(AActor* Instigator, const FGameplayEffectContextHandle& Context,
		float CurrentHealth, float& InOutShieldDamage, float& InOutHealthDamage, float& InOutPoiseDamage) const override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION() void OnRep_FloorHeld();
};
