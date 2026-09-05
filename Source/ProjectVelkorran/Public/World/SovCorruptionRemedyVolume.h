// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerBox.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "SovCorruptionRemedyVolume.generated.h"

/** One-shot-per-entry authored remedy/cleanse volume. */
UCLASS(Blueprintable, meta = (DisplayName = "Sovereign Legacy Corruption Remedy Volume"))
class PROJECTVELKORRAN_API ASovCorruptionRemedyVolume : public ATriggerBox
{
	GENERATED_BODY()

public:
	ASovCorruptionRemedyVolume();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	/** Must exactly match a source-authored remedy when one is specified. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Corruption|Remedy")
	FGameplayTag RemedyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Corruption|Remedy", meta = (ClampMin = "0.0"))
	float ExposureReduction = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sovereign|Corruption|Remedy")
	bool bClearAll = false;

	/** Allows a target whose PlayerState ASC becomes ready after overlap begins. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Corruption|Remedy", meta = (ClampMin = "0.1", Units = "s"))
	float OverlapReconciliationInterval = 0.5f;

private:
	void ReconcileOverlaps();
	void TryApplyToActor(AActor* OtherActor);

	TSet<TWeakObjectPtr<AActor>> ProcessedActors;
	FTimerHandle ReconcileTimerHandle;
};
