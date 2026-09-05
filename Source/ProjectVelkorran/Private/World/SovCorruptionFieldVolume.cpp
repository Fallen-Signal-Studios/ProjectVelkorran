// Copyright Fallen Signal Studios. All Rights Reserved.

#include "World/SovCorruptionFieldVolume.h"

#include "Components/SovLegacyCorruptionComponent.h"
#include "Engine/World.h"
#include "Sovereign/SovGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovCorruptionField, Log, All);

ASovCorruptionFieldVolume::ASovCorruptionFieldVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SourceSpec.SourceId = TEXT("Prototype.EclipseField");
	SourceSpec.RemedyTag = FSovGameplayTags::Get().Status_Cleanse_Corruption;
	SourceSpec.RemedyInstruction = NSLOCTEXT(
		"SovereignCorruption",
		"PrototypeFieldRemedy",
		"Reach an Eclipse purge point to reduce corruption.");
	SourceSpec.PresentationProfile = TEXT("EclipseField.Default");
	SourceSpec.AccessibilitySubstitute = NSLOCTEXT(
		"SovereignCorruption",
		"PrototypeFieldAccessibility",
		"Directional corruption warning with meter and band text.");
}

void ASovCorruptionFieldVolume::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}

	if (!SourceSpec.HasValidNumbers())
	{
		UE_LOG(
			LogSovCorruptionField,
			Warning,
			TEXT("%s has an invalid corruption SourceSpec; it will not register targets."),
			*GetNameSafe(this));
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

void ASovCorruptionFieldVolume::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconcileTimerHandle);
	}
	if (HasAuthority())
	{
		UnregisterAll();
	}
	Super::EndPlay(EndPlayReason);
}

void ASovCorruptionFieldVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (HasAuthority())
	{
		TryRegisterActor(OtherActor);
	}
}

void ASovCorruptionFieldVolume::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (HasAuthority())
	{
		UnregisterActor(OtherActor);
	}
}

void ASovCorruptionFieldVolume::RefreshRegisteredTargets()
{
	if (!HasAuthority())
	{
		return;
	}
	if (!SourceSpec.HasValidNumbers())
	{
		UnregisterAll();
		return;
	}

	for (TPair<TWeakObjectPtr<AActor>, FRegisteredTarget>& Pair : RegisteredTargets)
	{
		FRegisteredTarget& Target = Pair.Value;
		if (Target.Component.IsValid() && Target.SourceHandle.IsValid())
		{
			Target.Component->UpdateCorruptionSource(
				Target.SourceHandle,
				SourceSpec);
		}
	}
}

void ASovCorruptionFieldVolume::ReconcileOverlaps()
{
	if (!HasAuthority())
	{
		return;
	}
	if (!SourceSpec.HasValidNumbers())
	{
		UnregisterAll();
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
			TryRegisterActor(Actor);
		}
	}

	TArray<TWeakObjectPtr<AActor>> StaleActors;
	for (const TPair<TWeakObjectPtr<AActor>, FRegisteredTarget>& Pair : RegisteredTargets)
	{
		if (!Pair.Key.IsValid() || !CurrentOverlaps.Contains(Pair.Key))
		{
			StaleActors.Add(Pair.Key);
		}
	}
	for (const TWeakObjectPtr<AActor>& StaleActor : StaleActors)
	{
		UnregisterActor(StaleActor.Get());
		if (!StaleActor.IsValid())
		{
			if (FRegisteredTarget Registered = RegisteredTargets.FindRef(StaleActor);
				Registered.Component.IsValid())
			{
				Registered.Component->UnregisterCorruptionSource(
					Registered.SourceHandle);
			}
			RegisteredTargets.Remove(StaleActor);
		}
	}
}

void ASovCorruptionFieldVolume::TryRegisterActor(AActor* OtherActor)
{
	if (!HasAuthority() || !IsValid(OtherActor) || OtherActor == this
		|| OtherActor->GetWorld() != GetWorld())
	{
		return;
	}
	if (FRegisteredTarget* ExistingTarget = RegisteredTargets.Find(OtherActor))
	{
		if (ExistingTarget->Component.IsValid()
			&& ExistingTarget->Component->IsInitialized()
			&& ExistingTarget->Component->HasRegisteredCorruptionSource(
				ExistingTarget->SourceHandle))
		{
			return;
		}

		if (ExistingTarget->Component.IsValid())
		{
			ExistingTarget->Component->UnregisterCorruptionSource(
				ExistingTarget->SourceHandle);
		}
		RegisteredTargets.Remove(OtherActor);
	}

	USovLegacyCorruptionComponent* CorruptionComponent =
		OtherActor->FindComponentByClass<USovLegacyCorruptionComponent>();
	if (!IsValid(CorruptionComponent) || !CorruptionComponent->IsInitialized())
	{
		return;
	}

	const FSovLegacyCorruptionSourceHandle SourceHandle =
		CorruptionComponent->RegisterCorruptionSource(this, SourceSpec);
	if (!SourceHandle.IsValid())
	{
		return;
	}

	FRegisteredTarget& Target = RegisteredTargets.Add(OtherActor);
	Target.Component = CorruptionComponent;
	Target.SourceHandle = SourceHandle;
}

void ASovCorruptionFieldVolume::UnregisterActor(AActor* OtherActor)
{
	if (!HasAuthority())
	{
		return;
	}

	FRegisteredTarget Registered;
	bool bFound = false;
	if (IsValid(OtherActor))
	{
		bFound = RegisteredTargets.RemoveAndCopyValue(OtherActor, Registered);
	}
	if (!bFound)
	{
		return;
	}
	if (Registered.Component.IsValid())
	{
		Registered.Component->UnregisterCorruptionSource(
			Registered.SourceHandle);
	}
}

void ASovCorruptionFieldVolume::UnregisterAll()
{
	for (const TPair<TWeakObjectPtr<AActor>, FRegisteredTarget>& Pair : RegisteredTargets)
	{
		if (Pair.Value.Component.IsValid())
		{
			Pair.Value.Component->UnregisterCorruptionSource(
				Pair.Value.SourceHandle);
		}
	}
	RegisteredTargets.Reset();
}
