// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Characters/SovDroneNPCBase.h"
#include "SovDroneDeathSuppressionTestFixtures.generated.h"

/** Real DroneNPC death/suppression implementation, with content-free combat setup. */
UCLASS(Transient, NotBlueprintable)
class ASovDroneDeathSuppressionTestCharacter : public ASovDroneNPCBase
{
	GENERATED_BODY()
public:
	ASovDroneDeathSuppressionTestCharacter(const FObjectInitializer& ObjectInitializer);
	void InitializeTestCombat();
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual FGameplayTagContainer GetFactions() const override;
	int32 DeathNotifications = 0;

	// Uses the same ASC notification as BeginPlay, without authored NPC assets.
	UFUNCTION(CallInEditor)
	void ForwardNativeDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC, bool bIsDead);
};
