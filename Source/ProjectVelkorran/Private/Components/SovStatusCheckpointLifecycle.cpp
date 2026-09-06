// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovStatusComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovStatusCheckpointLifecycle, Log, All);

bool USovStatusComponent::IsCheckpointTargetReady() const
{
	if (bCheckpointLifecycleEnding || !IsInitialized() || IsTargetDead() || bDeferredDeathCleanupPending)
	{
		return false;
	}
	const UNarrativeAttributeSetBase* Attributes =
		AbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>();
	if (Attributes && (!FMath::IsFinite(Attributes->GetHealth()) || Attributes->GetHealth() <= 0.f))
	{
		return false;
	}
	const ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(GetOwner());
	return !Player || Player->IsCharacterReady();
}

void USovStatusComponent::BindCheckpointLifecycle()
{
	if (bCheckpointLifecycleEnding) { return; }
	if (ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(GetOwner()))
	{
		Player->OnCharacterReady.AddUniqueDynamic(this, &ThisClass::HandleCheckpointCharacterReady);
	}
}

void USovStatusComponent::CleanupCheckpointLifecycle()
{
	bCheckpointLifecycleEnding = true;
	++CheckpointLifecycleGeneration;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredCheckpointRestoreTimer);
	}
	if (ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(GetOwner()))
	{
		Player->OnCharacterReady.RemoveDynamic(this, &ThisClass::HandleCheckpointCharacterReady);
	}
	bAwaitingRestoreBoundary = false;
}

bool USovStatusComponent::StageNativeCheckpointState(const FSovStatusCheckpointState& State)
{
	AActor* OwnerActor = GetOwner();
	if (bRestoringCheckpoint || bClearingOwnedEffects || bChangingAbilitySystem
		|| bCompletingRestoreBoundary || bCheckpointLifecycleEnding || !IsValid(this) || !IsValid(OwnerActor)
		|| OwnerActor->IsActorBeingDestroyed() || !OwnerActor->HasAuthority()
		|| OwnerActor->FindComponentByClass<USovStatusComponent>() != this)
	{
		return false;
	}
	if (DefinitionRegistry.IsEmpty())
	{
		BuildDefinitionRegistry();
	}
	if (!ValidateCheckpointState(State))
	{
		return false;
	}
	BindCheckpointLifecycle();
	if (!Cast<ANarrativeCharacter>(OwnerActor))
	{
		// Content-free actors and direct integrations have no Narrative resource
		// stage. Their existing explicit readiness contract remains sufficient.
		return RestoreCheckpointState(State);
	}

	// Pawn component records precede PlayerState/NPC resource restoration.
	// Preserve the admitted semantic state until the actual completion boundary.
	PendingCheckpointState = State;
	bHasPendingCheckpointState = true;
	bAwaitingRestoreBoundary = true;
	const uint64 Generation = ++CheckpointLifecycleGeneration;
	TStrongObjectPtr<USovStatusComponent> KeepComponent(this);
	TStrongObjectPtr<AActor> KeepOwner(OwnerActor);
	if (IsInitialized())
	{
		TStrongObjectPtr<UNarrativeAbilitySystemComponent> KeepASC(AbilitySystemComponent.Get());
		TStrongObjectPtr<const UNarrativeAttributeSetBase> KeepAttributes(
			KeepASC->GetSet<UNarrativeAttributeSetBase>());
		const uint64 ActorInfoEpoch = KeepASC->GetCombatActorInfoEpoch();
		const uint64 LifeEpoch = KeepAttributes.IsValid() ? KeepAttributes->GetCombatLifeEpoch() : 0;
		const ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(OwnerActor);
		const int32 InitializationGeneration = Player ? Player->GetCharacterInitializationGeneration() : 0;
		for (const FSovStatusPresentationEntry& Entry : ReplicatedStatusPresentation)
		{
			if (!PendingCheckpointPresentation.ContainsByPredicate([&Entry](const FSovStatusPresentationEntry& Pending)
				{ return Pending.RequestTag == Entry.RequestTag; }))
			{
				PendingCheckpointPresentation.Add(Entry);
			}
		}
		TGuardValue<bool> BoundaryGuard(bCompletingRestoreBoundary, true);
		TGuardValue<bool> RestoreGuard(bRestoringCheckpoint, true);
		TGuardValue<FSovStatusCheckpointState> SnapshotGuard(RestoringCheckpointState, PendingCheckpointState);
		// Old continuous modifiers must leave before explicit saved resource
		// writes. Capture still sees the full pending snapshot throughout cleanup.
		ClearAllTrackedEffects(EStatusRemovalPolicy::CheckpointRestore);
		if (!IsValid(this) || bCheckpointLifecycleEnding || CheckpointLifecycleGeneration != Generation
			|| !IsValid(OwnerActor) || OwnerActor->IsActorBeingDestroyed() || GetOwner() != OwnerActor
			|| OwnerActor->FindComponentByClass<USovStatusComponent>() != this
			|| !IsValid(KeepASC.Get()) || AbilitySystemComponent.Get() != KeepASC.Get() || !IsInitialized()
			|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor) != KeepASC.Get()
			|| KeepASC->GetCombatActorInfoEpoch() != ActorInfoEpoch
			|| KeepASC->GetSet<UNarrativeAttributeSetBase>() != KeepAttributes.Get()
			|| (KeepAttributes.IsValid() && KeepAttributes->GetCombatLifeEpoch() != LifeEpoch)
			|| (Player && Player->GetCharacterInitializationGeneration() != InitializationGeneration))
		{
			return false;
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredCheckpointRestoreTimer);
		const ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(OwnerActor);
		if (!Player || Player->IsCharacterReady())
		{
			// Generic Narrative loads are synchronous and have no project resource
			// callback. Campaign restore explicitly flushes before releasing actors;
			// this one-shot fallback is only for generic loads of an already-ready actor.
			DeferredCheckpointRestoreTimer = World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ThisClass::HandleDeferredCheckpointRestore, Generation));
		}
	}
	return true;
}

bool USovStatusComponent::CompletePendingCheckpointRestore()
{
	AActor* OwnerActor = GetOwner();
	if (bRestoringCheckpoint || bClearingOwnedEffects || bChangingAbilitySystem
		|| bCompletingRestoreBoundary || bCheckpointLifecycleEnding || !IsValid(this) || !IsValid(OwnerActor)
		|| OwnerActor->IsActorBeingDestroyed() || !OwnerActor->HasAuthority()
		|| OwnerActor->FindComponentByClass<USovStatusComponent>() != this)
	{
		if (IsValid(this)) { bLastSaveRecordLoadAccepted = false; }
		return false;
	}
	TGuardValue<bool> BoundaryGuard(bCompletingRestoreBoundary, true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredCheckpointRestoreTimer);
	}
	bAwaitingRestoreBoundary = false;
	if (!bHasPendingCheckpointState)
	{
		return true;
	}
	if (!IsInitialized())
	{
		if (AbilitySystemComponent.Get() != nullptr)
		{
			// A previously bound ASC that moved to another avatar is retired,
			// not an ordinary delayed-initialization queue.
			bLastSaveRecordLoadAccepted = false;
			return false;
		}
		// An early generic load may precede actor-info binding. Initialization
		// retries the same preserved queue, without inventing a polling loop.
		return true;
	}

	TStrongObjectPtr<USovStatusComponent> KeepComponent(this);
	TStrongObjectPtr<AActor> KeepOwner(OwnerActor);
	TStrongObjectPtr<UNarrativeAbilitySystemComponent> KeepASC(AbilitySystemComponent.Get());
	TStrongObjectPtr<const UNarrativeAttributeSetBase> KeepAttributes(
		KeepASC->GetSet<UNarrativeAttributeSetBase>());
	const uint64 ActorInfoEpoch = KeepASC->GetCombatActorInfoEpoch();
	const uint64 LifecycleGeneration = CheckpointLifecycleGeneration;
	const uint64 LifeEpoch = KeepAttributes.IsValid() ? KeepAttributes->GetCombatLifeEpoch() : 0;
	const ANarrativePlayerCharacter* Player = Cast<ANarrativePlayerCharacter>(OwnerActor);
	const int32 InitializationGeneration = Player ? Player->GetCharacterInitializationGeneration() : 0;
	const auto StillOwnsBoundary = [&]()
	{
		return IsValid(this) && !bCheckpointLifecycleEnding
			&& CheckpointLifecycleGeneration == LifecycleGeneration
			&& IsValid(OwnerActor) && !OwnerActor->IsActorBeingDestroyed()
			&& IsValid(KeepASC.Get()) && GetOwner() == OwnerActor
			&& OwnerActor->FindComponentByClass<USovStatusComponent>() == this
			&& AbilitySystemComponent.Get() == KeepASC.Get() && IsInitialized()
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor) == KeepASC.Get()
			&& KeepASC->GetCombatActorInfoEpoch() == ActorInfoEpoch
			&& KeepASC->GetSet<UNarrativeAttributeSetBase>() == KeepAttributes.Get()
			&& (!KeepAttributes.IsValid() || KeepAttributes->GetCombatLifeEpoch() == LifeEpoch)
			&& (!Player || Player->GetCharacterInitializationGeneration() == InitializationGeneration);
	};

	// Revive can occur in the fatal frame. Its queued cleanup must finish before
	// installing saved handles, or the old timer would erase the restored state.
	if (bDeferredDeathCleanupPending && !IsTargetDead())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(DeferredDeathCleanupTimer);
		}
		bDeferredDeathCleanupPending = false;
		ClearAllTrackedEffects(EStatusRemovalPolicy::Death);
		if (!StillOwnsBoundary())
		{
			bLastSaveRecordLoadAccepted = false;
			return false;
		}
	}
	if (!IsCheckpointTargetReady())
	{
		// Managed character initialization publishes readiness after saved
		// resources and campaign data settle; that event completes this queue.
		const bool bLiving = !IsTargetDead() && (!KeepAttributes.IsValid()
			|| (FMath::IsFinite(KeepAttributes->GetHealth()) && KeepAttributes->GetHealth() > 0.f));
		const bool bAwaitingPlayerReadiness = bLiving && Player && !Player->IsCharacterReady();
		if (!bAwaitingPlayerReadiness) { bLastSaveRecordLoadAccepted = false; }
		return bAwaitingPlayerReadiness;
	}
	ApplyPendingCheckpointRestore();
	const bool bCompleted = StillOwnsBoundary() && !bHasPendingCheckpointState;
	bLastSaveRecordLoadAccepted = bCompleted;
	return bCompleted;
}

void USovStatusComponent::HandleCheckpointCharacterReady(ANarrativePlayerCharacter* Character)
{
	if (Character == GetOwner() && IsValid(Character) && Character->IsCharacterReady()
		&& bHasPendingCheckpointState)
	{
		if (!CompletePendingCheckpointRestore() && IsValid(this))
		{
			bLastSaveRecordLoadAccepted = false;
			UE_LOG(LogSovStatusCheckpointLifecycle, Verbose,
				TEXT("Ready-boundary status restore rejected for %s; snapshot retained."), *GetNameSafe(GetOwner()));
		}
	}
}

void USovStatusComponent::HandleDeferredCheckpointRestore(const uint64 ExpectedGeneration)
{
	if (ExpectedGeneration == CheckpointLifecycleGeneration && bHasPendingCheckpointState)
	{
		if (!CompletePendingCheckpointRestore() && IsValid(this))
		{
			bLastSaveRecordLoadAccepted = false;
			UE_LOG(LogSovStatusCheckpointLifecycle, Verbose,
				TEXT("Deferred status restore rejected for %s; snapshot retained."), *GetNameSafe(GetOwner()));
		}
	}
}
