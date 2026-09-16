// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SovProximityDetectionComponent.generated.h"

/** One hostile the player has actually seen, as the radar knows it. */
USTRUCT(BlueprintType)
struct FSovProximityContact
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Detection")
	TWeakObjectPtr<AActor> Actor;

	/** Where it was last actually seen. A memory contact keeps its last sighting, not a live position. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Detection")
	FVector LastSeenLocation = FVector::ZeroVector;

	/** Signed degrees from the player's facing, negative to the left. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Detection")
	float BearingDegrees = 0.f;

	/** 0 at the player, 1 at the edge of the sweep. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Detection")
	float NormalisedRange = 0.f;

	/** Full while in sight, fading while remembered. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Detection")
	float Alpha = 0.f;

	/** True only while this is a live sighting rather than a memory. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Detection")
	bool bLiveSighting = false;
};

/**
 * Hostiles the player has seen nearby, for the radar.
 *
 * Deliberately a detection component rather than a widget reading the world: what the player is
 * allowed to know is gameplay. It never reports an enemy that has not been seen, it forgets what it
 * has lost sight of, and it shows a remembered position rather than tracking something through a
 * wall. Those rules live in SovProximityDetectionPolicy, which portable tests cover.
 *
 * Read-only: it damages nothing, alerts nobody, and never tells an NPC where the player is.
 */
UCLASS(ClassGroup = (Sovereign), meta = (BlueprintSpawnableComponent))
class PROJECTVELKORRAN_API USovProximityDetectionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USovProximityDetectionComponent();

	/** Current contacts, strongest first. Rebuilt each observation; never held across frames by callers. */
	const TArray<FSovProximityContact>& GetContacts() const { return Contacts; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Detection")
	int32 GetContactCount() const { return Contacts.Num(); }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Detection")
	int32 GetLiveSightingCount() const;

protected:
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;

private:
	friend struct FSovProximityDetectionTestAccess;

	/** What the component remembers between observations, before policy turns it into a contact. */
	struct FTracked
	{
		TWeakObjectPtr<AActor> Actor;
		FVector LastSeenLocation = FVector::ZeroVector;
		float ContinuouslyVisibleSeconds = 0.f;
		float SecondsSinceSeen = 0.f;
		bool bAcquired = false;
	};

	void Observe(float DeltaSeconds);
	bool IsHostileTarget(AActor* Target) const;
	bool HasLineOfSight(const AActor* Target) const;
	float FacingYawDegrees() const;

	TArray<FTracked> Tracked;

	UPROPERTY(Transient) TArray<FSovProximityContact> Contacts;
};
