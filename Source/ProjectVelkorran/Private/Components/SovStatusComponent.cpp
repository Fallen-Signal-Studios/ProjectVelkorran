// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovStatusComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SovLegacyCorruptionComponent.h"
#include "Engine/World.h"
#include "Effects/SovGameplayEffect_CinderGrenade.h"
#include "Effects/SovGameplayEffect_Status.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovStatus, Log, All);

namespace
{
	constexpr float MinimumStatusDuration = 0.01f;

	FGameplayTagContainer FilterSourceAbilityTags(
		const FGameplayTagContainer& CandidateTags)
	{
		FGameplayTagContainer Result;
		const FGameplayTag AbilityRoot = FSovGameplayTags::Get().Ability;
		for (const FGameplayTag& CandidateTag : CandidateTags)
		{
			if (CandidateTag != AbilityRoot
				&& CandidateTag.MatchesTag(AbilityRoot))
			{
				Result.AddTag(CandidateTag);
			}
		}
		return Result;
	}
}

USovStatusComponent::USovStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USovStatusComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(
		USovStatusComponent,
		ReplicatedStatusPresentation);
}

void USovStatusComponent::BeginPlay()
{
	Super::BeginPlay();

	BuildDefinitionRegistry();
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}

	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovStatusComponent::InitializeWithAbilitySystem(
	UNarrativeAbilitySystemComponent* InAbilitySystemComponent)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)
		|| !IsValid(InAbilitySystemComponent)
		|| InAbilitySystemComponent->GetAvatarActor() != OwnerActor)
	{
		return false;
	}

	if (OwnerActor->FindComponentByClass<USovStatusComponent>() != this)
	{
		UE_LOG(
			LogSovStatus,
			Error,
			TEXT("%s has more than one Status component. Ignoring duplicate %s."),
			*GetNameSafe(OwnerActor),
			*GetNameSafe(this));
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		ApplyPendingCheckpointRestore();
		return true;
	}

	UninitializeFromAbilitySystem();
	if (DefinitionRegistry.IsEmpty())
	{
		BuildDefinitionRegistry();
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	AbilitySystemComponent->OnStatusApplicationRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleStatusApplicationRequested);
	AbilitySystemComponent->OnDeathStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleDeathStateChanged);

	ApplyPendingCheckpointRestore();
	return true;
}

bool USovStatusComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent.Get())
		&& IsValid(GetOwner())
		&& AbilitySystemComponent->GetAvatarActor() == GetOwner();
}

void USovStatusComponent::TryInitializeFromOwner()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	UNarrativeAbilitySystemComponent* OwnerASC =
		Cast<UNarrativeAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor));
	if (IsValid(OwnerASC)
		&& OwnerASC->GetAvatarActor() == OwnerActor
		&& OwnerASC != AbilitySystemComponent.Get())
	{
		InitializeWithAbilitySystem(OwnerASC);
	}
}

void USovStatusComponent::UninitializeFromAbilitySystem()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredDeathCleanupTimer);
		for (TPair<FGameplayTag, FTimerHandle>& Pair : StatusExpiryTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	bDeferredDeathCleanupPending = false;
	StatusExpiryTimers.Empty();

	UNarrativeAbilitySystemComponent* PreviousASC = AbilitySystemComponent.Get();
	if (IsValid(PreviousASC))
	{
		PreviousASC->OnStatusApplicationRequested.RemoveDynamic(
			this,
			&ThisClass::HandleStatusApplicationRequested);
		PreviousASC->OnDeathStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);

		if (GetOwner() && GetOwner()->HasAuthority())
		{
			for (const TPair<FGameplayTag, FRuntimeStatusRecord>& Pair : ActiveStatuses)
			{
				if (Pair.Value.EffectHandle.IsValid())
				{
					PreviousASC->RemoveActiveGameplayEffect(Pair.Value.EffectHandle);
				}
			}
			for (const TPair<FGameplayTag, FActiveGameplayEffectHandle>& Pair :
				RecoveryImmunityHandles)
			{
				if (Pair.Value.IsValid())
				{
					PreviousASC->RemoveActiveGameplayEffect(Pair.Value);
				}
			}
		}
	}

	ActiveStatuses.Empty();
	RecoveryImmunityHandles.Empty();
	ReplayLedger.Empty();
	ReplayLedgerOrder.Empty();
	if (GetOwner()
		&& GetOwner()->HasAuthority()
		&& !ReplicatedStatusPresentation.IsEmpty())
	{
		ReplicatedStatusPresentation.Empty();
		MarkReplicatedPresentationDirty();
	}
	AbilitySystemComponent = nullptr;
}

void USovStatusComponent::BuildDefinitionRegistry()
{
	DefinitionRegistry.Empty();
	CreateBuiltInDefinitions();

	for (USovStatusDefinition* Definition : BuiltInDefinitions)
	{
		if (IsValid(Definition) && Definition->IsStructurallyValid())
		{
			DefinitionRegistry.Add(Definition->RequestTag, Definition);
		}
	}

	for (USovStatusDefinition* Definition : StatusDefinitionOverrides)
	{
		if (!IsValid(Definition) || !Definition->IsStructurallyValid())
		{
			UE_LOG(
				LogSovStatus,
				Warning,
				TEXT("%s contains an invalid Status Definition override: %s."),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Definition));
			continue;
		}

		bool bStateTagCollision = false;
		for (const TPair<FGameplayTag, TObjectPtr<USovStatusDefinition>>& Pair :
			DefinitionRegistry)
		{
			if (Pair.Key != Definition->RequestTag
				&& IsValid(Pair.Value.Get())
				&& Pair.Value->StateTag == Definition->StateTag)
			{
				bStateTagCollision = true;
				break;
			}
		}
		if (bStateTagCollision)
		{
			UE_LOG(
				LogSovStatus,
				Warning,
				TEXT("%s ignores Status Definition %s because state tag %s is already mapped by another request."),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Definition),
				*Definition->StateTag.ToString());
			continue;
		}
		DefinitionRegistry.Add(Definition->RequestTag, Definition);
	}
}

void USovStatusComponent::CreateBuiltInDefinitions()
{
	BuiltInDefinitions.Empty();
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();

	const auto MakeDefinition = [this](
		const FName ObjectName,
		const FGameplayTag& RequestTag,
		const FGameplayTag& StateTag,
		const TCHAR* DisplayName,
		const float Duration,
		const FGameplayTag& SpecificImmunity,
		const FGameplayTag& CleanseTag)
	{
		USovStatusDefinition* Definition = NewObject<USovStatusDefinition>(
			this,
			ObjectName,
			RF_Transient);
		Definition->RequestTag = RequestTag;
		Definition->StateTag = StateTag;
		Definition->DisplayName = FText::FromString(FString(DisplayName));
		Definition->PresentationTag = RequestTag;
		Definition->AccessibilityPresentationTag = StateTag;
		Definition->DurationPolicy = ESovStatusDurationPolicy::Timed;
		Definition->DefaultDuration = Duration;
		Definition->MaximumStacks = 1;
		Definition->ReapplyPolicy = ESovStatusReapplyPolicy::RefreshDuration;
		Definition->ImmunityTags.AddTag(Tags.Status_Immunity_All);
		Definition->ImmunityTags.AddTag(SpecificImmunity);
		Definition->CleanseTags.AddTag(CleanseTag);
		Definition->CheckpointBehavior =
			ESovStatusCheckpointBehavior::ClearOnCheckpoint;
		Definition->EffectClass = USovGameplayEffect_Status::StaticClass();
		BuiltInDefinitions.Add(Definition);
		return Definition;
	};

	USovStatusDefinition* Burn = MakeDefinition(
		TEXT("BuiltInStatus_Burn"),
		Tags.Status_Apply_Burn,
		Tags.State_Status_Burning,
		TEXT("Burning"),
		5.0f,
		Tags.Status_Immunity_Burn,
		Tags.Status_Cleanse_Burn);
	Burn->Period = 1.0f;
	Burn->UIPriority = 50;
	Burn->ReapplyPolicy = ESovStatusReapplyPolicy::ReplaceIfStronger;
	Burn->EffectClass = USovGameplayEffect_CinderGrenadeBurn::StaticClass();

	USovStatusDefinition* Chill = MakeDefinition(
		TEXT("BuiltInStatus_Chill"),
		Tags.Status_Apply_Chill,
		Tags.State_Status_Chilled,
		TEXT("Chilled"),
		4.0f,
		Tags.Status_Immunity_Chill,
		Tags.Status_Cleanse_Chill);
	Chill->MaximumStacks = 3;
	Chill->ReapplyPolicy = ESovStatusReapplyPolicy::AddStack;
	Chill->GrantedConstraintTags.AddTag(
		NarrativeTags.State_Movement_SlowWalking);
	Chill->UIPriority = 40;

	USovStatusDefinition* Freeze = MakeDefinition(
		TEXT("BuiltInStatus_Freeze"),
		Tags.Status_Apply_Freeze,
		Tags.State_Status_Frozen,
		TEXT("Frozen"),
		1.25f,
		Tags.Status_Immunity_Freeze,
		Tags.Status_Cleanse_Freeze);
	Freeze->ReapplyPolicy = ESovStatusReapplyPolicy::Reject;
	Freeze->bHardCrowdControl = true;
	Freeze->RecoveryImmunityTag = Tags.Status_Immunity_Freeze;
	Freeze->RecoveryImmunityDuration = 1.5f;
	Freeze->GrantedConstraintTags.AddTag(NarrativeTags.State_Busy);
	Freeze->GrantedConstraintTags.AddTag(NarrativeTags.State_Movement_Lock);
	Freeze->GrantedConstraintTags.AddTag(
		NarrativeTags.State_Weapon_BlockFiring);
	Freeze->UIPriority = 100;

	USovStatusDefinition* DeviceDisabled = MakeDefinition(
		TEXT("BuiltInStatus_DeviceDisabled"),
		Tags.Status_Apply_DeviceDisabled,
		Tags.State_Status_DeviceDisabled,
		TEXT("Device Disabled"),
		4.0f,
		Tags.Status_Immunity_DeviceDisable,
		Tags.Status_Cleanse_DeviceDisabled);
	DeviceDisabled->bRequiresDeviceEligibility = true;
	DeviceDisabled->UIPriority = 70;

	USovStatusDefinition* Exposed = MakeDefinition(
		TEXT("BuiltInStatus_Exposed"),
		Tags.Status_Apply_Exposed,
		Tags.State_Status_Exposed,
		TEXT("Exposed"),
		1.5f,
		Tags.Status_Immunity_Exposed,
		Tags.Status_Cleanse_Exposed);
	Exposed->UIPriority = 80;
}

USovStatusDefinition* USovStatusComponent::ResolveDefinition(
	const FGameplayTag StatusTag) const
{
	if (!StatusTag.IsValid())
	{
		return nullptr;
	}

	if (const TObjectPtr<USovStatusDefinition>* Exact =
		DefinitionRegistry.Find(StatusTag))
	{
		return Exact->Get();
	}

	for (const TPair<FGameplayTag, TObjectPtr<USovStatusDefinition>>& Pair :
		DefinitionRegistry)
	{
		if (IsValid(Pair.Value.Get()) && Pair.Value->StateTag == StatusTag)
		{
			return Pair.Value.Get();
		}
	}
	return nullptr;
}

FGameplayTag USovStatusComponent::ResolveRequestTag(
	const FGameplayTag StatusTag) const
{
	if (DefinitionRegistry.Contains(StatusTag))
	{
		return StatusTag;
	}
	if (USovStatusDefinition* Definition = ResolveDefinition(StatusTag))
	{
		return Definition->RequestTag;
	}
	return FGameplayTag();
}

bool USovStatusComponent::IsRequestStructurallyValid(
	const FSovStatusApplicationRequest& Request) const
{
	const AActor* OwnerActor = GetOwner();
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	return IsValid(OwnerActor)
		&& Request.RequestId.IsValid()
		&& Request.StatusTag.IsValid()
		&& Request.StatusTag != Tags.Status_Apply
		&& Request.StatusTag.MatchesTag(Tags.Status_Apply)
		&& Request.TargetActor.Get() == OwnerActor
		&& IsValid(Request.SourceActor.Get())
		&& Request.SourceActor->GetWorld() == OwnerActor->GetWorld()
		&& FMath::IsFinite(Request.Magnitude)
		&& Request.Magnitude > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(Request.Duration)
		&& Request.Duration >= 0.0f
		&& FMath::IsFinite(Request.EffectLevel)
		&& Request.EffectLevel > KINDA_SMALL_NUMBER;
}

bool USovStatusComponent::ConsumeReplayKey(
	const FGuid& RequestId,
	const FGameplayTag& StatusTag)
{
	const FReplayKey Key{RequestId, StatusTag};
	if (ReplayLedger.Contains(Key))
	{
		return false;
	}

	const int32 Capacity = FMath::Clamp(ReplayLedgerCapacity, 16, 2048);
	while (ReplayLedgerOrder.Num() >= Capacity)
	{
		const FReplayKey Oldest = ReplayLedgerOrder[0];
		ReplayLedgerOrder.RemoveAt(0, 1, EAllowShrinking::No);
		ReplayLedger.Remove(Oldest);
	}
	ReplayLedger.Add(Key);
	ReplayLedgerOrder.Add(Key);
	return true;
}

bool USovStatusComponent::IsTargetDead() const
{
	if (!IsValid(AbilitySystemComponent.Get()))
	{
		return true;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	return AbilitySystemComponent->IsDead()
		|| AbilitySystemComponent->HasMatchingGameplayTag(
			NarrativeTags.State_IsDead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Fatal);
}

bool USovStatusComponent::IsDefinitionEligible(
	const USovStatusDefinition& Definition,
	const FGameplayTagContainer& OwnedTags) const
{
	const bool bNeedsDeviceEligibility = Definition.bRequiresDeviceEligibility
		|| Definition.RequestTag
			== FSovGameplayTags::Get().Status_Apply_DeviceDisabled;
	if (bNeedsDeviceEligibility && !bDeviceStatusEligible)
	{
		return false;
	}
	if (!Definition.RequiredTargetTags.IsEmpty()
		&& !OwnedTags.HasAll(Definition.RequiredTargetTags))
	{
		return false;
	}
	return Definition.BlockedTargetTags.IsEmpty()
		|| !OwnedTags.HasAny(Definition.BlockedTargetTags);
}

ESovStatusApplicationResult USovStatusComponent::ApplyStatus(
	const FSovStatusApplicationRequest& Request)
{
	if (!IsRequestStructurallyValid(Request))
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedInvalidRequest,
			0);
		return ESovStatusApplicationResult::RejectedInvalidRequest;
	}

	AActor* OwnerActor = GetOwner();
	if (!IsInitialized()
		|| !OwnerActor->HasAuthority()
		|| AbilitySystemComponent->GetAvatarActor() != OwnerActor)
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedAuthority,
			0);
		return ESovStatusApplicationResult::RejectedAuthority;
	}

	// Structural and authority checks do not poison the replay ledger. Once a
	// valid target accepts this key for policy evaluation, every later replay is
	// rejected even if immunity, eligibility, or resource state changes.
	if (!ConsumeReplayKey(Request.RequestId, Request.StatusTag))
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedDuplicate,
			GetStatusStackCount(Request.StatusTag));
		return ESovStatusApplicationResult::RejectedDuplicate;
	}

	if (IsTargetDead())
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedDeadTarget,
			0);
		return ESovStatusApplicationResult::RejectedDeadTarget;
	}

	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (Request.StatusTag == Tags.Status_Apply_Corruption)
	{
		if (OwnedTags.HasTagExact(Tags.Status_Immunity)
			|| OwnedTags.HasTag(Tags.Status_Immunity_All)
			|| OwnedTags.HasTag(Tags.Status_Immunity_Corruption))
		{
			BroadcastApplicationResult(
				Request,
				ESovStatusApplicationResult::RejectedImmune,
				0);
			return ESovStatusApplicationResult::RejectedImmune;
		}

		USovLegacyCorruptionComponent* CorruptionComponent =
			OwnerActor->FindComponentByClass<USovLegacyCorruptionComponent>();
		if (!IsValid(CorruptionComponent))
		{
			BroadcastApplicationResult(
				Request,
				ESovStatusApplicationResult::RejectedIneligibleTarget,
				0);
			return ESovStatusApplicationResult::RejectedIneligibleTarget;
		}

		const float AppliedExposure = CorruptionComponent->ApplyInstantExposure(
			Request.Magnitude,
			Request.SourceActor.Get(),
			ESovLegacyCorruptionBand::Intrusion);
		const ESovStatusApplicationResult CorruptionResult =
			AppliedExposure > 0.0f
				? ESovStatusApplicationResult::Applied
				: ESovStatusApplicationResult::RejectedWeakerExisting;
		BroadcastApplicationResult(Request, CorruptionResult, 0);
		return CorruptionResult;
	}

	USovStatusDefinition* Definition = ResolveDefinition(Request.StatusTag);
	if (!IsValid(Definition) || !Definition->IsStructurallyValid())
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedInvalidRequest,
			0);
		return ESovStatusApplicationResult::RejectedInvalidRequest;
	}

	if (!IsDefinitionEligible(*Definition, OwnedTags))
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedIneligibleTarget,
			GetStatusStackCount(Request.StatusTag));
		return ESovStatusApplicationResult::RejectedIneligibleTarget;
	}

	if (OwnedTags.HasTagExact(Tags.Status_Immunity)
		|| OwnedTags.HasTag(Tags.Status_Immunity_All)
		|| OwnedTags.HasAny(Definition->ImmunityTags)
		|| (Definition->bHardCrowdControl
			&& OwnedTags.HasTag(Definition->RecoveryImmunityTag)))
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedImmune,
			GetStatusStackCount(Request.StatusTag));
		return ESovStatusApplicationResult::RejectedImmune;
	}

	const bool bResisted = !Definition->ResistanceTags.IsEmpty()
		&& OwnedTags.HasAny(Definition->ResistanceTags);
	float EffectiveMagnitude = Request.Magnitude;
	float EffectiveDuration = Definition->DurationPolicy
		== ESovStatusDurationPolicy::Infinite
		? 0.0f
		: (Request.Duration > KINDA_SMALL_NUMBER
			? Request.Duration
			: Definition->DefaultDuration);
	if (bResisted)
	{
		EffectiveMagnitude *= FMath::Clamp(
			Definition->ResistantMagnitudeMultiplier,
			0.0f,
			1.0f);
		if (Definition->DurationPolicy == ESovStatusDurationPolicy::Timed)
		{
			EffectiveDuration *= FMath::Clamp(
				Definition->ResistantDurationMultiplier,
				0.0f,
				1.0f);
		}
	}
	if (EffectiveMagnitude <= KINDA_SMALL_NUMBER
		|| (Definition->DurationPolicy == ESovStatusDurationPolicy::Timed
			&& EffectiveDuration < MinimumStatusDuration))
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedImmune,
			GetStatusStackCount(Request.StatusTag));
		return ESovStatusApplicationResult::RejectedImmune;
	}

	FRuntimeStatusRecord* Existing = ActiveStatuses.Find(Definition->RequestTag);
	const bool bHadExistingStatus = Existing != nullptr;
	const int32 OldStackCount = Existing ? Existing->StackCount : 0;
	int32 NewStackCount = 1;
	float NewMagnitude = EffectiveMagnitude;
	ESovStatusApplicationResult Result = ESovStatusApplicationResult::Applied;
	if (Existing)
	{
		Result = ESovStatusApplicationResult::Refreshed;
		NewStackCount = Existing->StackCount;
		NewMagnitude = FMath::Max(Existing->Magnitude, EffectiveMagnitude);
		switch (Definition->ReapplyPolicy)
		{
		case ESovStatusReapplyPolicy::Reject:
			BroadcastApplicationResult(
				Request,
				ESovStatusApplicationResult::RejectedDuplicate,
				Existing->StackCount);
			return ESovStatusApplicationResult::RejectedDuplicate;

		case ESovStatusReapplyPolicy::AddStack:
			NewStackCount = FMath::Min(
				Existing->StackCount + 1,
				FMath::Max(Definition->MaximumStacks, 1));
			break;

		case ESovStatusReapplyPolicy::ReplaceIfStronger:
			// Equal-strength applications refresh. Only a strictly weaker
			// replacement is rejected, preserving the strongest active payload.
			if (EffectiveMagnitude < Existing->Magnitude)
			{
				BroadcastApplicationResult(
					Request,
					ESovStatusApplicationResult::RejectedWeakerExisting,
					Existing->StackCount);
				return ESovStatusApplicationResult::RejectedWeakerExisting;
			}
			NewMagnitude = EffectiveMagnitude;
			break;

		case ESovStatusReapplyPolicy::RefreshDuration:
		default:
			break;
		}
	}

	const FActiveGameplayEffectHandle NewEffectHandle = ApplyDefinitionEffect(
		*Definition,
		Request,
		NewMagnitude,
		EffectiveDuration,
		NewStackCount);
	if (!NewEffectHandle.IsValid())
	{
		BroadcastApplicationResult(
			Request,
			ESovStatusApplicationResult::RejectedInvalidRequest,
			Existing ? Existing->StackCount : 0);
		return ESovStatusApplicationResult::RejectedInvalidRequest;
	}

	if (Existing)
	{
		if (FTimerHandle* ExistingTimer =
			StatusExpiryTimers.Find(Definition->RequestTag))
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(*ExistingTimer);
			}
			StatusExpiryTimers.Remove(Definition->RequestTag);
		}
		if (Existing->EffectHandle.IsValid()
			&& !(Existing->EffectHandle == NewEffectHandle))
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(
				Existing->EffectHandle);
		}
	}

	FRuntimeStatusRecord NewRecord;
	NewRecord.Definition = Definition;
	NewRecord.SourceActor = Request.SourceActor.Get();
	NewRecord.LastRequestId = Request.RequestId;
	NewRecord.SourceAbilityTags = FilterSourceAbilityTags(
		Request.SourceAbilityTags);
	NewRecord.EffectHandle = NewEffectHandle;
	NewRecord.Magnitude = NewMagnitude;
	NewRecord.AppliedDuration = EffectiveDuration;
	NewRecord.StackCount = NewStackCount;
	NewRecord.bInfinite = Definition->DurationPolicy
		== ESovStatusDurationPolicy::Infinite;
	if (!NewRecord.bInfinite)
	{
		NewRecord.EndWorldTime = GetWorld()
			? GetWorld()->GetTimeSeconds() + EffectiveDuration
			: EffectiveDuration;
	}
	ActiveStatuses.Add(Definition->RequestTag, NewRecord);
	UpsertReplicatedPresentation(*Definition, NewRecord);
	if (!NewRecord.bInfinite)
	{
		ScheduleExpiry(Definition->RequestTag, EffectiveDuration);
	}
	if (Definition->RequestTag == Tags.Status_Apply_Freeze)
	{
		if (APawn* TargetPawn = Cast<APawn>(OwnerActor))
		{
			if (UPawnMovementComponent* MovementComponent =
				TargetPawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}
			if (AController* TargetController = TargetPawn->GetController())
			{
				TargetController->StopMovement();
			}
		}
	}

	if (!bRestoringCheckpoint)
	{
		OnStatusChanged.Broadcast(
			Definition->RequestTag,
			Definition->StateTag,
			Result == ESovStatusApplicationResult::Applied
				? ESovStatusChangeReason::Applied
				: ESovStatusChangeReason::Refreshed,
			NewStackCount,
			Request.SourceActor.Get());
		SendLifecycleEvent(
			Result == ESovStatusApplicationResult::Applied
				? Tags.Event_Status_Applied
				: Tags.Event_Status_Refreshed,
			Definition->RequestTag,
			Definition->StateTag,
			Request.SourceActor.Get(),
			NewMagnitude);
			if (bHadExistingStatus
				&& Definition->ReapplyPolicy == ESovStatusReapplyPolicy::AddStack
				&& NewStackCount != OldStackCount)
		{
			SendLifecycleEvent(
				Tags.Event_Status_StackChanged,
				Definition->RequestTag,
				Definition->StateTag,
				Request.SourceActor.Get(),
				static_cast<float>(NewStackCount));
		}
	}
	BroadcastApplicationResult(Request, Result, NewStackCount);
	return Result;
}

ESovStatusApplicationResult USovStatusComponent::ApplyStatusByTag(
	const FGameplayTag StatusTag,
	AActor* SourceActor,
	const float Magnitude,
	const float Duration,
	const float EffectLevel)
{
	FSovStatusApplicationRequest Request;
	Request.RequestId = FGuid::NewGuid();
	Request.StatusTag = StatusTag;
	Request.SourceActor = SourceActor;
	Request.TargetActor = GetOwner();
	Request.Magnitude = Magnitude;
	Request.Duration = Duration;
	Request.EffectLevel = EffectLevel;
	Request.bRequiresAppliedDamage = false;

	if (IsValid(SourceActor))
	{
		UAbilitySystemComponent* ContextASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
		if (!IsValid(ContextASC) && IsInitialized())
		{
			ContextASC = AbilitySystemComponent.Get();
		}
		if (IsValid(ContextASC))
		{
			Request.Context = ContextASC->MakeEffectContext();
			Request.Context.AddInstigator(SourceActor, SourceActor);
			Request.Context.AddSourceObject(SourceActor);
		}
	}

	return ApplyStatus(Request);
}

FActiveGameplayEffectHandle USovStatusComponent::ApplyDefinitionEffect(
	const USovStatusDefinition& Definition,
	const FSovStatusApplicationRequest& Request,
	const float EffectiveMagnitude,
	const float EffectiveDuration,
	const int32 StackCount)
{
	if (!IsInitialized())
	{
		return FActiveGameplayEffectHandle();
	}

	TSubclassOf<UGameplayEffect> EffectClass = Definition.EffectClass;
	if (!EffectClass)
	{
		EffectClass = Definition.DurationPolicy == ESovStatusDurationPolicy::Infinite
			? USovGameplayEffect_StatusInfinite::StaticClass()
			: USovGameplayEffect_Status::StaticClass();
	}

	UAbilitySystemComponent* SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
			Request.SourceActor.Get());
	UAbilitySystemComponent* ApplyingASC = IsValid(SourceASC)
		? SourceASC
		: AbilitySystemComponent.Get();
	if (!IsValid(ApplyingASC))
	{
		return FActiveGameplayEffectHandle();
	}

	// The incoming damage context is audit provenance only. A periodic status
	// owns a fresh context so it cannot inherit a reused Deflection transaction,
	// hit result, or Status.Apply asset tags from the delivery effect.
	FGameplayEffectContextHandle Context = ApplyingASC->MakeEffectContext();
	Context.AddInstigator(Request.SourceActor.Get(), Request.SourceActor.Get());
	Context.AddSourceObject(Request.SourceActor.Get());

	FGameplayEffectSpecHandle SpecHandle = ApplyingASC->MakeOutgoingSpec(
		EffectClass,
		Request.EffectLevel,
		Context);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return FActiveGameplayEffectHandle();
	}

	for (const FGameplayTag& SourceAbilityTag :
		FilterSourceAbilityTags(Request.SourceAbilityTags))
	{
		Spec->AddDynamicAssetTag(SourceAbilityTag);
	}
	Spec->DynamicGrantedTags.AddTag(Definition.StateTag);
	Spec->DynamicGrantedTags.AppendTags(Definition.GrantedConstraintTags);
	Spec->SetSetByCallerMagnitude(
		FSovGameplayTags::Get().SetByCaller_Status_Magnitude,
		EffectiveMagnitude * FMath::Max(StackCount, 1));
	if (Definition.DurationPolicy == ESovStatusDurationPolicy::Timed)
	{
		Spec->SetSetByCallerMagnitude(
			FSovGameplayTags::Get().SetByCaller_Status_Duration,
			EffectiveDuration);
		Spec->SetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Duration,
			EffectiveDuration);
		Spec->SetDuration(EffectiveDuration, true);
	}

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (Definition.RequestTag == Tags.Status_Apply_Burn)
	{
		Spec->AddDynamicAssetTag(Tags.Damage_Channel_Thermal);
		Spec->AddDynamicAssetTag(Tags.Damage_BypassGuard);
		Spec->AddDynamicAssetTag(Tags.Damage_BypassDeflection);
		Spec->SetSetByCallerMagnitude(
			FNarrativeGameplayTags::Get().SetByCaller_Damage,
			EffectiveMagnitude * FMath::Max(StackCount, 1));
	}

	return ApplyingASC->ApplyGameplayEffectSpecToTarget(
		*Spec,
		AbilitySystemComponent.Get());
}

void USovStatusComponent::ScheduleExpiry(
	const FGameplayTag RequestTag,
	const float Duration)
{
	UWorld* World = GetWorld();
	if (!World || Duration < MinimumStatusDuration)
	{
		return;
	}

	FTimerHandle& Timer = StatusExpiryTimers.FindOrAdd(RequestTag);
	World->GetTimerManager().SetTimer(
		Timer,
		FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleStatusExpired,
			RequestTag),
		Duration,
		false);
}

void USovStatusComponent::HandleStatusExpired(const FGameplayTag RequestTag)
{
	RemoveStatusInternal(RequestTag, EStatusRemovalPolicy::Expired);
}

bool USovStatusComponent::RemoveStatusInternal(
	const FGameplayTag RequestTag,
	const EStatusRemovalPolicy RemovalPolicy,
	const bool bForceRecoveryImmunity)
{
	FRuntimeStatusRecord Record;
	if (!ActiveStatuses.RemoveAndCopyValue(RequestTag, Record))
	{
		return false;
	}

	if (FTimerHandle* Timer = StatusExpiryTimers.Find(RequestTag))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*Timer);
		}
		StatusExpiryTimers.Remove(RequestTag);
	}

	if (IsValid(AbilitySystemComponent.Get()) && Record.EffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(Record.EffectHandle);
	}
	RemoveReplicatedPresentation(RequestTag);

	USovStatusDefinition* Definition = Record.Definition.Get();
	const bool bNaturalRecovery =
		RemovalPolicy == EStatusRemovalPolicy::Expired
		|| RemovalPolicy == EStatusRemovalPolicy::Cleansed;
	if (IsValid(Definition) && (bNaturalRecovery || bForceRecoveryImmunity))
	{
		ApplyRecoveryImmunity(*Definition);
	}

	if (!bRestoringCheckpoint && IsValid(Definition))
	{
		ESovStatusChangeReason ChangeReason = ESovStatusChangeReason::Removed;
		FGameplayTag EventTag = FSovGameplayTags::Get().Event_Status_Removed;
		if (RemovalPolicy == EStatusRemovalPolicy::Expired)
		{
			ChangeReason = ESovStatusChangeReason::Expired;
		}
		else if (RemovalPolicy == EStatusRemovalPolicy::Cleansed)
		{
			ChangeReason = ESovStatusChangeReason::Cleansed;
			EventTag = FSovGameplayTags::Get().Event_Status_Cleansed;
		}
		OnStatusChanged.Broadcast(
			Definition->RequestTag,
			Definition->StateTag,
			ChangeReason,
			0,
			Record.SourceActor.Get());
		SendLifecycleEvent(
			EventTag,
			Definition->RequestTag,
			Definition->StateTag,
			Record.SourceActor.Get(),
			Record.Magnitude);
	}
	return true;
}

bool USovStatusComponent::RemoveStatus(
	const FGameplayTag StatusTag,
	const bool bApplyRecoveryImmunity)
{
	if (!IsInitialized() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	const FGameplayTag RequestTag = ResolveRequestTag(StatusTag);
	return RequestTag.IsValid()
		&& RemoveStatusInternal(
			RequestTag,
			EStatusRemovalPolicy::Explicit,
			bApplyRecoveryImmunity);
}

int32 USovStatusComponent::CleanseStatuses(
	const FGameplayTag CleanseTag,
	AActor* SourceActorFilter)
{
	if (!IsInitialized()
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !CleanseTag.IsValid())
	{
		return 0;
	}

	const bool bCleanseAll =
		CleanseTag == FSovGameplayTags::Get().Status_Cleanse_All;
	TArray<FGameplayTag> ToRemove;
	for (const TPair<FGameplayTag, FRuntimeStatusRecord>& Pair : ActiveStatuses)
	{
		const USovStatusDefinition* Definition = Pair.Value.Definition.Get();
		if (!IsValid(Definition)
			|| Definition->CleanseTags.IsEmpty()
			|| (IsValid(SourceActorFilter)
				&& Pair.Value.SourceActor.Get() != SourceActorFilter))
		{
			continue;
		}
		if (bCleanseAll || Definition->CleanseTags.HasTagExact(CleanseTag))
		{
			ToRemove.Add(Pair.Key);
		}
	}

	for (const FGameplayTag& RequestTag : ToRemove)
	{
		RemoveStatusInternal(RequestTag, EStatusRemovalPolicy::Cleansed);
	}
	return ToRemove.Num();
}

void USovStatusComponent::ClearAllTrackedEffects(
	const EStatusRemovalPolicy RemovalPolicy)
{
	TArray<FGameplayTag> StatusTags;
	ActiveStatuses.GenerateKeyArray(StatusTags);
	for (const FGameplayTag& StatusTag : StatusTags)
	{
		RemoveStatusInternal(StatusTag, RemovalPolicy);
	}

	// Recovery immunities are component-owned runtime state, not semantic
	// checkpoint data. Death and restore must not leak them into the next state.
	if (IsValid(AbilitySystemComponent.Get()))
	{
		for (const TPair<FGameplayTag, FActiveGameplayEffectHandle>& Pair :
			RecoveryImmunityHandles)
		{
			if (Pair.Value.IsValid())
			{
				AbilitySystemComponent->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}
	RecoveryImmunityHandles.Empty();
}

void USovStatusComponent::ApplyRecoveryImmunity(
	const USovStatusDefinition& Definition)
{
	if (!Definition.bHardCrowdControl
		|| !Definition.RecoveryImmunityTag.IsValid()
		|| Definition.RecoveryImmunityDuration <= KINDA_SMALL_NUMBER
		|| !IsInitialized()
		|| IsTargetDead())
	{
		return;
	}

	if (const FActiveGameplayEffectHandle* Existing =
		RecoveryImmunityHandles.Find(Definition.RecoveryImmunityTag))
	{
		if (Existing->IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(*Existing);
		}
		RecoveryImmunityHandles.Remove(Definition.RecoveryImmunityTag);
	}

	FGameplayEffectContextHandle Context =
		AbilitySystemComponent->MakeEffectContext();
	Context.AddSourceObject(GetOwner());
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		USovGameplayEffect_Status::StaticClass(),
		1.0f,
		Context);
	FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
	if (!Spec)
	{
		return;
	}

	Spec->DynamicGrantedTags.AddTag(Definition.RecoveryImmunityTag);
	Spec->SetSetByCallerMagnitude(
		FSovGameplayTags::Get().SetByCaller_Status_Duration,
		Definition.RecoveryImmunityDuration);
	Spec->SetDuration(Definition.RecoveryImmunityDuration, true);
	const FActiveGameplayEffectHandle Handle =
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
	if (Handle.IsValid())
	{
		RecoveryImmunityHandles.Add(
			Definition.RecoveryImmunityTag,
			Handle);
	}
}

bool USovStatusComponent::HasActiveStatus(const FGameplayTag StatusTag) const
{
	const FGameplayTag RequestTag = ResolveRequestTag(StatusTag);
	if (RequestTag.IsValid() && ActiveStatuses.Contains(RequestTag))
	{
		return true;
	}
	if (FindReplicatedPresentation(StatusTag))
	{
		return true;
	}

	const USovStatusDefinition* Definition = ResolveDefinition(StatusTag);
	return IsValid(Definition)
		&& IsInitialized()
		&& AbilitySystemComponent->HasMatchingGameplayTag(Definition->StateTag);
}

bool USovStatusComponent::WasStatusAppliedBy(
	const FGameplayTag StatusTag,
	const AActor* SourceActor) const
{
	if (!IsValid(SourceActor))
	{
		return false;
	}
	const FGameplayTag RequestTag = ResolveRequestTag(StatusTag);
	const FRuntimeStatusRecord* Record = ActiveStatuses.Find(RequestTag);
	return Record && Record->SourceActor.Get() == SourceActor;
}

int32 USovStatusComponent::GetStatusStackCount(
	const FGameplayTag StatusTag) const
{
	const FGameplayTag RequestTag = ResolveRequestTag(StatusTag);
	if (const FRuntimeStatusRecord* Record = ActiveStatuses.Find(RequestTag))
	{
		return Record->StackCount;
	}
	if (const FSovStatusPresentationEntry* Presentation =
		FindReplicatedPresentation(StatusTag))
	{
		return Presentation->StackCount;
	}
	return 0;
}

float USovStatusComponent::GetStatusRemainingDuration(
	const FGameplayTag StatusTag) const
{
	const FGameplayTag RequestTag = ResolveRequestTag(StatusTag);
	const FRuntimeStatusRecord* Record = ActiveStatuses.Find(RequestTag);
	if (Record)
	{
		if (Record->bInfinite)
		{
			return -1.0f;
		}
		return FMath::Max(
			Record->EndWorldTime
				- (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f),
			0.0f);
	}

	const FSovStatusPresentationEntry* Presentation =
		FindReplicatedPresentation(StatusTag);
	if (!Presentation)
	{
		return 0.0f;
	}
	if (Presentation->bInfinite)
	{
		return -1.0f;
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const float ServerWorldTime = GameState
		? GameState->GetServerWorldTimeSeconds()
		: (World ? World->GetTimeSeconds() : 0.0f);
	return FMath::Max(
		Presentation->ServerWorldExpiry - ServerWorldTime,
		0.0f);
}

USovStatusDefinition* USovStatusComponent::GetStatusDefinition(
	const FGameplayTag StatusTag) const
{
	return ResolveDefinition(StatusTag);
}

void USovStatusComponent::UpsertReplicatedPresentation(
	const USovStatusDefinition& Definition,
	const FRuntimeStatusRecord& RuntimeRecord)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	FSovStatusPresentationEntry NewEntry;
	NewEntry.RequestTag = Definition.RequestTag;
	NewEntry.StateTag = Definition.StateTag;
	NewEntry.StackCount = RuntimeRecord.StackCount;
	NewEntry.ServerWorldExpiry = RuntimeRecord.EndWorldTime;
	NewEntry.bInfinite = RuntimeRecord.bInfinite;
	NewEntry.UIPriority = Definition.UIPriority;
	NewEntry.PresentationTag = Definition.PresentationTag;
	NewEntry.AccessibilityPresentationTag =
		Definition.AccessibilityPresentationTag;

	const int32 ExistingIndex = ReplicatedStatusPresentation.IndexOfByPredicate(
		[&NewEntry](const FSovStatusPresentationEntry& Entry)
		{
			return Entry.RequestTag == NewEntry.RequestTag;
		});
	if (ExistingIndex != INDEX_NONE
		&& ArePresentationEntriesEqual(
			ReplicatedStatusPresentation[ExistingIndex],
			NewEntry))
	{
		return;
	}
	if (ExistingIndex == INDEX_NONE)
	{
		ReplicatedStatusPresentation.Add(NewEntry);
	}
	else
	{
		ReplicatedStatusPresentation[ExistingIndex] = NewEntry;
	}

	ReplicatedStatusPresentation.Sort([](
		const FSovStatusPresentationEntry& Left,
		const FSovStatusPresentationEntry& Right)
	{
		if (Left.UIPriority != Right.UIPriority)
		{
			return Left.UIPriority > Right.UIPriority;
		}
		return Left.RequestTag.ToString() < Right.RequestTag.ToString();
	});
	MarkReplicatedPresentationDirty();
}

void USovStatusComponent::RemoveReplicatedPresentation(
	const FGameplayTag RequestTag)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	const int32 RemovedCount = ReplicatedStatusPresentation.RemoveAll(
		[RequestTag](const FSovStatusPresentationEntry& Entry)
		{
			return Entry.RequestTag == RequestTag;
		});
	if (RemovedCount > 0)
	{
		MarkReplicatedPresentationDirty();
	}
}

void USovStatusComponent::MarkReplicatedPresentationDirty()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| !OwnerActor->GetIsReplicated())
	{
		return;
	}
	OwnerActor->FlushNetDormancy();
	OwnerActor->ForceNetUpdate();
}

const FSovStatusPresentationEntry*
USovStatusComponent::FindReplicatedPresentation(
	const FGameplayTag StatusTag) const
{
	if (!StatusTag.IsValid())
	{
		return nullptr;
	}
	return ReplicatedStatusPresentation.FindByPredicate(
		[StatusTag](const FSovStatusPresentationEntry& Entry)
		{
			return Entry.RequestTag == StatusTag
				|| Entry.StateTag == StatusTag;
		});
}

bool USovStatusComponent::ArePresentationEntriesEqual(
	const FSovStatusPresentationEntry& Left,
	const FSovStatusPresentationEntry& Right)
{
	return Left.RequestTag == Right.RequestTag
		&& Left.StateTag == Right.StateTag
		&& Left.StackCount == Right.StackCount
		&& FMath::IsNearlyEqual(
			Left.ServerWorldExpiry,
			Right.ServerWorldExpiry)
		&& Left.bInfinite == Right.bInfinite
		&& Left.UIPriority == Right.UIPriority
		&& Left.PresentationTag == Right.PresentationTag
		&& Left.AccessibilityPresentationTag
			== Right.AccessibilityPresentationTag;
}

void USovStatusComponent::OnRep_ReplicatedStatusPresentation()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		return;
	}

	TMap<FGameplayTag, FSovStatusPresentationEntry> NewObservedPresentation;
	for (const FSovStatusPresentationEntry& Entry :
		ReplicatedStatusPresentation)
	{
		if (!Entry.RequestTag.IsValid() || !Entry.StateTag.IsValid())
		{
			continue;
		}
		NewObservedPresentation.Add(Entry.RequestTag, Entry);

		const FSovStatusPresentationEntry* PreviousEntry =
			LastObservedPresentation.Find(Entry.RequestTag);
		if (!PreviousEntry)
		{
			OnStatusChanged.Broadcast(
				Entry.RequestTag,
				Entry.StateTag,
				ESovStatusChangeReason::Applied,
				Entry.StackCount,
				nullptr);
		}
		else if (!ArePresentationEntriesEqual(*PreviousEntry, Entry))
		{
			OnStatusChanged.Broadcast(
				Entry.RequestTag,
				Entry.StateTag,
				ESovStatusChangeReason::Refreshed,
				Entry.StackCount,
				nullptr);
		}
	}

	TArray<FGameplayTag> RemovedRequestTags;
	for (const TPair<FGameplayTag, FSovStatusPresentationEntry>& Pair :
		LastObservedPresentation)
	{
		if (!NewObservedPresentation.Contains(Pair.Key))
		{
			RemovedRequestTags.Add(Pair.Key);
		}
	}
	RemovedRequestTags.Sort([](
		const FGameplayTag& Left,
		const FGameplayTag& Right)
	{
		return Left.ToString() < Right.ToString();
	});
	for (const FGameplayTag& RemovedRequestTag : RemovedRequestTags)
	{
		const FSovStatusPresentationEntry* RemovedEntry =
			LastObservedPresentation.Find(RemovedRequestTag);
		if (RemovedEntry)
		{
			OnStatusChanged.Broadcast(
				RemovedEntry->RequestTag,
				RemovedEntry->StateTag,
				ESovStatusChangeReason::Removed,
				0,
				nullptr);
		}
	}
	LastObservedPresentation = MoveTemp(NewObservedPresentation);
}

FSovStatusCheckpointState USovStatusComponent::CaptureCheckpointState() const
{
	FSovStatusCheckpointState State;
	State.SchemaVersion = CheckpointSchemaVersion;
	const float WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	for (const TPair<FGameplayTag, FRuntimeStatusRecord>& Pair : ActiveStatuses)
	{
		const USovStatusDefinition* Definition = Pair.Value.Definition.Get();
		if (!IsValid(Definition)
			|| Definition->CheckpointBehavior
				== ESovStatusCheckpointBehavior::ClearOnCheckpoint)
		{
			continue;
		}

		FSovStatusCheckpointRecord Record;
		Record.DefinitionId = Definition->GetPrimaryAssetId();
		Record.RequestTag = Definition->RequestTag;
		Record.StackCount = FMath::Clamp(
			Pair.Value.StackCount,
			1,
			FMath::Max(Definition->MaximumStacks, 1));
		Record.Magnitude = FMath::Max(Pair.Value.Magnitude, 0.0f);
		Record.SourceAbilityTags = Pair.Value.SourceAbilityTags;
		Record.bInfinite = Pair.Value.bInfinite;
		if (!Record.bInfinite)
		{
			Record.RemainingDuration = Definition->CheckpointBehavior
				== ESovStatusCheckpointBehavior::PersistFullDuration
				? Pair.Value.AppliedDuration
				: FMath::Max(Pair.Value.EndWorldTime - WorldTime, 0.0f);
			if (Record.RemainingDuration < MinimumStatusDuration)
			{
				continue;
			}
		}
		State.Statuses.Add(Record);
	}

	State.Statuses.Sort([](
		const FSovStatusCheckpointRecord& Left,
		const FSovStatusCheckpointRecord& Right)
	{
		return Left.RequestTag.ToString() < Right.RequestTag.ToString();
	});
	return State;
}

bool USovStatusComponent::RestoreCheckpointState(
	const FSovStatusCheckpointState& State)
{
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| State.SchemaVersion != CheckpointSchemaVersion)
	{
		return false;
	}

	if (!IsInitialized() || IsTargetDead() || bDeferredDeathCleanupPending)
	{
		PendingCheckpointState = State;
		bHasPendingCheckpointState = true;
		return true;
	}
	return ApplyCheckpointStateNow(State);
}

void USovStatusComponent::ApplyPendingCheckpointRestore()
{
	if (!bHasPendingCheckpointState
		|| !IsInitialized()
		|| IsTargetDead()
		|| bDeferredDeathCleanupPending)
	{
		return;
	}

	const FSovStatusCheckpointState State = PendingCheckpointState;
	bHasPendingCheckpointState = false;
	PendingCheckpointState = FSovStatusCheckpointState();
	ApplyCheckpointStateNow(State);
}

bool USovStatusComponent::ApplyCheckpointStateNow(
	const FSovStatusCheckpointState& State)
{
	if (!IsInitialized()
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| State.SchemaVersion != CheckpointSchemaVersion)
	{
		return false;
	}

	bRestoringCheckpoint = true;
	ClearAllTrackedEffects(EStatusRemovalPolicy::CheckpointRestore);
	ReplayLedger.Empty();
	ReplayLedgerOrder.Empty();

	TArray<FGameplayTag> RestoredTags;
	for (const FSovStatusCheckpointRecord& SavedRecord : State.Statuses)
	{
		USovStatusDefinition* Definition = ResolveDefinition(
			SavedRecord.RequestTag);
		if (!IsValid(Definition)
			|| !Definition->IsStructurallyValid()
			|| Definition->CheckpointBehavior
				== ESovStatusCheckpointBehavior::ClearOnCheckpoint
			|| SavedRecord.StackCount <= 0
			|| !FMath::IsFinite(SavedRecord.Magnitude)
			|| SavedRecord.Magnitude <= KINDA_SMALL_NUMBER
			|| (Definition->DurationPolicy == ESovStatusDurationPolicy::Timed
				&& (!FMath::IsFinite(SavedRecord.RemainingDuration)
					|| SavedRecord.RemainingDuration < MinimumStatusDuration)))
		{
			continue;
		}

		FSovStatusApplicationRequest Request;
		Request.RequestId = FGuid::NewGuid();
		Request.StatusTag = Definition->RequestTag;
		Request.SourceActor = GetOwner();
		Request.TargetActor = GetOwner();
		Request.Magnitude = SavedRecord.Magnitude;
		Request.SourceAbilityTags = SavedRecord.SourceAbilityTags;
		Request.Duration = Definition->DurationPolicy
			== ESovStatusDurationPolicy::Timed
			? SavedRecord.RemainingDuration
			: 0.0f;
		Request.EffectLevel = 1.0f;
		Request.Context = AbilitySystemComponent->MakeEffectContext();

		const ESovStatusApplicationResult Result = ApplyStatus(Request);
		if (Result != ESovStatusApplicationResult::Applied
			&& Result != ESovStatusApplicationResult::Refreshed)
		{
			continue;
		}

		if (FRuntimeStatusRecord* RuntimeRecord =
			ActiveStatuses.Find(Definition->RequestTag))
		{
			const int32 RestoredStackCount = FMath::Clamp(
				SavedRecord.StackCount,
				1,
				FMath::Max(Definition->MaximumStacks, 1));
			if (RestoredStackCount != RuntimeRecord->StackCount)
			{
				const FActiveGameplayEffectHandle RestoredEffectHandle =
					ApplyDefinitionEffect(
						*Definition,
						Request,
						SavedRecord.Magnitude,
						Request.Duration,
						RestoredStackCount);
				if (RestoredEffectHandle.IsValid())
				{
					if (RuntimeRecord->EffectHandle.IsValid()
						&& !(RuntimeRecord->EffectHandle == RestoredEffectHandle))
					{
						AbilitySystemComponent->RemoveActiveGameplayEffect(
							RuntimeRecord->EffectHandle);
					}
					RuntimeRecord->EffectHandle = RestoredEffectHandle;
					RuntimeRecord->StackCount = RestoredStackCount;
				}
			}
			// Checkpoints deliberately serialize no raw actor identity. Story
			// attribution belongs to the consequence system, not a transient GE.
			RuntimeRecord->SourceActor.Reset();
			UpsertReplicatedPresentation(*Definition, *RuntimeRecord);
			RestoredTags.Add(Definition->RequestTag);
		}
	}
	bRestoringCheckpoint = false;

	for (const FGameplayTag& RequestTag : RestoredTags)
	{
		if (const FRuntimeStatusRecord* RuntimeRecord =
			ActiveStatuses.Find(RequestTag))
		{
			if (USovStatusDefinition* Definition =
				RuntimeRecord->Definition.Get())
			{
				OnStatusChanged.Broadcast(
					Definition->RequestTag,
					Definition->StateTag,
					ESovStatusChangeReason::Restored,
					RuntimeRecord->StackCount,
					nullptr);
			}
		}
	}
	return true;
}

void USovStatusComponent::BroadcastApplicationResult(
	const FSovStatusApplicationRequest& Request,
	const ESovStatusApplicationResult Result,
	const int32 NewStackCount)
{
	if (bRestoringCheckpoint)
	{
		return;
	}
	OnStatusApplicationResolved.Broadcast(Request, Result, NewStackCount);
	if (Result != ESovStatusApplicationResult::Applied
		&& Result != ESovStatusApplicationResult::Refreshed)
	{
		SendLifecycleEvent(
			FSovGameplayTags::Get().Event_Status_Rejected,
			Request.StatusTag,
			ResolveDefinition(Request.StatusTag)
				? ResolveDefinition(Request.StatusTag)->StateTag
				: FGameplayTag(),
			Request.SourceActor.Get(),
			static_cast<float>(Result));
	}
}

void USovStatusComponent::SendLifecycleEvent(
	const FGameplayTag EventTag,
	const FGameplayTag RequestTag,
	const FGameplayTag StateTag,
	AActor* SourceActor,
	const float Magnitude) const
{
	if (!EventTag.IsValid() || !IsValid(GetOwner()))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = SourceActor;
	Payload.Target = GetOwner();
	Payload.EventMagnitude = Magnitude;
	if (RequestTag.IsValid())
	{
		Payload.TargetTags.AddTag(RequestTag);
	}
	if (StateTag.IsValid())
	{
		Payload.TargetTags.AddTag(StateTag);
	}
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwner(),
		EventTag,
		Payload);
}

void USovStatusComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovStatusComponent::HandleStatusApplicationRequested(
	const FSovStatusApplicationRequest& Request)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)
		|| !OwnerActor->HasAuthority()
		|| Request.TargetActor.Get() != OwnerActor
		|| !IsValid(AbilitySystemComponent.Get())
		|| AbilitySystemComponent->GetAvatarActor() != OwnerActor)
	{
		return;
	}

	// ApplyStatus forwards Corruption into its dedicated component rather than
	// fabricating an ordinary transient status Gameplay Effect.
	ApplyStatus(Request);
}

void USovStatusComponent::HandleDeathStateChanged(
	AActor* KilledActor,
	UNarrativeAbilitySystemComponent* KilledActorASC,
	const bool bIsDead)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)
		|| !IsValid(KilledActorASC)
		|| KilledActor != OwnerActor
		|| KilledActorASC != AbilitySystemComponent.Get()
		|| KilledActorASC->GetAvatarActor() != OwnerActor
		|| !OwnerActor->HasAuthority())
	{
		return;
	}
	if (!bIsDead)
	{
		// If revival happens in the fatal transaction's frame, the mandated
		// deferred cleanup must run before restored semantic state is reapplied.
		if (!bDeferredDeathCleanupPending)
		{
			ApplyPendingCheckpointRestore();
		}
		return;
	}

	// Damage source callbacks run after the death delegate. Preserve Exposed
	// provenance until the fatal transaction can award Selene's +6 Echo, then
	// remove only component-owned state on the following game-thread tick.
	if (UWorld* World = GetWorld())
	{
		bDeferredDeathCleanupPending = true;
		World->GetTimerManager().ClearTimer(DeferredDeathCleanupTimer);
		DeferredDeathCleanupTimer = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::HandleDeferredDeathCleanup));
	}
}

void USovStatusComponent::HandleDeferredDeathCleanup()
{
	bDeferredDeathCleanupPending = false;
	if (!IsInitialized()
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| AbilitySystemComponent->GetAvatarActor() != GetOwner())
	{
		return;
	}
	ClearAllTrackedEffects(EStatusRemovalPolicy::Death);
	ApplyPendingCheckpointRestore();
}
