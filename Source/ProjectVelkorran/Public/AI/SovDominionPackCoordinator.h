// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "SovDominionPackCoordinator.generated.h"

class ANarrativeNPCCharacter;
class ANPCSpawner;
class ASovDominionHandler;
class UNarrativeAbilitySystemComponent;

/**
 * Authority-only runtime wiring for one separately spawned Dominion pack.
 *
 * The coordinator waits for one Handler spawner and each explicitly assigned
 * Hound spawner to expose exactly one living NPC. It then gives the Handler's
 * native command-link component its stable encounter identity, registers only
 * those resolved Hounds, and activates a fresh link with the Handler as source.
 */
UCLASS(Blueprintable, Placeable)
class PROJECTVELKORRAN_API ASovDominionPackCoordinator : public AActor
{
	GENERATED_BODY()

public:
	ASovDominionPackCoordinator();

	/** True only after every exact spawner reference has passed static validation. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack")
	bool HasValidSpawnerConfiguration() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack")
	bool IsPackInitialized() const { return bPackInitialized; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack")
	bool HasInitializationFailed() const { return bInitializationFailed; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack")
	int32 GetInitializationAttemptCount() const
	{
		return InitializationAttemptCount;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack|Configuration")
	FName GetCommandLinkId() const { return CommandLinkId; }

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack|Configuration")
	int32 GetConfiguredHoundSpawnerCount() const
	{
		return HoundSpawners.Num();
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack|Retry")
	float GetInitializationRetryInterval() const
	{
		return InitializationRetryInterval;
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Pack|Retry")
	int32 GetMaximumInitializationAttempts() const
	{
		return MaximumInitializationAttempts;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Exact spawner that must produce one ASovDominionHandler-derived NPC. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Pack|Spawners")
	TObjectPtr<ANPCSpawner> HandlerSpawner;

	/** One exact spawner per Hound. Null and repeated references fail closed. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Pack|Spawners")
	TArray<TObjectPtr<ANPCSpawner>> HoundSpawners;

	/** Required stable, encounter-unique identity, for example KennelA_HandlerLink. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Pack|Command Link")
	FName CommandLinkId = NAME_None;

	/** Delay between readiness checks while a configured spawner has no live NPC. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Pack|Retry", meta = (ClampMin = "0.01", Units = "s"))
	float InitializationRetryInterval = 0.25f;

	/** Includes the immediate BeginPlay attempt; setup fails closed when exhausted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Pack|Retry", meta = (ClampMin = "1"))
	int32 MaximumInitializationAttempts = 40;

private:
	void AttemptPackInitialization();
	bool ValidateSpawnerConfiguration(FString& OutFailureReason) const;
	bool ConfigureResolvedPack(
		ASovDominionHandler* Handler,
		const TArray<ANarrativeNPCCharacter*>& Hounds,
		FString& OutFailureReason);
	static bool HasExactlyOneReadyHornChargeSpec(
		const UNarrativeAbilitySystemComponent* AbilitySystem,
		bool& bOutSpecMissing);
	void RetryOrFail(const FString& WaitingReason);
	void FailInitialization(const FString& FailureReason);

	friend class FSovDominionPackCoordinatorContractTest;

	FTimerHandle InitializationRetryTimerHandle;

	UPROPERTY(Transient)
	bool bPackInitialized = false;

	UPROPERTY(Transient)
	bool bInitializationFailed = false;

	UPROPERTY(Transient)
	int32 InitializationAttemptCount = 0;
};
