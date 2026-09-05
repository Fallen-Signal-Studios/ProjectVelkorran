// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassCrowdRepresentationActorManagement.h"

#include "MassNarrativePedRepresentationActorManagement.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENAL_API UMassNarrativePedRepresentationActorManagement : public UMassCrowdRepresentationActorManagement
{
	GENERATED_BODY()
	friend struct FNarrativeMassRepresentationTestAccess;
	virtual EMassActorSpawnRequestAction OnPostActorSpawn(const FMassActorSpawnRequestHandle& SpawnRequestHandle, FConstStructView SpawnRequest, TSharedRef<FMassEntityManager> EntityManager) const override;

	virtual void SetActorEnabled(const EMassActorEnabledType EnabledType, AActor& Actor, const int32 EntityIdx, FMassCommandBuffer& CommandBuffer) const override;
};
