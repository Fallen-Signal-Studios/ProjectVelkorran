// Copyright Fallen Signal Studios. All Rights Reserved.

#include "World/SovCorruptionRemedyVolume.h"

#include "Components/SovCorruptionComponent.h"
#include "Engine/World.h"
#include "Sovereign/SovGameplayTags.h"

ASovCorruptionRemedyVolume::ASovCorruptionRemedyVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RemedyTag = FSovGameplayTags::Get().Status_Cleanse_Corruption;
}

void ASovCorruptionRemedyVolume::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	ReconcileOverlaps();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReconcileTimerHandle,
			this,
			&ThisClass::ReconcileOverlaps,
			FMath::Max(OverlapReconciliationInterval, 0.1f),
			true);
	}
}

void ASovCorruptionRemedyVolume::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconcileTimerHandle);
	}
	ProcessedActors.Reset();
	Super::EndPlay(EndPlayReason);
}

void ASovCorruptionRemedyVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (HasAuthority())
	{
		TryApplyToActor(OtherActor);
	}
}

void ASovCorruptionRemedyVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (HasAuthority() && IsValid(OtherActor))
	{
		ProcessedActors.Remove(OtherActor);
	}
}

void ASovCorruptionRemedyVolume::ReconcileOverlaps()
{
	if (!HasAuthority())
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);
	TSet<TWeakObjectPtr<AActor>> CurrentOverlaps;
	for (AActor* Actor : OverlappingActors)
	{
		if (IsValid(Actor) && Actor != this)
		{
			CurrentOverlaps.Add(Actor);
			TryApplyToActor(Actor);
		}
	}

	for (auto It = ProcessedActors.CreateIterator(); It; ++It)
	{
		if (!It->IsValid() || !CurrentOverlaps.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
}

void ASovCorruptionRemedyVolume::TryApplyToActor(AActor* OtherActor)
{
	if (!HasAuthority() || !IsValid(OtherActor) || OtherActor == this
		|| OtherActor->GetWorld() != GetWorld()
		|| ProcessedActors.Contains(OtherActor))
	{
		return;
	}

	USovCorruptionComponent* CorruptionComponent =
		OtherActor->FindComponentByClass<USovCorruptionComponent>();
	if (!IsValid(CorruptionComponent) || !CorruptionComponent->IsInitialized())
	{
		return;
	}

	const bool bApplied = CorruptionComponent->ApplyRemedy(
		RemedyTag,
		ExposureReduction,
		bClearAll,
		this);
	if (bApplied)
	{
		ProcessedActors.Add(OtherActor);
	}
}
