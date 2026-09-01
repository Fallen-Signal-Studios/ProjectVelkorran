// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Weapons/SovTransformingWeaponVisual.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovTransformingWeaponVisual, Log, All);

ASovTransformingWeaponVisual::ASovTransformingWeaponVisual(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ASovTransformingWeaponVisual::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Progress = GetTransitionProgress();
	ApplyMaterialProgress(Progress);
	const bool bWaitingForAuthoritativeSnapshot =
		!HasAuthority() && !bHasReceivedAuthoritativeTransitionState;
	if (IsTransitionPhase() || bWaitingForAuthoritativeSnapshot)
	{
		// The ASC may initialize after the visual. Retry transition gating while
		// the phase is active rather than relying on one replication callback.
		SetTransitionGateActive(true);
	}

	if (HasAuthority() && (IsTransitionPhase() || bAwaitingDeathRecovery))
	{
		if (IsTransitionPhase() && IsOwnerDead())
		{
			bAwaitingDeathRecovery = true;
			ForceCompleteTransition();
		}
		else if (bAwaitingDeathRecovery && !IsOwnerDead())
		{
			RecoverFromDeathInterruption();
		}
	}
}

void ASovTransformingWeaponVisual::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
		World->GetTimerManager().ClearTimer(CollisionRefreshTimerHandle);
	}
	StopDeflectionWeaponMontageLocal(0.0f);
	StopCharacterTransitionMontages(0.0f);
	SetTransitionGateActive(false);
	Super::EndPlay(EndPlayReason);
}

void ASovTransformingWeaponVisual::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(
		ASovTransformingWeaponVisual,
		TransitionState,
		COND_None,
		REPNOTIFY_Always);
}

bool ASovTransformingWeaponVisual::HandleAttachmentRequest_Implementation(
	const FGameplayTag& EquipSlot,
	const FGameplayTag& TargetWieldSlot)
{
	if (!bEnableStagedTransitions)
	{
		return false;
	}

	const uint32 ThisRequestGeneration = ++AttachmentRequestGeneration;
	if (HasAuthority())
	{
		LatestRequestedWieldSlot = TargetWieldSlot;
	}
	if (bPhysicalAttachmentCommitInProgress)
	{
		// Existing BPHandleWield/Holster hooks may change semantic state. Queue
		// that request until the current physical handoff is fully transactional.
		QueuedEquipSlot = EquipSlot;
		QueuedTargetWieldSlot = TargetWieldSlot;
		QueuedRequestGeneration = ThisRequestGeneration;
		bHasQueuedAttachmentRequest = true;
		return true;
	}
	if (HasAuthority() && IsOwnerDead())
	{
		bAwaitingDeathRecovery = true;
		SetActorTickEnabled(true);
		return true;
	}

	const bool bTargetIsCommitted =
		AttachState.WieldedSlot == TargetWieldSlot;

	// A replicated commit still needs Narrative's low-level attach path.
	if (bTargetIsCommitted)
	{
		if (HasAuthority())
		{
			// Reversing a stow while the weapon is still physically in hand.
			if (TargetWieldSlot.IsValid()
				&& (TransitionState.Phase
						== ESovWeaponTransitionPhase::Retracting
					|| TransitionState.Phase
						== ESovWeaponTransitionPhase::Stowing))
			{
				TransitionState.TargetWieldSlot = TargetWieldSlot;
				EnterPhase(
					ESovWeaponTransitionPhase::Deploying,
					GetAnimationDuration(
						WeaponDeployAnimation,
						WeaponDeployPlayRate,
						FallbackDeployDuration),
					true);
				return true;
			}

			// Cancelling a draw before the grip handoff leaves it holstered.
			if (!TargetWieldSlot.IsValid()
				&& TransitionState.Phase
					== ESovWeaponTransitionPhase::Drawing)
			{
				TransitionState.TargetWieldSlot = FGameplayTag();
				EnterPhase(
					ESovWeaponTransitionPhase::Holstered,
					0.0f,
					true);
				return true;
			}
		}

		// A client observing a reversal waits for the server's new phase packet.
		if (!HasAuthority()
			&& IsTransitionPhase()
			&& TransitionState.TargetWieldSlot != TargetWieldSlot)
		{
			return true;
		}

		return false;
	}

	// Repeated Narrative callbacks for the same pending target are idempotent.
	if (IsTransitionPhase()
		&& TransitionState.TargetWieldSlot == TargetWieldSlot)
	{
		if (!HasAuthority()
			&& IsValid(CharacterOwner)
			&& CharacterOwner->IsLocallyControlled())
		{
			SetTransitionGateActive(true);
		}
		return true;
	}

	// Simulated clients wait for the replicated semantic phase and committed
	// AttachState. They never perform the physical handoff themselves.
	if (!HasAuthority())
	{
		if (IsValid(CharacterOwner) && CharacterOwner->IsLocallyControlled())
		{
			// WieldState can precede TransitionState on the owning client. Close
			// that prediction window immediately, then reconcile from replication.
			SetTransitionGateActive(true);
		}
		return true;
	}

	if (!TargetWieldSlot.IsValid())
	{
		return BeginStow();
	}
	if (AttachState.WieldedSlot.IsValid()
		&& AttachState.WieldedSlot != TargetWieldSlot)
	{
		return BeginWieldSlotChange(TargetWieldSlot);
	}
	return BeginDraw(TargetWieldSlot);
}

bool ASovTransformingWeaponVisual::BeginDraw(
	const FGameplayTag TargetWieldSlot)
{
	if (!HasAuthority() || !TargetWieldSlot.IsValid())
	{
		return false;
	}
	LatestRequestedWieldSlot = TargetWieldSlot;
	if (IsTransitionPhase()
		&& TransitionState.TargetWieldSlot == TargetWieldSlot)
	{
		return true;
	}
	if (AttachState.WieldedSlot.IsValid()
		|| (TransitionState.Phase != ESovWeaponTransitionPhase::Holstered
			&& TransitionState.Phase != ESovWeaponTransitionPhase::Drawing))
	{
		return false;
	}

	TransitionState.TargetWieldSlot = TargetWieldSlot;
	EnterPhase(
		ESovWeaponTransitionPhase::Drawing,
		DrawAttachmentDelay,
		true);
	return true;
}

bool ASovTransformingWeaponVisual::BeginStow()
{
	if (HasAuthority())
	{
		LatestRequestedWieldSlot = FGameplayTag();
	}
	return BeginStowInternal();
}

bool ASovTransformingWeaponVisual::BeginStowInternal()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (IsTransitionPhase()
		&& !TransitionState.TargetWieldSlot.IsValid())
	{
		return true;
	}
	if (!AttachState.WieldedSlot.IsValid())
	{
		if (TransitionState.Phase == ESovWeaponTransitionPhase::Drawing)
		{
			TransitionState.TargetWieldSlot = FGameplayTag();
			EnterPhase(
				ESovWeaponTransitionPhase::Holstered,
				0.0f,
				true);
			return true;
		}
		return TransitionState.Phase
			== ESovWeaponTransitionPhase::Holstered;
	}
	if (TransitionState.Phase != ESovWeaponTransitionPhase::Ready
		&& TransitionState.Phase != ESovWeaponTransitionPhase::Deploying)
	{
		return false;
	}

	TransitionState.TargetWieldSlot = FGameplayTag();
	EnterPhase(
		ESovWeaponTransitionPhase::Retracting,
		GetAnimationDuration(
			WeaponRetractAnimation,
			WeaponRetractPlayRate,
			FallbackRetractDuration),
		true);
	return true;
}

bool ASovTransformingWeaponVisual::BeginWieldSlotChange(
	const FGameplayTag TargetWieldSlot)
{
	if (!HasAuthority() || !TargetWieldSlot.IsValid())
	{
		return false;
	}

	LatestRequestedWieldSlot = TargetWieldSlot;
	if (!AttachState.WieldedSlot.IsValid())
	{
		return BeginDraw(TargetWieldSlot);
	}
	if (AttachState.WieldedSlot == TargetWieldSlot)
	{
		return true;
	}
	return BeginStowInternal();
}

bool ASovTransformingWeaponVisual::CommitWieldAttachment(
	const int32 ExpectedTransitionSerial)
{
	if (!HasAuthority()
		|| TransitionState.Phase != ESovWeaponTransitionPhase::Drawing
		|| !TransitionState.TargetWieldSlot.IsValid()
		|| TransitionState.TransitionSerial != ExpectedTransitionSerial
		|| bPhysicalAttachmentCommitInProgress)
	{
		return false;
	}

	bPhysicalAttachmentCommitInProgress = true;
	const bool bCommitted = CommitDeferredAttachment(
		AttachState.EquippedSlot,
		TransitionState.TargetWieldSlot);
	bPhysicalAttachmentCommitInProgress = false;
	if (!bCommitted)
	{
		bHasQueuedAttachmentRequest = false;
		QueuedRequestGeneration = 0;
		QueuedEquipSlot = FGameplayTag();
		QueuedTargetWieldSlot = FGameplayTag();
		return false;
	}

	EnterPhase(
		ESovWeaponTransitionPhase::Deploying,
		GetAnimationDuration(
			WeaponDeployAnimation,
			WeaponDeployPlayRate,
			FallbackDeployDuration),
		false);
	ProcessQueuedAttachmentRequest();
	return true;
}

bool ASovTransformingWeaponVisual::CommitHolsterAttachment(
	const int32 ExpectedTransitionSerial)
{
	if (!HasAuthority()
		|| TransitionState.Phase != ESovWeaponTransitionPhase::Stowing
		|| TransitionState.TransitionSerial != ExpectedTransitionSerial
		|| bPhysicalAttachmentCommitInProgress)
	{
		return false;
	}

	bPhysicalAttachmentCommitInProgress = true;
	const bool bCommitted = CommitDeferredAttachment(
		AttachState.EquippedSlot,
		FGameplayTag());
	bPhysicalAttachmentCommitInProgress = false;
	if (!bCommitted)
	{
		bHasQueuedAttachmentRequest = false;
		QueuedRequestGeneration = 0;
		QueuedEquipSlot = FGameplayTag();
		QueuedTargetWieldSlot = FGameplayTag();
		return false;
	}

	ProcessQueuedAttachmentRequest();
	if (IsOwnerDead())
	{
		bAwaitingDeathRecovery = true;
	}

	if (LatestRequestedWieldSlot.IsValid()
		&& !bAwaitingDeathRecovery)
	{
		TransitionState.TargetWieldSlot = LatestRequestedWieldSlot;
		EnterPhase(
			ESovWeaponTransitionPhase::Drawing,
			DrawAttachmentDelay,
			true);
	}
	else
	{
		EnterPhase(
			ESovWeaponTransitionPhase::Holstered,
			0.0f,
			false);
	}
	return true;
}

void ASovTransformingWeaponVisual::ProcessQueuedAttachmentRequest()
{
	if (!bHasQueuedAttachmentRequest)
	{
		return;
	}

	const FGameplayTag QueuedEquip = QueuedEquipSlot;
	const FGameplayTag QueuedTarget = QueuedTargetWieldSlot;
	const uint32 RequestGeneration = QueuedRequestGeneration;
	bHasQueuedAttachmentRequest = false;
	QueuedEquipSlot = FGameplayTag();
	QueuedTargetWieldSlot = FGameplayTag();
	QueuedRequestGeneration = 0;

	// TransitionSerial identifies presentation. RequestGeneration orders every
	// semantic request, including idempotent repairs that do not start a phase.
	if (AttachmentRequestGeneration == RequestGeneration)
	{
		HandleAttachmentRequest(QueuedEquip, QueuedTarget);
	}
}

void ASovTransformingWeaponVisual::OnWielded()
{
	Super::OnWielded();
	RefreshDynamicMaterials();
	ApplyTransitionState();
	if (IsWeaponReady())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(
				CollisionRefreshTimerHandle);
			CollisionRefreshTimerHandle =
				World->GetTimerManager().SetTimerForNextTick(
					this,
					&ASovTransformingWeaponVisual::RefreshReadyCollisionData);
		}
	}
	if (bHasObservedPhysicalAttachment && GetNetMode() != NM_DedicatedServer)
	{
		ReceiveWieldAttachmentCommitted();
	}
	bHasObservedPhysicalAttachment = true;
}

void ASovTransformingWeaponVisual::OnHolstered()
{
	Super::OnHolstered();
	RefreshDynamicMaterials();
	ApplyTransitionState();
	if (bHasObservedPhysicalAttachment && GetNetMode() != NM_DedicatedServer)
	{
		ReceiveHolsterAttachmentCommitted();
	}
	bHasObservedPhysicalAttachment = true;
}

bool ASovTransformingWeaponVisual::IsWeaponReady() const
{
	const bool bHasAuthoritativeState = HasAuthority()
		? TransitionState.TransitionSerial > 0
		: bHasReceivedAuthoritativeTransitionState;
	return bHasAuthoritativeState
		&& TransitionState.Phase == ESovWeaponTransitionPhase::Ready
		&& AttachState.WieldedSlot.IsValid()
		&& AttachState.WieldedSlot == TransitionState.TargetWieldSlot;
}

float ASovTransformingWeaponVisual::GetTransitionProgress() const
{
	const float PhaseAlpha = TransitionState.PhaseDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(
			GetCurrentPhaseAge() / TransitionState.PhaseDuration,
			0.0f,
			1.0f)
		: 1.0f;

	switch (TransitionState.Phase)
	{
	case ESovWeaponTransitionPhase::Deploying:
		return FMath::Lerp(
			TransitionState.PhaseStartProgress,
			1.0f,
			PhaseAlpha);
	case ESovWeaponTransitionPhase::Ready:
		return 1.0f;
	case ESovWeaponTransitionPhase::Retracting:
		return FMath::Lerp(
			TransitionState.PhaseStartProgress,
			0.0f,
			PhaseAlpha);
	case ESovWeaponTransitionPhase::Drawing:
	case ESovWeaponTransitionPhase::Stowing:
		return FMath::Clamp(
			TransitionState.PhaseStartProgress,
			0.0f,
			1.0f);
	default:
		return 0.0f;
	}
}

bool ASovTransformingWeaponVisual::PlayDeflectionWeaponMontage()
{
	if (!IsWeaponReady() || !WeaponDeflectionMontage)
	{
		return false;
	}

	if (HasAuthority())
	{
		FlushNetDormancy();
		MulticastPlayDeflectionWeaponMontage();
		return true;
	}

	const bool bPlayed = PlayDeflectionWeaponMontageLocal();
	if (IsValid(CharacterOwner) && CharacterOwner->IsLocallyControlled())
	{
		// Busy serializes Deflection attempts. This token survives a short
		// montage finishing before its authority multicast reaches the owner.
		bAwaitingAuthoritativeDeflectionMontage = bPlayed;
	}
	return bPlayed;
}

void ASovTransformingWeaponVisual::StopDeflectionWeaponMontage()
{
	const float BlendOutTime = FMath::Max(
		WeaponDeflectionMontageCancelBlendOutTime,
		0.0f);
	if (HasAuthority())
	{
		FlushNetDormancy();
		MulticastStopDeflectionWeaponMontage(BlendOutTime);
		return;
	}

	StopDeflectionWeaponMontageLocal(BlendOutTime);
}

void ASovTransformingWeaponVisual::HandleAttachedToOwner_Implementation()
{
	Super::HandleAttachedToOwner_Implementation();

	if (HasAuthority() && TransitionState.TransitionSerial == 0)
	{
		bHasReceivedAuthoritativeTransitionState = true;
		TransitionState.Phase = AttachState.WieldedSlot.IsValid()
			? ESovWeaponTransitionPhase::Ready
			: ESovWeaponTransitionPhase::Holstered;
		TransitionState.TargetWieldSlot = AttachState.WieldedSlot;
		TransitionState.ServerPhaseStartTime = GetSynchronizedServerTime();
		TransitionState.PhaseDuration = 0.0f;
		TransitionState.PhaseStartProgress =
			AttachState.WieldedSlot.IsValid() ? 1.0f : 0.0f;
		TransitionState.TransitionSerial = 1;
		LatestRequestedWieldSlot = AttachState.WieldedSlot;
		FlushNetDormancy();
		ForceNetUpdate();
	}
	else if (!HasAuthority() && TransitionState.TransitionSerial == 0)
	{
		// Safe endpoint while waiting for the replicated packet.
		TransitionState.Phase = AttachState.WieldedSlot.IsValid()
			? ESovWeaponTransitionPhase::Ready
			: ESovWeaponTransitionPhase::Holstered;
		TransitionState.TargetWieldSlot = AttachState.WieldedSlot;
		TransitionState.PhaseStartProgress =
			AttachState.WieldedSlot.IsValid() ? 1.0f : 0.0f;
	}

	ApplyTransitionState();
}

TArray<UPrimitiveComponent*>
ASovTransformingWeaponVisual::GetCollidingPrimitives_Implementation()
{
	if (!IsWeaponReady())
	{
		return {};
	}

	return Super::GetCollidingPrimitives_Implementation();
}

void ASovTransformingWeaponVisual::OnRep_TransitionState()
{
	bHasReceivedAuthoritativeTransitionState =
		TransitionState.TransitionSerial > 0;
	ApplyTransitionState();
}

void ASovTransformingWeaponVisual::EnterPhase(
	const ESovWeaponTransitionPhase NewPhase,
	const float Duration,
	const bool bBeginNewTransition)
{
	if (!HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
	}

	if (bBeginNewTransition)
	{
		++TransitionState.TransitionSerial;
	}

	const float StartProgress = FMath::Clamp(
		GetTransitionProgress(),
		0.0f,
		1.0f);
	TransitionState.Phase = NewPhase;
	TransitionState.ServerPhaseStartTime = GetSynchronizedServerTime();
	TransitionState.PhaseStartProgress = StartProgress;
	TransitionState.PhaseDuration = FMath::Max(Duration, 0.0f);
	if (NewPhase == ESovWeaponTransitionPhase::Deploying)
	{
		TransitionState.PhaseDuration *= 1.0f - StartProgress;
	}
	else if (NewPhase == ESovWeaponTransitionPhase::Retracting)
	{
		TransitionState.PhaseDuration *= StartProgress;
	}
	else if (NewPhase == ESovWeaponTransitionPhase::Ready)
	{
		TransitionState.PhaseStartProgress = 1.0f;
	}
	else if (NewPhase == ESovWeaponTransitionPhase::Holstered)
	{
		TransitionState.PhaseStartProgress = 0.0f;
	}

	ApplyTransitionState();
	FlushNetDormancy();
	ForceNetUpdate();

	if (!IsTransitionPhase())
	{
		return;
	}

	if (TransitionState.PhaseDuration <= KINDA_SMALL_NUMBER)
	{
		HandlePhaseTimerExpired();
		return;
	}

	GetWorldTimerManager().SetTimer(
		PhaseTimerHandle,
		this,
		&ASovTransformingWeaponVisual::HandlePhaseTimerExpired,
		TransitionState.PhaseDuration,
		false);
}

void ASovTransformingWeaponVisual::HandlePhaseTimerExpired()
{
	if (!HasAuthority())
	{
		return;
	}

	switch (TransitionState.Phase)
	{
	case ESovWeaponTransitionPhase::Drawing:
		if (!CommitWieldAttachment(TransitionState.TransitionSerial))
		{
			ForceCompleteTransition();
		}
		break;
	case ESovWeaponTransitionPhase::Deploying:
		EnterPhase(ESovWeaponTransitionPhase::Ready, 0.0f, false);
		break;
	case ESovWeaponTransitionPhase::Retracting:
		EnterPhase(
			ESovWeaponTransitionPhase::Stowing,
			HolsterAttachmentDelayAfterRetract,
			false);
		break;
	case ESovWeaponTransitionPhase::Stowing:
		if (!CommitHolsterAttachment(TransitionState.TransitionSerial))
		{
			ForceCompleteTransition();
		}
		break;
	default:
		break;
	}
}

void ASovTransformingWeaponVisual::ForceCompleteTransition()
{
	if (!HasAuthority())
	{
		return;
	}

	// A forced settle must never jump a corpse or invalid handoff toward a pending
	// socket. Settle presentation around the physical attachment already applied.
	TransitionState.TargetWieldSlot = AttachState.WieldedSlot;
	EnterPhase(
		AttachState.WieldedSlot.IsValid()
			? ESovWeaponTransitionPhase::Ready
			: ESovWeaponTransitionPhase::Holstered,
		0.0f,
		false);
}

void ASovTransformingWeaponVisual::RecoverFromDeathInterruption()
{
	if (!HasAuthority() || !bAwaitingDeathRecovery || IsOwnerDead())
	{
		return;
	}

	bAwaitingDeathRecovery = false;
	if (AttachState.WieldedSlot == LatestRequestedWieldSlot)
	{
		SetActorTickEnabled(IsTransitionPhase());
		return;
	}

	const bool bRecoveryStarted = LatestRequestedWieldSlot.IsValid()
		? BeginWieldSlotChange(LatestRequestedWieldSlot)
		: BeginStowInternal();
	if (!bRecoveryStarted)
	{
		UE_LOG(
			LogSovTransformingWeaponVisual,
			Warning,
			TEXT("%s could not resume its transforming weapon transition after death recovery."),
			*GetNameSafe(this));
	}
	SetActorTickEnabled(IsTransitionPhase());
}

void ASovTransformingWeaponVisual::ApplyTransitionState()
{
	// TransitionState and AttachState are separate replicated packets. Wait for
	// Narrative's owner graph before marking a phase as locally presented so a
	// late visual can still reconstruct the correct animation and gate.
	if (!AttachState.CanAttach()
		|| !IsValid(CharacterOwner)
		|| !IsValid(VisualOwner)
		|| !IsValid(WeaponMesh))
	{
		return;
	}

	// The phase and physical socket are separate replicated packets. Do not
	// present deployment on the holster (or a collapsed stable pose in hand)
	// while waiting for the matching authoritative attachment packet.
	const bool bPhaseRequiresWieldAttachment =
		TransitionState.Phase != ESovWeaponTransitionPhase::Holstered
		&& TransitionState.Phase != ESovWeaponTransitionPhase::Drawing;
	if (AttachState.WieldedSlot.IsValid() != bPhaseRequiresWieldAttachment)
	{
		return;
	}
	const bool bPhaseRequiresTargetAttachment =
		TransitionState.Phase == ESovWeaponTransitionPhase::Deploying
		|| TransitionState.Phase == ESovWeaponTransitionPhase::Ready;
	if (bPhaseRequiresTargetAttachment
		&& AttachState.WieldedSlot != TransitionState.TargetWieldSlot)
	{
		return;
	}

	const bool bTransitioning = IsTransitionPhase();
	const bool bWaitingForAuthoritativeSnapshot =
		!HasAuthority() && !bHasReceivedAuthoritativeTransitionState;
	SetTransitionGateActive(
		bTransitioning || bWaitingForAuthoritativeSnapshot);
	SetActorTickEnabled(
		bTransitioning
		|| bWaitingForAuthoritativeSnapshot
		|| (HasAuthority() && bAwaitingDeathRecovery));

	const float PhaseAge = GetCurrentPhaseAge();
	const bool bPresentationChanged =
		LastPresentedSerial != TransitionState.TransitionSerial
		|| LastPresentedPhase != TransitionState.Phase;
	const bool bHasAuthoritativePresentation =
		TransitionState.TransitionSerial > 0
		&& (HasAuthority() || bHasReceivedAuthoritativeTransitionState);

	if (bPresentationChanged)
	{
		if (TransitionState.Phase != ESovWeaponTransitionPhase::Ready)
		{
			// A physical draw/stow transition always takes priority over a
			// cosmetic Deflection spin that was still blending out.
			StopDeflectionWeaponMontageLocal(0.10f);
		}

		const bool bWasExtending =
			LastPresentedPhase == ESovWeaponTransitionPhase::Drawing
			|| LastPresentedPhase == ESovWeaponTransitionPhase::Deploying;
		const bool bWasRetracting =
			LastPresentedPhase == ESovWeaponTransitionPhase::Retracting
			|| LastPresentedPhase == ESovWeaponTransitionPhase::Stowing;
		const bool bNowExtending =
			TransitionState.Phase == ESovWeaponTransitionPhase::Drawing
			|| TransitionState.Phase == ESovWeaponTransitionPhase::Deploying;
		const bool bNowRetracting =
			TransitionState.Phase == ESovWeaponTransitionPhase::Retracting
			|| TransitionState.Phase == ESovWeaponTransitionPhase::Stowing;
		const bool bStablePhase =
			TransitionState.Phase == ESovWeaponTransitionPhase::Ready
			|| TransitionState.Phase == ESovWeaponTransitionPhase::Holstered;
		if ((bWasExtending && bNowRetracting)
			|| (bWasRetracting && bNowExtending)
			|| bStablePhase)
		{
			StopCharacterTransitionMontages();
		}

		ApplyCharacterMontage(PhaseAge);
		ApplyWeaponAnimation(PhaseAge);
		if (IsWeaponReady())
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(
					CollisionRefreshTimerHandle);
				CollisionRefreshTimerHandle =
					World->GetTimerManager().SetTimerForNextTick(
					this,
					&ASovTransformingWeaponVisual::RefreshReadyCollisionData);
			}
		}
		else
		{
			// Remove cached blade collision for every non-ready phase.
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(
					CollisionRefreshTimerHandle);
			}
			CacheCollisionData(true);
		}

		if (bHasAuthoritativePresentation)
		{
			switch (TransitionState.Phase)
			{
			case ESovWeaponTransitionPhase::Drawing:
				PlayPhaseCue(DrawCue, PhaseAge);
				break;
			case ESovWeaponTransitionPhase::Deploying:
				PlayPhaseCue(DeployCue, PhaseAge);
				break;
			case ESovWeaponTransitionPhase::Retracting:
				PlayPhaseCue(RetractCue, PhaseAge);
				break;
			case ESovWeaponTransitionPhase::Stowing:
				PlayPhaseCue(StowCue, PhaseAge);
				break;
			default:
				break;
			}

			if (GetNetMode() != NM_DedicatedServer)
			{
				ReceiveTransitionPhaseChanged(
					TransitionState.Phase,
					GetTransitionProgress(),
					TransitionState.TransitionSerial);
			}
			LastPresentedSerial = TransitionState.TransitionSerial;
			LastPresentedPhase = TransitionState.Phase;
		}
	}

	RefreshDynamicMaterials();
	ApplyMaterialProgress(GetTransitionProgress());
}

void ASovTransformingWeaponVisual::ApplyWeaponAnimation(
	const float /*PhaseAge*/)
{
	if (!bUseNativeSingleNodeWeaponAnimation)
	{
		return;
	}

	if (TransitionState.Phase == ESovWeaponTransitionPhase::Holstered
		|| TransitionState.Phase == ESovWeaponTransitionPhase::Drawing
		|| TransitionState.Phase == ESovWeaponTransitionPhase::Stowing)
	{
		ApplyStableWeaponPose(false);
		return;
	}
	if (TransitionState.Phase == ESovWeaponTransitionPhase::Ready)
	{
		ApplyStableWeaponPose(true);
		return;
	}

	UAnimSequenceBase* Animation =
		TransitionState.Phase == ESovWeaponTransitionPhase::Deploying
			? WeaponDeployAnimation.Get()
			: WeaponRetractAnimation.Get();
	const float PlayRate =
		TransitionState.Phase == ESovWeaponTransitionPhase::Deploying
			? WeaponDeployPlayRate
			: WeaponRetractPlayRate;
	if (!Animation)
	{
		return;
	}
	const float AbsoluteProgress = GetTransitionProgress();
	const float AnimationAlpha =
		TransitionState.Phase == ESovWeaponTransitionPhase::Deploying
			? AbsoluteProgress
			: 1.0f - AbsoluteProgress;

	for (USkeletalMeshComponent* Mesh : GetWeaponMeshes())
	{
		if (!IsValid(Mesh))
		{
			continue;
		}
		Mesh->PlayAnimation(Animation, false);
		Mesh->SetPlayRate(FMath::Max(PlayRate, KINDA_SMALL_NUMBER));
		Mesh->SetPosition(
			FMath::Clamp(AnimationAlpha, 0.0f, 1.0f)
				* Animation->GetPlayLength(),
			false);
	}
}

void ASovTransformingWeaponVisual::ApplyCharacterMontage(
	const float PhaseAge)
{
	UAnimMontage* MainMontage = nullptr;
	UAnimMontage* LocalMontage = nullptr;
	float PlayRate = 1.0f;
	if (TransitionState.Phase == ESovWeaponTransitionPhase::Drawing)
	{
		MainMontage = CharacterDrawMontage.Get();
		LocalMontage = LocalCharacterDrawMontage.Get()
			? LocalCharacterDrawMontage.Get()
			: MainMontage;
		PlayRate = CharacterDrawPlayRate;
	}
	else if (TransitionState.Phase == ESovWeaponTransitionPhase::Retracting)
	{
		MainMontage = CharacterStowMontage.Get();
		LocalMontage = LocalCharacterStowMontage.Get()
			? LocalCharacterStowMontage.Get()
			: MainMontage;
		PlayRate = CharacterStowPlayRate;
	}

	if (!IsValid(VisualOwner))
	{
		return;
	}

	auto PlayOnMesh = [PhaseAge, PlayRate](
		USkeletalMeshComponent* Mesh,
		UAnimMontage* Montage,
		TObjectPtr<UAnimMontage>& ActiveMontage)
	{
		if (!IsValid(Mesh) || !Montage)
		{
			return;
		}
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			if (ActiveMontage.Get() && ActiveMontage.Get() != Montage)
			{
				AnimInstance->Montage_Stop(0.10f, ActiveMontage.Get());
			}
			AnimInstance->Montage_Play(
				Montage,
				FMath::Max(PlayRate, KINDA_SMALL_NUMBER),
				EMontagePlayReturnType::MontageLength,
				FMath::Min(
					PhaseAge * FMath::Max(PlayRate, KINDA_SMALL_NUMBER),
					Montage->GetPlayLength()),
				false);
			ActiveMontage = Montage;
		}
	};

	USkeletalMeshComponent* MainMesh = VisualOwner->GetMainMesh();
	USkeletalMeshComponent* LocalMesh = VisualOwner->GetLocalMesh();
	PlayOnMesh(MainMesh, MainMontage, ActiveMainCharacterMontage);
	if (LocalMesh != MainMesh)
	{
		PlayOnMesh(LocalMesh, LocalMontage, ActiveLocalCharacterMontage);
	}
}

void ASovTransformingWeaponVisual::StopCharacterTransitionMontages(
	const float BlendOutTime)
{
	if (!IsValid(VisualOwner))
	{
		ActiveMainCharacterMontage = nullptr;
		ActiveLocalCharacterMontage = nullptr;
		return;
	}

	auto StopOnMesh = [BlendOutTime](
		USkeletalMeshComponent* Mesh,
		TObjectPtr<UAnimMontage>& ActiveMontage)
	{
		if (IsValid(Mesh) && ActiveMontage.Get())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(
					BlendOutTime,
					ActiveMontage.Get());
			}
		}
		ActiveMontage = nullptr;
	};

	USkeletalMeshComponent* MainMesh = VisualOwner->GetMainMesh();
	USkeletalMeshComponent* LocalMesh = VisualOwner->GetLocalMesh();
	StopOnMesh(MainMesh, ActiveMainCharacterMontage);
	if (LocalMesh != MainMesh)
	{
		StopOnMesh(LocalMesh, ActiveLocalCharacterMontage);
	}
}

bool ASovTransformingWeaponVisual::PlayDeflectionWeaponMontageLocal()
{
	if (GetNetMode() == NM_DedicatedServer
		|| !IsWeaponReady()
		|| !WeaponDeflectionMontage)
	{
		return false;
	}

	if (bUseNativeSingleNodeWeaponAnimation)
	{
		if (!bLoggedDeflectionMontageSetupWarning)
		{
			UE_LOG(
				LogSovTransformingWeaponVisual,
				Warning,
				TEXT("%s cannot play WeaponDeflectionMontage while native single-node weapon animation is enabled. Assign a weapon AnimBP with the montage Slot and disable bUseNativeSingleNodeWeaponAnimation."),
				*GetNameSafe(this));
			bLoggedDeflectionMontageSetupWarning = true;
		}
		return false;
	}

	const float PlayRate = FMath::Max(
		WeaponDeflectionMontagePlayRate,
		KINDA_SMALL_NUMBER);
	auto PlayOnMesh = [this, PlayRate](
		USkeletalMeshComponent* Mesh,
		UAnimMontage* Montage,
		TObjectPtr<UAnimMontage>& ActiveMontage)
	{
		if (!IsValid(Mesh) || !Montage)
		{
			return false;
		}

		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (!AnimInstance)
		{
			return false;
		}
		if (ActiveMontage.Get() && ActiveMontage.Get() != Montage)
		{
			AnimInstance->Montage_Stop(0.05f, ActiveMontage.Get());
		}

		const float Duration = AnimInstance->Montage_Play(
			Montage,
			PlayRate,
			EMontagePlayReturnType::MontageLength,
			0.0f,
			false);
		if (Duration <= 0.0f)
		{
			return false;
		}

		if (!WeaponDeflectionMontageStartSection.IsNone()
			&& Montage->GetSectionIndex(
				WeaponDeflectionMontageStartSection) != INDEX_NONE)
		{
			AnimInstance->Montage_JumpToSection(
				WeaponDeflectionMontageStartSection,
				Montage);
		}
		ActiveMontage = Montage;
		return true;
	};

	UAnimMontage* MainMontage = WeaponDeflectionMontage.Get();
	UAnimMontage* LocalMontage = LocalWeaponDeflectionMontage.Get()
		? LocalWeaponDeflectionMontage.Get()
		: MainMontage;
	const bool bPlayedMain = PlayOnMesh(
		WeaponMesh,
		MainMontage,
		ActiveMainWeaponDeflectionMontage);
	const bool bPlayedLocal = LocalWeaponMesh != WeaponMesh
		&& PlayOnMesh(
			LocalWeaponMesh,
			LocalMontage,
			ActiveLocalWeaponDeflectionMontage);
	if (!bPlayedMain && !bPlayedLocal
		&& !bLoggedDeflectionMontageSetupWarning)
	{
		UE_LOG(
			LogSovTransformingWeaponVisual,
			Warning,
			TEXT("%s could not play its Deflection montage. Verify both weapon meshes use a compatible AnimBP and that its graph contains the montage Slot."),
			*GetNameSafe(this));
		bLoggedDeflectionMontageSetupWarning = true;
	}

	return bPlayedMain || bPlayedLocal;
}

void ASovTransformingWeaponVisual::StopDeflectionWeaponMontageLocal(
	const float BlendOutTime)
{
	auto StopOnMesh = [BlendOutTime](
		USkeletalMeshComponent* Mesh,
		TObjectPtr<UAnimMontage>& ActiveMontage)
	{
		if (IsValid(Mesh) && ActiveMontage.Get())
		{
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->Montage_Stop(
					FMath::Max(BlendOutTime, 0.0f),
					ActiveMontage.Get());
			}
		}
		ActiveMontage = nullptr;
	};

	StopOnMesh(WeaponMesh, ActiveMainWeaponDeflectionMontage);
	if (LocalWeaponMesh != WeaponMesh)
	{
		StopOnMesh(
			LocalWeaponMesh,
			ActiveLocalWeaponDeflectionMontage);
	}
}

void ASovTransformingWeaponVisual::MulticastPlayDeflectionWeaponMontage_Implementation()
{
	// The autonomous proxy already played from its local-predicted ability.
	// Consume the attempt token rather than checking current playback: a short
	// predicted montage may have finished before this RPC arrives.
	if (!HasAuthority()
		&& IsValid(CharacterOwner)
		&& CharacterOwner->IsLocallyControlled()
		&& bAwaitingAuthoritativeDeflectionMontage)
	{
		bAwaitingAuthoritativeDeflectionMontage = false;
		return;
	}

	PlayDeflectionWeaponMontageLocal();
}

void ASovTransformingWeaponVisual::MulticastStopDeflectionWeaponMontage_Implementation(
	const float BlendOutTime)
{
	StopDeflectionWeaponMontageLocal(BlendOutTime);
}

void ASovTransformingWeaponVisual::ApplyStableWeaponPose(
	const bool bReadyPose)
{
	UAnimSequenceBase* StableAnimation = bReadyPose
		? WeaponReadyAnimation.Get()
		: WeaponHolsteredAnimation.Get();
	for (USkeletalMeshComponent* Mesh : GetWeaponMeshes())
	{
		if (!IsValid(Mesh))
		{
			continue;
		}
		if (StableAnimation)
		{
			Mesh->PlayAnimation(StableAnimation, true);
			Mesh->SetPlayRate(1.0f);
		}
		else
		{
			UAnimSequenceBase* EndpointAnimation = nullptr;
			float EndpointPosition = 0.0f;
			if (bReadyPose)
			{
				EndpointAnimation = WeaponDeployAnimation.Get()
					? WeaponDeployAnimation.Get()
					: WeaponRetractAnimation.Get();
				EndpointPosition = WeaponDeployAnimation.Get()
					&& EndpointAnimation
					? EndpointAnimation->GetPlayLength()
					: 0.0f;
			}
			else
			{
				EndpointAnimation = WeaponRetractAnimation.Get()
					? WeaponRetractAnimation.Get()
					: WeaponDeployAnimation.Get();
				EndpointPosition = WeaponRetractAnimation.Get()
					&& EndpointAnimation
					? EndpointAnimation->GetPlayLength()
					: 0.0f;
			}
			if (EndpointAnimation)
			{
				Mesh->PlayAnimation(EndpointAnimation, false);
				Mesh->SetPosition(EndpointPosition, false);
				Mesh->SetPlayRate(0.0f);
			}
		}
	}
}

void ASovTransformingWeaponVisual::PlayPhaseCue(
	const FSovWeaponTransitionCue& Cue,
	const float PhaseAge)
{
	if (GetNetMode() == NM_DedicatedServer
		|| PhaseAge > Cue.MaximumLatePlaybackAge)
	{
		return;
	}

	USkeletalMeshComponent* AttachMesh = GetRelevantWeaponMesh();
	if (!IsValid(AttachMesh))
	{
		AttachMesh = WeaponMesh;
	}
	if (!IsValid(AttachMesh))
	{
		return;
	}

	if (Cue.NiagaraSystem)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			Cue.NiagaraSystem.Get(),
			AttachMesh,
			Cue.AttachSocket,
			Cue.RelativeTransform.GetLocation(),
			Cue.RelativeTransform.Rotator(),
			Cue.RelativeTransform.GetScale3D(),
			EAttachLocation::KeepRelativeOffset,
			true,
			ENCPoolMethod::AutoRelease,
			true,
			true);
	}

	if (Cue.Sound)
	{
		UGameplayStatics::SpawnSoundAttached(
			Cue.Sound.Get(),
			AttachMesh,
			Cue.AttachSocket,
			Cue.RelativeTransform.GetLocation(),
			Cue.RelativeTransform.Rotator(),
			EAttachLocation::KeepRelativeOffset,
			true,
			Cue.VolumeMultiplier,
			Cue.PitchMultiplier,
			PhaseAge);
	}
}

void ASovTransformingWeaponVisual::RefreshDynamicMaterials()
{
	if (TransformationMaterialParameter.IsNone())
	{
		return;
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		TransformationMaterials.Reset();
		return;
	}

	TArray<TObjectPtr<UMaterialInstanceDynamic>> CurrentMaterials;
	for (USkeletalMeshComponent* Mesh : GetWeaponMeshes())
	{
		if (!IsValid(Mesh))
		{
			continue;
		}
		for (int32 MaterialIndex = 0;
			MaterialIndex < Mesh->GetNumMaterials();
			++MaterialIndex)
		{
			UMaterialInstanceDynamic* Material =
				Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(MaterialIndex));
			if (!Material)
			{
				Material = Mesh->CreateDynamicMaterialInstance(MaterialIndex);
			}
			if (Material)
			{
				CurrentMaterials.AddUnique(Material);
			}
		}
	}
	TransformationMaterials = MoveTemp(CurrentMaterials);
}

void ASovTransformingWeaponVisual::RefreshReadyCollisionData()
{
	if (IsWeaponReady())
	{
		CacheCollisionData(true);
	}
}

void ASovTransformingWeaponVisual::ApplyMaterialProgress(
	const float NormalizedProgress)
{
	if (TransformationMaterialParameter.IsNone())
	{
		return;
	}

	const float Value = FMath::Lerp(
		CollapsedMaterialValue,
		ExtendedMaterialValue,
		FMath::Clamp(NormalizedProgress, 0.0f, 1.0f));
	for (int32 Index = TransformationMaterials.Num() - 1;
		Index >= 0;
		--Index)
	{
		UMaterialInstanceDynamic* Material = TransformationMaterials[Index];
		if (IsValid(Material))
		{
			Material->SetScalarParameterValue(
				TransformationMaterialParameter,
				Value);
		}
		else
		{
			TransformationMaterials.RemoveAtSwap(Index);
		}
	}
}

void ASovTransformingWeaponVisual::SetTransitionGateActive(
	const bool bActive)
{
	UAbilitySystemComponent* AbilitySystem =
		TransitionGateAbilitySystem.IsValid()
			? TransitionGateAbilitySystem.Get()
			: (IsValid(CharacterOwner)
				? CharacterOwner->GetAbilitySystemComponent()
				: nullptr);
	if (!AbilitySystem)
	{
		return;
	}

	const FNarrativeGameplayTags& NarrativeTags =
		FNarrativeGameplayTags::Get();
	FGameplayTagContainer GateTags;
	GateTags.AddTag(NarrativeTags.State_Weapon_Equipping);
	GateTags.AddTag(NarrativeTags.State_Weapon_BlockFiring);

	if (HasAuthority())
	{
		if (bActive && !TransitionGateEffectHandle.IsValid())
		{
			if (UNarrativeAbilitySystemComponent* NarrativeAbilitySystem =
				Cast<UNarrativeAbilitySystemComponent>(AbilitySystem))
			{
				TransitionGateEffectHandle =
					NarrativeAbilitySystem->AddDynamicTagsGameplayEffect(
						GateTags);
			}
			if (!TransitionGateEffectHandle.IsValid()
				&& !bOwnsLocalLooseTransitionGate)
			{
				AbilitySystem->AddLooseGameplayTags(GateTags);
				bOwnsLocalLooseTransitionGate = true;
			}
			if (TransitionGateEffectHandle.IsValid()
				|| bOwnsLocalLooseTransitionGate)
			{
				TransitionGateAbilitySystem = AbilitySystem;
			}
		}
		else if (!bActive)
		{
			if (TransitionGateEffectHandle.IsValid())
			{
				AbilitySystem->RemoveActiveGameplayEffect(
					TransitionGateEffectHandle);
				TransitionGateEffectHandle = FActiveGameplayEffectHandle();
			}
			if (bOwnsLocalLooseTransitionGate)
			{
				AbilitySystem->RemoveLooseGameplayTags(GateTags);
				bOwnsLocalLooseTransitionGate = false;
			}
			TransitionGateAbilitySystem.Reset();
		}
		return;
	}

	// Replicated phase convergence closes the owning client's prediction lane.
	if (bActive && !bOwnsLocalLooseTransitionGate)
	{
		AbilitySystem->AddLooseGameplayTags(GateTags);
		bOwnsLocalLooseTransitionGate = true;
		TransitionGateAbilitySystem = AbilitySystem;
	}
	else if (!bActive && bOwnsLocalLooseTransitionGate)
	{
		AbilitySystem->RemoveLooseGameplayTags(GateTags);
		bOwnsLocalLooseTransitionGate = false;
		TransitionGateAbilitySystem.Reset();
	}
}

float ASovTransformingWeaponVisual::GetSynchronizedServerTime() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const AGameStateBase* GameState = World->GetGameState())
		{
			return GameState->GetServerWorldTimeSeconds();
		}
		return World->GetTimeSeconds();
	}
	return 0.0f;
}

float ASovTransformingWeaponVisual::GetCurrentPhaseAge() const
{
	return FMath::Max(
		GetSynchronizedServerTime() - TransitionState.ServerPhaseStartTime,
		0.0f);
}

float ASovTransformingWeaponVisual::GetAnimationDuration(
	const UAnimSequenceBase* Animation,
	const float PlayRate,
	const float FallbackDuration) const
{
	if (Animation && Animation->GetPlayLength() > KINDA_SMALL_NUMBER)
	{
		return Animation->GetPlayLength()
			/ FMath::Max(PlayRate, KINDA_SMALL_NUMBER);
	}
	return FMath::Max(FallbackDuration, 0.0f);
}

bool ASovTransformingWeaponVisual::IsTransitionPhase() const
{
	return TransitionState.Phase == ESovWeaponTransitionPhase::Drawing
		|| TransitionState.Phase == ESovWeaponTransitionPhase::Deploying
		|| TransitionState.Phase == ESovWeaponTransitionPhase::Retracting
		|| TransitionState.Phase == ESovWeaponTransitionPhase::Stowing;
}

bool ASovTransformingWeaponVisual::IsOwnerDead() const
{
	const UAbilitySystemComponent* AbilitySystem = IsValid(CharacterOwner)
		? CharacterOwner->GetAbilitySystemComponent()
		: nullptr;
	return AbilitySystem
		&& AbilitySystem->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_IsDead);
}
