// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Pickups/SovCombatSustainPickup.h"
#include "SovEchoCombatSustainPickup.generated.h"

/** Automatic pickup that grants a small amount of the player's Echo resource. */
UCLASS(Blueprintable, meta = (DisplayName = "Echo Combat Sustain Pickup"))
class PROJECTVELKORRAN_API ASovEchoCombatSustainPickup : public ASovCombatSustainPickup
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies the authoritative payload before FinishSpawning is called. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Combat Sustain|Echo")
	void InitializeEcho(float InEchoAmount);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat Sustain|Echo")
	float GetEchoAmount() const { return EchoAmount; }

protected:
	virtual bool TryGrantTo(ASovPlayerCharacterBase* CollectingPlayer) override;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat Sustain|Echo", meta = (ClampMin = "0.01"))
	float EchoAmount = 1.0f;
};
