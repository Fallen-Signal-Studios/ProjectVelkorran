// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GAS/SovCombatTypes.h"
#include "SovCombatSustainDropComponent.generated.h"

class ASovAmmoCombatSustainPickup;
class ASovEchoCombatSustainPickup;
class UAmmoItem;
class UNarrativeAbilitySystemComponent;

/**
 * Spawns short-lived, automatic combat-sustain pickups from an NPC's fatal
 * player-caused damage transaction. Amounts and offsets are explicitly authored;
 * this component does not use Narrative loot tables or participate in saving.
 */
UCLASS(ClassGroup = (Sovereign), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovCombatSustainDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovCombatSustainDropComponent();

	UFUNCTION(BlueprintCallable, Category = "Sovereign|Combat Sustain|Drops")
	void InitializeWithAbilitySystem(
		UNarrativeAbilitySystemComponent* InAbilitySystemComponent);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat Sustain|Drops")
	bool IsInitialized() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat Sustain|Drops")
	bool HasSpawnedDropsForCurrentDeath() const
	{
		return bDropsSpawnedForCurrentDeath;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Master switch for this NPC archetype's transient combat-sustain drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops")
	bool bCombatSustainDropsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Ammo")
	bool bDropAmmo = true;

	/** Blueprint child supplies the mesh, overlap presentation, sound, and effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Ammo")
	TSubclassOf<ASovAmmoCombatSustainPickup> AmmoPickupClass;

	/** Narrative ammo item class replenished by the pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Ammo")
	TSubclassOf<UAmmoItem> AmmoItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Ammo", meta = (ClampMin = "1", UIMin = "1"))
	int32 AmmoAmount = 5;

	/** Local-space offset from the killed NPC at the moment of the fatal hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Ammo", meta = (MakeEditWidget = true))
	FVector AmmoSpawnOffset = FVector(0.f, 30.f, 35.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Echo")
	bool bDropEcho = true;

	/** Blueprint child supplies the mote mesh, overlap presentation, sound, and effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Echo")
	TSubclassOf<ASovEchoCombatSustainPickup> EchoPickupClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Echo", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EchoAmount = 1.f;

	/** Local-space offset from the killed NPC at the moment of the fatal hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Sustain|Drops|Echo", meta = (MakeEditWidget = true))
	FVector EchoSpawnOffset = FVector(0.f, -30.f, 35.f);

private:
	UFUNCTION()
	void HandleAbilitySystemInitialized();

	UFUNCTION()
	void HandleDamageResolved(const FSovDamageResult& Result);

	UFUNCTION()
	void HandleDeathStateChanged(
		AActor* KilledActor,
		UNarrativeAbilitySystemComponent* KilledActorASC,
		const bool bIsDead);

	void TryInitializeFromOwner();
	bool IsEligiblePlayerSource(const AActor* SourceActor) const;
	bool IsEligibleFatalDamage(const FSovDamageResult& Result) const;
	void SpawnConfiguredDrops();
	FTransform MakePickupSpawnTransform(const FVector& LocalOffset) const;

	UPROPERTY(Transient)
	TObjectPtr<UNarrativeAbilitySystemComponent> AbilitySystemComponent = nullptr;

	bool bDropsSpawnedForCurrentDeath = false;
};
