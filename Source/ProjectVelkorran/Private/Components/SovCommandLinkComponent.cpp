// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovCommandLinkComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SovWeakPointComponent.h"
#include "GameplayEffect.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovCommandLink, Log, All);

USovCommandLinkComponent::USovCommandLinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USovCommandLinkComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	for (int32 Index = LinkedActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* LinkedActor = LinkedActors[Index].Get();
		if (!IsValid(LinkedActor)
			|| LinkedActor->GetWorld() != GetWorld()
			|| LinkedActor == GetOwner())
		{
			LinkedActors.RemoveAt(Index);
		}
	}
	for (int32 Index = LinkedActors.Num() - 1; Index >= 0; --Index)
	{
		if (LinkedActors.Find(LinkedActors[Index]) != Index)
		{
			LinkedActors.RemoveAt(Index);
		}
	}

	if (!HasValidCommandLinkConfiguration())
	{
		UE_LOG(
			LogSovCommandLink,
			Warning,
			TEXT("%s has an invalid Command Link configuration (stable LinkId, finite reveal duration, and at least one unique live participant are required); it will remain inactive."),
			*GetNameSafe(GetOwner()));
	}

	BindParticipant(GetOwner());
	for (AActor* LinkedActor : GetLinkedActors())
	{
		BindParticipant(LinkedActor);
	}

	if (bStartsActive)
	{
		ActivateCommandLink(GetOwner());
	}
}

void USovCommandLinkComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClearAllParticipantContributions();
	}
	UnbindAllParticipants();
	Super::EndPlay(EndPlayReason);
}

void USovCommandLinkComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovCommandLinkComponent, ReplicationState);
	DOREPLIFETIME(USovCommandLinkComponent, LinkedActors);
}

TArray<AActor*> USovCommandLinkComponent::GetLinkedActors() const
{
	TArray<AActor*> Result;
	Result.Reserve(LinkedActors.Num());
	for (AActor* Actor : LinkedActors)
	{
		if (IsValid(Actor))
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

bool USovCommandLinkComponent::HasValidCommandLinkConfiguration() const
{
	if (LinkId == NAME_None || !FMath::IsFinite(WeakPointRevealDuration)
		|| WeakPointRevealDuration < 0.0f
		|| BuildParticipantSnapshot().IsEmpty())
	{
		return false;
	}

	TSet<const AActor*> SeenActors;
	for (const AActor* Actor : LinkedActors)
	{
		if (!IsValid(Actor) || Actor->GetWorld() != GetWorld()
			|| Actor == GetOwner() || SeenActors.Contains(Actor))
		{
			return false;
		}
		SeenActors.Add(Actor);
	}
	return true;
}

bool USovCommandLinkComponent::ContainsLinkedActor(const AActor* Actor) const
{
	return IsValid(Actor) && LinkedActors.Contains(Actor);
}

bool USovCommandLinkComponent::RegisterLinkedActor(AActor* Actor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| bCommandLinkMutationInProgress)
	{
		return false;
	}

	PruneInvalidLinkedActors();
	if (!IsValid(Actor)
		|| Actor->GetWorld() != GetWorld()
		|| Actor == GetOwner() || LinkedActors.Contains(Actor))
	{
		return false;
	}

	LinkedActors.Add(Actor);
	BindParticipant(Actor);
	ReconcileParticipant(Actor);
	WakeOwnerForReplication();
	return true;
}

bool USovCommandLinkComponent::UnregisterLinkedActor(AActor* Actor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| bCommandLinkMutationInProgress)
	{
		return false;
	}

	// Do not require IsValid here. OnDestroyed can leave an authored UObject
	// reference addressable briefly even though normal validity checks reject it.
	// Removing that exact entry keeps a later reset/reactivation from failing its
	// otherwise-valid configuration check.
	if (!LinkedActors.Contains(Actor))
	{
		PruneInvalidLinkedActors();
		return false;
	}

	RemoveParticipantContributions(Actor);
	LinkedActors.Remove(Actor);
	if (Actor != ReplicationState.CommandSource.Get())
	{
		UnbindParticipant(Actor);
	}
	PruneInvalidLinkedActors();
	WakeOwnerForReplication();
	return true;
}

bool USovCommandLinkComponent::ActivateCommandLink(AActor* InCommandSource)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| bCommandLinkMutationInProgress
		|| ReplicationState.State == ESovCommandLinkState::Active)
	{
		return false;
	}
	PruneInvalidLinkedActors();
	if (!HasValidCommandLinkConfiguration())
	{
		return false;
	}

	AActor* ResolvedSource = IsValid(InCommandSource)
		? InCommandSource
		: GetOwner();
	if (!IsValid(ResolvedSource) || ResolvedSource->GetWorld() != GetWorld())
	{
		return false;
	}

	const ESovCommandLinkState OldState = ReplicationState.State;
	bCommandLinkMutationInProgress = true;
	AActor* PreviousSource = ReplicationState.CommandSource.Get();
	ClearAllParticipantContributions();
	ReplicationState.LinkId = LinkId;
	ReplicationState.LinkInstanceId = FGuid::NewGuid();
	ReplicationState.LastSeverTransactionId.Invalidate();
	ReplicationState.CommandSource = ResolvedSource;
	ReplicationState.LastSeveredBy = nullptr;
	ReplicationState.bLastSeverEligibleForEchoReward = false;
	ReplicationState.LastSeverAffectedActors.Reset();
	if (IsValid(PreviousSource)
		&& PreviousSource != ResolvedSource
		&& PreviousSource != GetOwner()
		&& !LinkedActors.Contains(PreviousSource))
	{
		UnbindParticipant(PreviousSource);
	}
	BindParticipant(ResolvedSource);
	ReplicationState.State = ESovCommandLinkState::Active;
	++ReplicationState.Revision;
	ReconcileAllParticipants();
	WakeOwnerForReplication();
	OnCommandLinkStateChanged.Broadcast(
		OldState,
		ESovCommandLinkState::Active);
	bCommandLinkMutationInProgress = false;
	ProcessDeferredMutation();
	return true;
}

FSovCommandLinkSnapshot USovCommandLinkComponent::CaptureCommandLinkState() const
{
	FSovCommandLinkSnapshot Snapshot;
	Snapshot.LinkId = GetLinkId();
	Snapshot.State = ReplicationState.State;
	Snapshot.LinkInstanceId = ReplicationState.LinkInstanceId;
	Snapshot.LastSeverTransactionId = ReplicationState.LastSeverTransactionId;
	return Snapshot;
}

bool USovCommandLinkComponent::RestoreCommandLinkState(const FSovCommandLinkSnapshot& Snapshot, AActor* CommandSource, const TArray<AActor*>& Participants)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bCommandLinkMutationInProgress
		|| Snapshot.LinkId.IsNone() || Snapshot.LinkId != LinkId
		|| (Snapshot.State != ESovCommandLinkState::Inactive && Snapshot.State != ESovCommandLinkState::Active && Snapshot.State != ESovCommandLinkState::Severed)
		|| (Snapshot.State != ESovCommandLinkState::Inactive && !Snapshot.LinkInstanceId.IsValid())
		|| (Snapshot.State == ESovCommandLinkState::Severed && !Snapshot.LastSeverTransactionId.IsValid())) { return false; }
	AActor* Source = IsValid(CommandSource) ? CommandSource : GetOwner();
	if (Source->GetWorld() != GetWorld()) { return false; }
	TSet<AActor*> UniqueParticipants;
	for (AActor* Participant : Participants)
	{
		if (!IsValid(Participant) || Participant == GetOwner() || Participant->GetWorld() != GetWorld() || UniqueParticipants.Contains(Participant)) { return false; }
		UniqueParticipants.Add(Participant);
	}
	const ESovCommandLinkState PreviousState = ReplicationState.State;
	bCommandLinkMutationInProgress = true;
	ClearAllParticipantContributions();
	UnbindAllParticipants();
	LinkedActors.Reset();
	for (AActor* Participant : Participants) { LinkedActors.Add(Participant); }
	ReplicationState.State = Snapshot.State;
	ReplicationState.LinkId = Snapshot.LinkId;
	ReplicationState.LinkInstanceId = Snapshot.LinkInstanceId;
	ReplicationState.LastSeverTransactionId = Snapshot.LastSeverTransactionId;
	ReplicationState.CommandSource = Source;
	ReplicationState.LastSeveredBy = nullptr;
	ReplicationState.bLastSeverEligibleForEchoReward = false;
	ReplicationState.LastSeverAffectedActors.Reset();
	++ReplicationState.Revision;
	BindParticipant(GetOwner());
	BindParticipant(Source);
	for (AActor* Participant : Participants) { BindParticipant(Participant); }
	ReconcileAllParticipants();
	WakeOwnerForReplication();
	if (PreviousState != Snapshot.State) { OnCommandLinkStateChanged.Broadcast(PreviousState, Snapshot.State); }
	bCommandLinkMutationInProgress = false;
	ProcessDeferredMutation();
	return true;
}

void USovCommandLinkComponent::ResetCommandLink()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (bCommandLinkMutationInProgress)
	{
		bDeferredResetRequested = true;
		return;
	}
	PruneInvalidLinkedActors();

	AActor* ExistingSource = ReplicationState.CommandSource.Get();
	const ESovCommandLinkState OldState = ReplicationState.State;
	bCommandLinkMutationInProgress = true;
	ReplicationState.State = ESovCommandLinkState::Inactive;
	++ReplicationState.Revision;
	ClearAllParticipantContributions();
	ReplicationState.LastSeverTransactionId.Invalidate();
	ReplicationState.LinkInstanceId.Invalidate();
	ReplicationState.LastSeveredBy = nullptr;
	ReplicationState.bLastSeverEligibleForEchoReward = false;
	ReplicationState.LastSeverAffectedActors.Reset();
	ReconcileAllParticipants();
	WakeOwnerForReplication();
	if (OldState != ESovCommandLinkState::Inactive)
	{
		OnCommandLinkStateChanged.Broadcast(
			OldState,
			ESovCommandLinkState::Inactive);
	}
	const bool bShouldReactivate = bStartsActive
		&& !bDeferredDeactivateRequested;
	bCommandLinkMutationInProgress = false;
	ProcessDeferredMutation();
	if (bShouldReactivate
		&& ReplicationState.State == ESovCommandLinkState::Inactive)
	{
		ActivateCommandLink(
			IsValid(ExistingSource) ? ExistingSource : GetOwner());
	}
}

ESovCommandLinkSeverResolution USovCommandLinkComponent::TrySeverCommandLink(
	AActor* SeveredBy,
	FSovCommandLinkSeverResult& OutResult)
{
	OutResult = FSovCommandLinkSeverResult();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(SeveredBy)
		|| SeveredBy->GetWorld() != GetWorld()
		|| GetLinkId() == NAME_None)
	{
		return ESovCommandLinkSeverResolution::Invalid;
	}
	if (ReplicationState.State == ESovCommandLinkState::Severed)
	{
		OutResult = BuildLastSeverResult();
		return ESovCommandLinkSeverResolution::AlreadySevered;
	}
	if (bCommandLinkMutationInProgress)
	{
		return ESovCommandLinkSeverResolution::Invalid;
	}
	if (ReplicationState.State != ESovCommandLinkState::Active)
	{
		return ESovCommandLinkSeverResolution::Inactive;
	}
	if (!ReplicationState.LinkInstanceId.IsValid())
	{
		return ESovCommandLinkSeverResolution::Invalid;
	}
	if (!bSeverable || IsDisruptionImmune())
	{
		return ESovCommandLinkSeverResolution::Immune;
	}
	const bool bHostileSeverer = IsHostileToAnyParticipant(SeveredBy);
	if (!bHostileSeverer)
	{
		return ESovCommandLinkSeverResolution::NotHostile;
	}

	OutResult.TransactionId = FGuid::NewGuid();
	OutResult.LinkInstanceId = ReplicationState.LinkInstanceId;
	OutResult.LinkId = GetLinkId();
	OutResult.LinkOwner = GetOwner();
	OutResult.CommandSource = IsValid(ReplicationState.CommandSource.Get())
		? ReplicationState.CommandSource.Get()
		: GetOwner();
	OutResult.SeveredBy = SeveredBy;
	OutResult.bEligibleForEchoReward = bCanGrantSeleneEcho;
	for (AActor* Participant : BuildParticipantSnapshot())
	{
		OutResult.AffectedActors.Add(Participant);
	}

	// Commit the unique transition before any tag/effect removal. Those removals
	// can synchronously invoke GAS listeners; a re-entrant request must observe
	// AlreadySevered and the same immutable transaction, never mint a duplicate.
	const ESovCommandLinkState OldState = ReplicationState.State;
	ReplicationState.LastSeverTransactionId = OutResult.TransactionId;
	ReplicationState.LastSeveredBy = SeveredBy;
	ReplicationState.bLastSeverEligibleForEchoReward =
		OutResult.bEligibleForEchoReward;
	ReplicationState.LastSeverAffectedActors = OutResult.AffectedActors;
	ReplicationState.State = ESovCommandLinkState::Severed;
	++ReplicationState.Revision;
	bCommandLinkMutationInProgress = true;
	ClearAllParticipantContributions();
	ReconcileAllParticipants();
	WakeOwnerForReplication();
	OnCommandLinkStateChanged.Broadcast(
		OldState,
		ESovCommandLinkState::Severed);

	if (bRevealWeakPointsOnSever && WeakPointRevealDuration > KINDA_SMALL_NUMBER)
	{
		RevealParticipantWeakPoints(OutResult.AffectedActors, SeveredBy);
	}

	FGameplayEventData EventPayload;
	EventPayload.EventTag = FSovGameplayTags::Get().Event_CommandLink_Severed;
	EventPayload.Instigator = SeveredBy;
	EventPayload.Target = GetOwner();
	EventPayload.OptionalObject = this;
	EventPayload.EventMagnitude = 1.0f;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		SeveredBy,
		EventPayload.EventTag,
		EventPayload);

	OnCommandLinkSevered.Broadcast(OutResult);
	bCommandLinkMutationInProgress = false;
	ProcessDeferredMutation();
	return ESovCommandLinkSeverResolution::NewlySevered;
}

TArray<AActor*> USovCommandLinkComponent::BuildParticipantSnapshot() const
{
	TArray<AActor*> Participants;
	const auto IsLiveParticipant = [this](AActor* Actor)
	{
		if (!IsValid(Actor) || Actor->GetWorld() != GetWorld())
		{
			return false;
		}
		const UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
			Cast<UNarrativeAbilitySystemComponent>(ResolveAbilitySystem(Actor));
		return !IsValid(NarrativeAbilitySystem)
			|| !NarrativeAbilitySystem->IsDead();
	};

	if (bIncludeOwnerAsParticipant && IsLiveParticipant(GetOwner()))
	{
		Participants.Add(GetOwner());
	}
	for (AActor* LinkedActor : LinkedActors)
	{
		if (IsLiveParticipant(LinkedActor))
		{
			Participants.AddUnique(LinkedActor);
		}
	}
	return Participants;
}

int32 USovCommandLinkComponent::PruneInvalidLinkedActors()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| bCommandLinkMutationInProgress)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	for (int32 Index = LinkedActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* LinkedActor = LinkedActors[Index].Get();
		if (IsValid(LinkedActor))
		{
			continue;
		}

		RemoveParticipantContributions(LinkedActor);
		LinkedActors.RemoveAt(Index);
		++RemovedCount;
	}

	if (RemovedCount > 0)
	{
		WakeOwnerForReplication();
	}
	return RemovedCount;
}

UAbilitySystemComponent* USovCommandLinkComponent::ResolveAbilitySystem(
	AActor* Actor) const
{
	return IsValid(Actor)
		? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)
		: nullptr;
}

bool USovCommandLinkComponent::IsDisruptionImmune() const
{
	if (!bRespectDeviceDisableImmunity)
	{
		return false;
	}

	const FGameplayTag ImmunityTag =
		FSovGameplayTags::Get().Status_Immunity_DeviceDisable;
	AActor* ActorsToCheck[] = {
		GetOwner(),
		ReplicationState.CommandSource.Get()};
	for (AActor* Actor : ActorsToCheck)
	{
		if (UAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Actor))
		{
			if (AbilitySystem->HasMatchingGameplayTag(ImmunityTag))
			{
				return true;
			}
		}
	}
	return false;
}

bool USovCommandLinkComponent::IsHostileToAnyParticipant(
	const AActor* Instigator) const
{
	const INarrativeTeamAgentInterface* InstigatorTeam =
		Cast<const INarrativeTeamAgentInterface>(Instigator);
	if (!InstigatorTeam)
	{
		return false;
	}

	const TArray<AActor*> ActorsToCheck = BuildParticipantSnapshot();
	for (AActor* Participant : ActorsToCheck)
	{
		if (IsValid(Participant)
			&& Participant != Instigator
			&& InstigatorTeam->GetTeamAttitudeTowards(*Participant)
				== ETeamAttitude::Hostile)
		{
			return true;
		}
	}
	return false;
}

void USovCommandLinkComponent::BindParticipant(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	Actor->OnDestroyed.AddUniqueDynamic(
		this,
		&ThisClass::HandleParticipantDestroyed);
	if (ANarrativeCharacter* NarrativeCharacter =
		Cast<ANarrativeCharacter>(Actor))
	{
		NarrativeCharacter->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleParticipantASCInitialized);
		BoundParticipantCharacters.Add(NarrativeCharacter);
	}

	UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(ResolveAbilitySystem(Actor));
	if (IsValid(NarrativeAbilitySystem)
		&& !BoundParticipantAbilitySystems.Contains(NarrativeAbilitySystem))
	{
		NarrativeAbilitySystem->OnDeathStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleParticipantDeathStateChanged);
		BoundParticipantAbilitySystems.Add(NarrativeAbilitySystem);
	}
}

void USovCommandLinkComponent::UnbindParticipant(AActor* Actor)
{
	if (IsValid(Actor))
	{
		Actor->OnDestroyed.RemoveDynamic(
			this,
			&ThisClass::HandleParticipantDestroyed);
		if (ANarrativeCharacter* NarrativeCharacter =
			Cast<ANarrativeCharacter>(Actor))
		{
			NarrativeCharacter->OnASCInitialized.RemoveDynamic(
				this,
				&ThisClass::HandleParticipantASCInitialized);
			BoundParticipantCharacters.Remove(NarrativeCharacter);
		}
	}

	UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(ResolveAbilitySystem(Actor));
	if (IsValid(NarrativeAbilitySystem))
	{
		NarrativeAbilitySystem->OnDeathStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleParticipantDeathStateChanged);
		BoundParticipantAbilitySystems.Remove(NarrativeAbilitySystem);
	}
}

void USovCommandLinkComponent::UnbindAllParticipants()
{
	if (IsValid(GetOwner()))
	{
		GetOwner()->OnDestroyed.RemoveDynamic(
			this,
			&ThisClass::HandleParticipantDestroyed);
	}
	if (IsValid(ReplicationState.CommandSource.Get()))
	{
		ReplicationState.CommandSource->OnDestroyed.RemoveDynamic(
			this,
			&ThisClass::HandleParticipantDestroyed);
	}
	for (AActor* Actor : LinkedActors)
	{
		if (IsValid(Actor))
		{
			Actor->OnDestroyed.RemoveDynamic(
				this,
				&ThisClass::HandleParticipantDestroyed);
		}
	}
	for (const TWeakObjectPtr<ANarrativeCharacter>& WeakCharacter :
		BoundParticipantCharacters)
	{
		if (ANarrativeCharacter* Character = WeakCharacter.Get())
		{
			Character->OnASCInitialized.RemoveDynamic(
				this,
				&ThisClass::HandleParticipantASCInitialized);
		}
	}
	BoundParticipantCharacters.Reset();
	for (const TWeakObjectPtr<UNarrativeAbilitySystemComponent>& WeakAbilitySystem :
		BoundParticipantAbilitySystems)
	{
		if (UNarrativeAbilitySystemComponent* AbilitySystem = WeakAbilitySystem.Get())
		{
			AbilitySystem->OnDeathStateChanged.RemoveDynamic(
				this,
				&ThisClass::HandleParticipantDeathStateChanged);
		}
	}
	BoundParticipantAbilitySystems.Reset();
}

void USovCommandLinkComponent::ReconcileParticipant(AActor* Actor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Actor))
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Actor);
	if (!IsValid(AbilitySystem))
	{
		return;
	}
	if (const UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystem);
		IsValid(NarrativeAbilitySystem) && NarrativeAbilitySystem->IsDead())
	{
		RemoveParticipantContributions(Actor);
		return;
	}
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	const TWeakObjectPtr<AActor> WeakActor(Actor);

	if (ReplicationState.State == ESovCommandLinkState::Active)
	{
		if (SeveredTagRecipients.Remove(WeakActor) > 0)
		{
			AbilitySystem->RemoveLooseGameplayTag(
				Tags.State_CommandLink_Severed,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		if (!ActiveTagRecipients.Contains(WeakActor))
		{
			AbilitySystem->AddLooseGameplayTag(
				Tags.State_CommandLink_Active,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
			ActiveTagRecipients.Add(WeakActor);
		}
		ApplyActiveEffect(Actor, AbilitySystem);
		WakeParticipantForReplication(Actor, AbilitySystem);
		return;
	}

	if (ActiveTagRecipients.Remove(WeakActor) > 0)
	{
		AbilitySystem->RemoveLooseGameplayTag(
			Tags.State_CommandLink_Active,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	RemoveActiveEffect(Actor);
	if (ReplicationState.State == ESovCommandLinkState::Severed)
	{
		if (!SeveredTagRecipients.Contains(WeakActor))
		{
			AbilitySystem->AddLooseGameplayTag(
				Tags.State_CommandLink_Severed,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
			SeveredTagRecipients.Add(WeakActor);
		}
	}
	else if (SeveredTagRecipients.Remove(WeakActor) > 0)
	{
		AbilitySystem->RemoveLooseGameplayTag(
			Tags.State_CommandLink_Severed,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	WakeParticipantForReplication(Actor, AbilitySystem);
}

void USovCommandLinkComponent::RemoveParticipantContributions(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	UAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Actor);
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	if (IsValid(AbilitySystem))
	{
		const FSovGameplayTags& Tags = FSovGameplayTags::Get();
		if (ActiveTagRecipients.Remove(WeakActor) > 0)
		{
			AbilitySystem->RemoveLooseGameplayTag(
				Tags.State_CommandLink_Active,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		if (SeveredTagRecipients.Remove(WeakActor) > 0)
		{
			AbilitySystem->RemoveLooseGameplayTag(
				Tags.State_CommandLink_Severed,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
	else
	{
		ActiveTagRecipients.Remove(WeakActor);
		SeveredTagRecipients.Remove(WeakActor);
	}
	RemoveActiveEffect(Actor);
	WakeParticipantForReplication(Actor, AbilitySystem);
}

void USovCommandLinkComponent::ReconcileAllParticipants()
{
	for (AActor* Actor : BuildParticipantSnapshot())
	{
		BindParticipant(Actor);
		ReconcileParticipant(Actor);
	}
	if (IsValid(ReplicationState.CommandSource.Get()))
	{
		BindParticipant(ReplicationState.CommandSource.Get());
	}
}

void USovCommandLinkComponent::ClearAllParticipantContributions()
{
	TArray<TWeakObjectPtr<AActor>> Recipients = ActiveTagRecipients.Array();
	for (const TWeakObjectPtr<AActor>& WeakActor : SeveredTagRecipients)
	{
		Recipients.AddUnique(WeakActor);
	}
	for (const TPair<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle>& Entry :
		ActiveEffectHandles)
	{
		Recipients.AddUnique(Entry.Key);
	}
	for (const TWeakObjectPtr<AActor>& WeakActor : Recipients)
	{
		if (AActor* Actor = WeakActor.Get())
		{
			RemoveParticipantContributions(Actor);
		}
	}
	ActiveTagRecipients.Reset();
	SeveredTagRecipients.Reset();
	ActiveEffectHandles.Reset();
}

void USovCommandLinkComponent::ApplyActiveEffect(
	AActor* Actor,
	UAbilitySystemComponent* TargetAbilitySystem)
{
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	if (!ActiveLinkEffectClass || !IsValid(Actor)
		|| !IsValid(TargetAbilitySystem)
		|| ActiveEffectHandles.Contains(WeakActor))
	{
		return;
	}

	UAbilitySystemComponent* SourceAbilitySystem = ResolveAbilitySystem(
		IsValid(ReplicationState.CommandSource.Get())
			? ReplicationState.CommandSource.Get()
			: GetOwner());
	if (!IsValid(SourceAbilitySystem))
	{
		SourceAbilitySystem = TargetAbilitySystem;
	}
	FGameplayEffectContextHandle Context = SourceAbilitySystem->MakeEffectContext();
	AActor* SourceActor = IsValid(ReplicationState.CommandSource.Get())
		? ReplicationState.CommandSource.Get()
		: GetOwner();
	Context.AddInstigator(SourceActor, SourceActor);
	FGameplayEffectSpecHandle SpecHandle = SourceAbilitySystem->MakeOutgoingSpec(
		ActiveLinkEffectClass,
		1.0f,
		Context);
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		const FActiveGameplayEffectHandle AppliedHandle =
			SourceAbilitySystem->ApplyGameplayEffectSpecToTarget(
				*Spec,
				TargetAbilitySystem);
		if (AppliedHandle.IsValid())
		{
			ActiveEffectHandles.Add(WeakActor, AppliedHandle);
		}
	}
}

void USovCommandLinkComponent::RemoveActiveEffect(AActor* Actor)
{
	const TWeakObjectPtr<AActor> WeakActor(Actor);
	const FActiveGameplayEffectHandle* Handle =
		ActiveEffectHandles.Find(WeakActor);
	if (!Handle)
	{
		return;
	}
	if (UAbilitySystemComponent* AbilitySystem = ResolveAbilitySystem(Actor))
	{
		AbilitySystem->RemoveActiveGameplayEffect(*Handle);
	}
	ActiveEffectHandles.Remove(WeakActor);
}

void USovCommandLinkComponent::DeactivateWithoutSever()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (bCommandLinkMutationInProgress)
	{
		bDeferredDeactivateRequested = true;
		return;
	}
	const ESovCommandLinkState OldState = ReplicationState.State;
	bCommandLinkMutationInProgress = true;
	ReplicationState.State = ESovCommandLinkState::Inactive;
	++ReplicationState.Revision;
	ClearAllParticipantContributions();
	ReconcileAllParticipants();
	WakeOwnerForReplication();
	if (OldState != ESovCommandLinkState::Inactive)
	{
		OnCommandLinkStateChanged.Broadcast(
			OldState,
			ESovCommandLinkState::Inactive);
	}
	bCommandLinkMutationInProgress = false;
	ProcessDeferredMutation();
}

void USovCommandLinkComponent::ProcessDeferredMutation()
{
	const bool bShouldDeactivate = bDeferredDeactivateRequested;
	const bool bShouldReset = bDeferredResetRequested;
	bDeferredDeactivateRequested = false;
	bDeferredResetRequested = false;

	// Source death/destruction wins over an encounter reset requested from the
	// same synchronous callback chain.
	if (bShouldDeactivate)
	{
		DeactivateWithoutSever();
	}
	else if (bShouldReset)
	{
		ResetCommandLink();
	}
}

void USovCommandLinkComponent::WakeOwnerForReplication() const
{
	if (AActor* Owner = GetOwner(); IsValid(Owner) && Owner->HasAuthority())
	{
		Owner->FlushNetDormancy();
		Owner->ForceNetUpdate();
	}
}

void USovCommandLinkComponent::WakeParticipantForReplication(
	AActor* Actor,
	UAbilitySystemComponent* AbilitySystem) const
{
	TArray<AActor*, TInlineAllocator<2>> ReplicationOwners;
	if (IsValid(Actor))
	{
		ReplicationOwners.Add(Actor);
	}
	if (IsValid(AbilitySystem) && IsValid(AbilitySystem->GetOwnerActor()))
	{
		ReplicationOwners.AddUnique(AbilitySystem->GetOwnerActor());
	}
	for (AActor* ReplicationOwner : ReplicationOwners)
	{
		if (ReplicationOwner->HasAuthority())
		{
			ReplicationOwner->FlushNetDormancy();
			ReplicationOwner->ForceNetUpdate();
		}
	}
}

void USovCommandLinkComponent::RevealParticipantWeakPoints(
	const TArray<TObjectPtr<AActor>>& Participants,
	AActor* RevealInstigator) const
{
	for (AActor* Participant : Participants)
	{
		if (IsValid(Participant))
		{
			if (USovWeakPointComponent* WeakPoints =
				Participant->FindComponentByClass<USovWeakPointComponent>())
			{
				WeakPoints->RevealWeakPoints(
					FMath::Max(WeakPointRevealDuration, 0.0f),
					RevealInstigator);
			}
		}
	}
}

FSovCommandLinkSeverResult USovCommandLinkComponent::BuildLastSeverResult() const
{
	FSovCommandLinkSeverResult Result;
	Result.TransactionId = ReplicationState.LastSeverTransactionId;
	Result.LinkInstanceId = ReplicationState.LinkInstanceId;
	Result.LinkId = GetLinkId();
	Result.LinkOwner = GetOwner();
	Result.CommandSource = IsValid(ReplicationState.CommandSource.Get())
		? ReplicationState.CommandSource.Get()
		: GetOwner();
	Result.SeveredBy = ReplicationState.LastSeveredBy.Get();
	Result.bEligibleForEchoReward =
		ReplicationState.bLastSeverEligibleForEchoReward;
	Result.AffectedActors = ReplicationState.LastSeverAffectedActors;
	return Result;
}

void USovCommandLinkComponent::HandleParticipantDestroyed(AActor* DestroyedActor)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const bool bDestroyedCommandSource = DestroyedActor == GetOwner()
		|| DestroyedActor == ReplicationState.CommandSource.Get();
	const bool bRemovedLinkedActor = LinkedActors.Remove(DestroyedActor) > 0;
	if (bRemovedLinkedActor)
	{
		// ReconcileAllParticipants iterates a snapshot, so membership can be
		// removed here even when destruction was triggered by a synchronous tag,
		// effect, or presentation callback during a link mutation.
		WakeOwnerForReplication();
	}
	if (bCommandLinkMutationInProgress)
	{
		const TWeakObjectPtr<AActor> WeakDestroyedActor(DestroyedActor);
		ActiveTagRecipients.Remove(WeakDestroyedActor);
		SeveredTagRecipients.Remove(WeakDestroyedActor);
		ActiveEffectHandles.Remove(WeakDestroyedActor);
		if (bDestroyedCommandSource)
		{
			bDeferredDeactivateRequested = true;
		}
		return;
	}
	RemoveParticipantContributions(DestroyedActor);
	if (bDestroyedCommandSource)
	{
		DeactivateWithoutSever();
	}
}

void USovCommandLinkComponent::HandleParticipantASCInitialized()
{
	if (GetOwner() && GetOwner()->HasAuthority()
		&& !bCommandLinkMutationInProgress)
	{
		ReconcileAllParticipants();
	}
}

void USovCommandLinkComponent::HandleParticipantDeathStateChanged(
	AActor* ChangedActor,
	UNarrativeAbilitySystemComponent* ChangedActorASC,
	const bool bIsDead)
{
	static_cast<void>(ChangedActorASC);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ChangedActor))
	{
		return;
	}
	if (bCommandLinkMutationInProgress)
	{
		if (ChangedActor == GetOwner()
			|| ChangedActor == ReplicationState.CommandSource.Get())
		{
			if (bIsDead)
			{
				bDeferredDeactivateRequested = true;
			}
			else if (bStartsActive)
			{
				bDeferredResetRequested = true;
			}
		}
		return;
	}

	if (ChangedActor == GetOwner()
		|| ChangedActor == ReplicationState.CommandSource.Get())
	{
		if (bIsDead)
		{
			DeactivateWithoutSever();
		}
		else if (bStartsActive)
		{
			ResetCommandLink();
		}
		return;
	}

	if (bIsDead)
	{
		RemoveParticipantContributions(ChangedActor);
	}
	else
	{
		ReconcileParticipant(ChangedActor);
	}
}

void USovCommandLinkComponent::OnRep_ReplicationState(
	const FSovCommandLinkReplicationState& OldState)
{
	if (OldState.State != ReplicationState.State)
	{
		OnCommandLinkStateChanged.Broadcast(
			OldState.State,
			ReplicationState.State);
	}
	if (ReplicationState.State == ESovCommandLinkState::Severed
		&& ReplicationState.LastSeverTransactionId.IsValid()
		&& OldState.LastSeverTransactionId
			!= ReplicationState.LastSeverTransactionId)
	{
		const FSovCommandLinkSeverResult Result = BuildLastSeverResult();
		OnCommandLinkSevered.Broadcast(Result);
	}
}
