// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SovLegacyCorruptionComponent.h"
#include "Engine/TriggerBox.h"
#include "TimerManager.h"
#include "SovCorruptionFieldVolume.generated.h"

class USovLegacyCorruptionComponent;

/**
 * Authored Eclipse field. Overlap owns registration lifetime; the corruption
 * component owns elapsed-time accumulation, falloff, occlusion, and band caps.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Legacy Corruption Field Volume"))
class PROJECTVELKORRAN_API ASovCorruptionFieldVolume : public ATriggerBox
{
	GENERATED_BODY()

public:
	ASovCorruptionFieldVolume();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Corruption")
	FSovCorruptionSourceSpec GetSourceSpec() const { return SourceSpec; }

	/** Re-publishes edited runtime tuning without replaying InstantExposure. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Corruption")
	void RefreshRegisteredTargets();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Corruption")
	FSovCorruptionSourceSpec SourceSpec;

	/** Reconciles late PlayerState ASC readiness without actor/component polling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption", meta = (ClampMin = "0.1", Units = "s"))
	float OverlapReconciliationInterval = 0.5f;

private:
	struct FRegisteredTarget
	{
		TWeakObjectPtr<USovLegacyCorruptionComponent> Component;
		FSovLegacyCorruptionSourceHandle SourceHandle;
	};

	void ReconcileOverlaps();
	void TryRegisterActor(AActor* OtherActor);
	void UnregisterActor(AActor* OtherActor);
	void UnregisterAll();

	TMap<TWeakObjectPtr<AActor>, FRegisteredTarget> RegisteredTargets;
	FTimerHandle ReconcileTimerHandle;
};
