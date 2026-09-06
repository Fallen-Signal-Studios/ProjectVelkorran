// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_DominionHandler.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/SovDominionHandler.h"
#include "Components/SovCommandLinkComponent.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovDominionHandlerAbility, Log, All);

namespace
{
	constexpr float CommandLifecycleWatchdogMargin = 0.05f;
}

USovGameplayAbility_DominionHandlerCommandHound::
	USovGameplayAbility_DominionHandlerCommandHound()
{
	// Native ability CDOs can be constructed while dependent modules are still
	// registering their classes, before NarrativeArsenal's StartupModule has
	// populated its gameplay-tag singletons. Initialize on first use so this
	// CDO never captures invalid tags; both paths remain safe on later calls.
	if (!FNarrativeGameplayTags::Get().Narrative_Input_Ability1.IsValid())
	{
		FNarrativeGameplayTags::InitializeNativeTags();
	}
	FSovGameplayTags::InitializeNativeTags();

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
	bRequiresAmmo = false;

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	InputTag = NarrativeTags.Narrative_Input_Ability1;
	AbilityIdentityTag = SovTags.Ability_NPC_DominionHandler_CommandHound;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(AbilityIdentityTag);
	SetAssetTags(AssetTags);

	ActivationRequiredTags.AddTag(SovTags.State_CommandLink_Active);
	ActivationBlockedTags.AddTag(NarrativeTags.State_IsDead);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Busy);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Interacting);
	ActivationBlockedTags.AddTag(NarrativeTags.State_SequencerControlled);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Movement_Ragdoll);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_Equipping);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_BlockFiring);
	ActivationBlockedTags.AddTag(NarrativeTags.State_Weapon_IsFiring);
	ActivationBlockedTags.AddTag(SovTags.State_Fatal);
	ActivationBlockedTags.AddTag(SovTags.State_Poise_Broken);
	ActivationBlockedTags.AddTag(SovTags.State_Status_Frozen);
	ActivationBlockedTags.AddTag(SovTags.State_Status_DeviceDisabled);
	ActivationBlockedTags.AddTag(SovTags.State_CommandLink_Severed);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Busy);
	ActivationOwnedTags.AddTag(NarrativeTags.State_Weapon_BlockFiring);
	ActivationOwnedTags.AddTag(
		NarrativeTags.State_Movement_PostponePathUpdates);

	DefaultBotAttackFrequency = 5.5f;
	// Narrative consumes this as Handler-to-hostile engagement range. The
	// separate character setting owns Handler-to-Hound communication distance.
	DefaultBotAttackRange = 2200.0f;
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	HasActiveCommandLinkActivationRequirement() const
{
	return ActivationRequiredTags.HasTagExact(
		FSovGameplayTags::Get().State_CommandLink_Active);
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	BlocksCommandLinkSeverAtActivation() const
{
	return ActivationBlockedTags.HasTagExact(
		FSovGameplayTags::Get().State_CommandLink_Severed);
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	BlocksWeaponEquippingAtActivation() const
{
	return ActivationBlockedTags.HasTagExact(
		FNarrativeGameplayTags::Get().State_Weapon_Equipping);
}

bool USovGameplayAbility_DominionHandlerCommandHound::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(
			Handle,
			ActorInfo,
			SourceTags,
			TargetTags,
			OptionalRelevantTags))
	{
		return false;
	}

	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_Networking);
		}
		return false;
	}

	const ASovDominionHandler* HandlerAvatar =
		Cast<ASovDominionHandler>(ActorInfo->AvatarActor.Get());
	const UAbilitySystemComponent* AbilitySystem =
		ActorInfo->AbilitySystemComponent.Get();
	const USovCommandLinkComponent* CommandLink = IsValid(HandlerAvatar)
		? HandlerAvatar->GetCommandLinkComponent()
		: nullptr;
	if (!IsValid(AbilitySystem)
		|| !IsValid(HandlerAvatar)
		|| !HasRequiredCommandConfiguration()
		|| !IsValid(CommandLink)
		|| !CommandLink->IsCommandLinkActive()
		|| !CommandLink->GetLinkInstanceId().IsValid()
		|| !IsValid(HandlerAvatar->FindBestCommandableHound()))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsMissing);
		}
		return false;
	}

	const UWorld* World = GetWorld();
	if (IsValid(World)
		&& World->GetTimeSeconds() + KINDA_SMALL_NUMBER
			< NextAllowedActivationTime)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				FNarrativeGameplayTags::Get().Ability_ActivateFail_Cooldown);
		}
		return false;
	}

	return true;
}

void USovGameplayAbility_DominionHandlerCommandHound::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	const uint64 ThisActivationEpoch = AdvanceActivationEpoch();
	bIssueAttempted = false;
	bOrderIssued = false;
	bAbilityStarted = false;
	bEndingAbility = false;
	bDispatchInProgress = false;
	bDeferredEndRequested = false;
	bDeferredEndReplicate = false;
	bDeferredEndWasCancelled = false;
	CapturedLinkInstanceId.Invalidate();
	PendingHound.Reset();
	OrderedHound.Reset();
	CommandTarget.Reset();
	Handler = ActorInfo
		? Cast<ASovDominionHandler>(ActorInfo->AvatarActor.Get())
		: nullptr;
	CharacterOwner = Handler.Get();

	USovCommandLinkComponent* CommandLink = IsValid(Handler.Get())
		? Handler->GetCommandLinkComponent()
		: nullptr;
	if (!ActorInfo
		|| !ActorInfo->IsNetAuthority()
		|| !IsValid(ActorInfo->AbilitySystemComponent.Get())
		|| !HasRequiredCommandConfiguration()
		|| !IsValid(CommandLink)
		|| !CommandLink->IsCommandLinkActive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	CapturedLinkInstanceId = CommandLink->GetLinkInstanceId();
	PendingHound = Handler->FindBestCommandableHound();
	if (!CapturedLinkInstanceId.IsValid()
		|| !IsValid(PendingHound.Get()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (!IsActivationEpochCurrent(ThisActivationEpoch))
	{
		return;
	}

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	if (!IsActivationEpochCurrent(ThisActivationEpoch))
	{
		return;
	}

	BindCancellationTags(ActorInfo->AbilitySystemComponent.Get());
	if (!CanContinueCommand())
	{
		CancelCommandAbility();
		return;
	}

	bAbilityStarted = true;
	StartCommandMontage();
	if (!IsActivationEpochCurrent(ThisActivationEpoch))
	{
		return;
	}
	if (!IsValid(Handler.Get()))
	{
		CancelCommandAbility();
		return;
	}
	// Selection fairness follows visible attempts, not speculative activation
	// checks. Record immediately before the reliable anticipation cue is sent.
	Handler->RecordCommandAttempt(
		PendingHound.Get(),
		CapturedLinkInstanceId);
	Handler->MulticastPresentHoundHornChargeAnticipation(
		PendingHound.Get(),
		CapturedLinkInstanceId);
	if (!IsActivationEpochCurrent(ThisActivationEpoch))
	{
		return;
	}
	if (!IsValid(Handler.Get()))
	{
		CancelCommandAbility();
		return;
	}
	ReceiveHoundOrderStarted(PendingHound.Get(), CapturedLinkInstanceId);
	if (!IsActivationEpochCurrent(ThisActivationEpoch))
	{
		return;
	}
	if (!IsValid(Handler.Get()))
	{
		CancelCommandAbility();
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		CancelCommandAbility();
		return;
	}

	const float SafeIssueDelay = FMath::Max(CommandIssueDelay, 0.0f);
	if (SafeIssueDelay <= KINDA_SMALL_NUMBER)
	{
		HandleCommandIssueTimer(ThisActivationEpoch);
	}
	else
	{
		const FTimerDelegate IssueTimerDelegate = FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleCommandIssueTimer,
			ThisActivationEpoch);
		World->GetTimerManager().SetTimer(
			CommandIssueTimerHandle,
			IssueTimerDelegate,
			SafeIssueDelay,
			false);
	}

	if (!IsActivationEpochCurrent(ThisActivationEpoch))
	{
		return;
	}
	const FTimerDelegate WatchdogTimerDelegate = FTimerDelegate::CreateUObject(
		this,
		&ThisClass::HandleMaximumDurationExpired,
		ThisActivationEpoch);
	World->GetTimerManager().SetTimer(
		MaximumDurationTimerHandle,
		WatchdogTimerDelegate,
		FMath::Max(MaximumActiveDuration, 0.1f),
		false);
}

void USovGameplayAbility_DominionHandlerCommandHound::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	// Exact Hound activation can synchronously emit tag/death callbacks. Let the
	// dispatch return first so an accepted charge is recorded before Ended fires.
	if (bDispatchInProgress)
	{
		bDeferredEndRequested = true;
		bDeferredEndReplicate |= bReplicateEndAbility;
		bDeferredEndWasCancelled |= bWasCancelled;
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(
			this,
			&ThisClass::EndAbility,
			Handle,
			ActorInfo,
			ActivationInfo,
			bReplicateEndAbility,
			bWasCancelled));
		return;
	}

	if (bEndingAbility)
	{
		return;
	}
	bEndingAbility = true;
	AdvanceActivationEpoch();

	UnbindCancellationTags();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CommandIssueTimerHandle);
		World->GetTimerManager().ClearTimer(RecoveryTimerHandle);
		World->GetTimerManager().ClearTimer(MaximumDurationTimerHandle);
	}

	MontageTask = nullptr;
	const bool bShouldBroadcastEnd = bAbilityStarted;
	const bool bIssuedSnapshot = bOrderIssued;
	bAbilityStarted = false;
	bIssueAttempted = false;
	bOrderIssued = false;
	bDispatchInProgress = false;
	bDeferredEndRequested = false;
	bDeferredEndReplicate = false;
	bDeferredEndWasCancelled = false;
	if (bShouldBroadcastEnd)
	{
		ReceiveHoundOrderEnded(bWasCancelled, bIssuedSnapshot);
	}

	Handler.Reset();
	PendingHound.Reset();
	OrderedHound.Reset();
	CommandTarget.Reset();
	CapturedLinkInstanceId.Invalidate();

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
	bEndingAbility = false;
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	HasRequiredCommandConfiguration() const
{
	return InputTag.IsValid()
		&& AbilityIdentityTag.IsValid()
		&& bRequiresActiveCommandLink
		&& ActivationRequiredTags.HasTagExact(
			FSovGameplayTags::Get().State_CommandLink_Active)
		&& FMath::IsFinite(MontagePlayRate)
		&& MontagePlayRate > KINDA_SMALL_NUMBER
		&& FMath::IsFinite(CommandIssueDelay)
		&& CommandIssueDelay >= 0.0f
		&& FMath::IsFinite(PostIssueRecovery)
		&& PostIssueRecovery >= 0.0f
		&& FMath::IsFinite(CommandCooldownDuration)
		&& CommandCooldownDuration >= 0.0f
		&& FMath::IsFinite(MaximumActiveDuration)
		&& MaximumActiveDuration >= 0.1f
		// Timers with identical expiry times are not a gameplay ordering API.
		// Require the watchdog to trail normal recovery by a real margin.
		&& MaximumActiveDuration + KINDA_SMALL_NUMBER
			>= CommandIssueDelay
				+ PostIssueRecovery
				+ CommandLifecycleWatchdogMargin;
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	CanContinueCommand() const
{
	if (!IsActive()
		|| bEndingAbility
		|| !CurrentActorInfo
		|| !CurrentActorInfo->IsNetAuthority()
		|| !IsValid(Handler.Get())
		|| CurrentActorInfo->AvatarActor.Get() != Handler.Get()
		|| !CapturedLinkInstanceId.IsValid())
	{
		return false;
	}

	const UAbilitySystemComponent* AbilitySystem =
		CurrentActorInfo->AbilitySystemComponent.Get();
	const USovCommandLinkComponent* CommandLink =
		Handler->GetCommandLinkComponent();
	if (!IsValid(AbilitySystem)
		|| !IsValid(CommandLink)
		|| !CommandLink->IsCommandLinkActive()
		|| CommandLink->GetLinkInstanceId() != CapturedLinkInstanceId)
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	return AbilitySystem->HasMatchingGameplayTag(
			SovTags.State_CommandLink_Active)
		&& !AbilitySystem->HasMatchingGameplayTag(
			SovTags.State_CommandLink_Severed)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_IsDead)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Interacting)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_SequencerControlled)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Movement_Ragdoll)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Weapon_Equipping)
		&& !AbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Weapon_IsFiring)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Fatal)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Poise_Broken)
		&& !AbilitySystem->HasMatchingGameplayTag(SovTags.State_Status_Frozen)
		&& !AbilitySystem->HasMatchingGameplayTag(
			SovTags.State_Status_DeviceDisabled);
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	IsActivationEpochCurrent(const uint64 ExpectedEpoch) const
{
	return ExpectedEpoch != 0
		&& ExpectedEpoch == ActivationEpoch
		&& IsActive()
		&& !bEndingAbility;
}

uint64 USovGameplayAbility_DominionHandlerCommandHound::
	AdvanceActivationEpoch()
{
	++ActivationEpoch;
	if (ActivationEpoch == 0)
	{
		++ActivationEpoch;
	}
	return ActivationEpoch;
}

void USovGameplayAbility_DominionHandlerCommandHound::
	CancelCommandAbility()
{
	if (IsActive())
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			true);
	}
}

void USovGameplayAbility_DominionHandlerCommandHound::StartCommandMontage()
{
	if (!IsValid(CommandMontage.Get()))
	{
		return;
	}

	MontageTask = UAbilityTask_PlayMontageAndWait::
		CreatePlayMontageAndWaitProxy(
			this,
			TEXT("DominionHandlerCommandMontage"),
			CommandMontage.Get(),
			FMath::Max(MontagePlayRate, 0.01f),
			MontageStartSection,
			true);
	if (!IsValid(MontageTask.Get()))
	{
		UE_LOG(
			LogSovDominionHandlerAbility,
			Warning,
			TEXT("%s could not create its command montage task."),
			*GetNameSafe(this));
		return;
	}

	MontageTask->OnCompleted.AddDynamic(
		this,
		&ThisClass::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(
		this,
		&ThisClass::HandleMontageCompleted);
	MontageTask->OnInterrupted.AddDynamic(
		this,
		&ThisClass::HandleMontageInterrupted);
	MontageTask->OnCancelled.AddDynamic(
		this,
		&ThisClass::HandleMontageInterrupted);
	MontageTask->ReadyForActivation();
}

void USovGameplayAbility_DominionHandlerCommandHound::
	HandleCommandIssueTimer(const uint64 ExpectedEpoch)
{
	if (bIssueAttempted || !IsActivationEpochCurrent(ExpectedEpoch))
	{
		return;
	}
	bIssueAttempted = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CommandIssueTimerHandle);
	}

	AActor* IssuedHound = nullptr;
	AActor* IssuedTarget = nullptr;
	const TWeakObjectPtr<AActor> RequestedHound = PendingHound;
	const FGuid IssuingLinkInstanceId = CapturedLinkInstanceId;
	ASovDominionHandler* IssuingHandler = Handler.Get();
	bool bAcceptedOrder = false;
	if (CanContinueCommand() && IsValid(IssuingHandler))
	{
		bDispatchInProgress = true;
		bAcceptedOrder = IssuingHandler->TryOrderLinkedHoundHornCharge(
			IssuingLinkInstanceId,
			RequestedHound.Get(),
			IssuedHound,
			IssuedTarget)
			&& IsValid(IssuedHound);
		bDispatchInProgress = false;
	}
	if (!IsActivationEpochCurrent(ExpectedEpoch))
	{
		return;
	}
	if (!bAcceptedOrder)
	{
		if (FinishDeferredEndIfRequested())
		{
			return;
		}
		if (IsActive())
		{
			ReceiveHoundOrderFailed(
				RequestedHound.Get(),
				IssuingLinkInstanceId);
			if (IsActivationEpochCurrent(ExpectedEpoch))
			{
				CancelCommandAbility();
			}
		}
		return;
	}

	bOrderIssued = true;
	PendingHound = IssuedHound;
	OrderedHound = IssuedHound;
	CommandTarget = IssuedTarget;
	if (UWorld* World = GetWorld())
	{
		NextAllowedActivationTime = World->GetTimeSeconds()
			+ FMath::Max(CommandCooldownDuration, 0.0f);
	}
	if (bDeferredEndRequested)
	{
		if (IsValid(IssuingHandler))
		{
			IssuingHandler->MulticastPresentHoundHornChargeOrder(
				IssuedHound,
				IssuedTarget,
				IssuingLinkInstanceId);
		}
		FinishDeferredEndIfRequested();
		return;
	}

	// Commit gameplay state before presentation: a synchronous server-side
	// Blueprint response must observe this as an issued order.
	if (!IsValid(IssuingHandler))
	{
		CancelCommandAbility();
		return;
	}
	IssuingHandler->MulticastPresentHoundHornChargeOrder(
		IssuedHound,
		IssuedTarget,
		IssuingLinkInstanceId);
	if (!IsActivationEpochCurrent(ExpectedEpoch))
	{
		return;
	}

	ReceiveHoundOrderIssued(
		IssuedHound,
		IssuedTarget,
		IssuingLinkInstanceId);
	if (!IsActivationEpochCurrent(ExpectedEpoch))
	{
		return;
	}

	const float Recovery = FMath::Max(PostIssueRecovery, 0.0f);
	if (Recovery <= KINDA_SMALL_NUMBER)
	{
		HandleRecoveryFinished(ExpectedEpoch);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		const FTimerDelegate RecoveryTimerDelegate = FTimerDelegate::CreateUObject(
			this,
			&ThisClass::HandleRecoveryFinished,
			ExpectedEpoch);
		World->GetTimerManager().SetTimer(
			RecoveryTimerHandle,
			RecoveryTimerDelegate,
			Recovery,
			false);
		return;
	}

	CancelCommandAbility();
}

bool USovGameplayAbility_DominionHandlerCommandHound::
	FinishDeferredEndIfRequested()
{
	if (!bDeferredEndRequested || bDispatchInProgress)
	{
		return false;
	}

	const bool bReplicateEnd = bDeferredEndReplicate;
	const bool bWasCancelled = bDeferredEndWasCancelled;
	bDeferredEndRequested = false;
	bDeferredEndReplicate = false;
	bDeferredEndWasCancelled = false;
	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		bReplicateEnd,
		bWasCancelled);
	return true;
}

void USovGameplayAbility_DominionHandlerCommandHound::
	HandleRecoveryFinished(const uint64 ExpectedEpoch)
{
	if (IsActivationEpochCurrent(ExpectedEpoch))
	{
		EndAbility(
			CurrentSpecHandle,
			CurrentActorInfo,
			CurrentActivationInfo,
			true,
			false);
	}
}

void USovGameplayAbility_DominionHandlerCommandHound::
	HandleMaximumDurationExpired(const uint64 ExpectedEpoch)
{
	if (!IsActivationEpochCurrent(ExpectedEpoch))
	{
		return;
	}
	UE_LOG(
		LogSovDominionHandlerAbility,
		Warning,
		TEXT("%s cancelled %s after its %.2f second lifecycle watchdog expired."),
		*GetNameSafe(GetAvatarActorFromActorInfo()),
		*GetNameSafe(this),
		MaximumActiveDuration);
	CancelCommandAbility();
}

void USovGameplayAbility_DominionHandlerCommandHound::
	HandleMontageCompleted()
{
	MontageTask = nullptr;
}

void USovGameplayAbility_DominionHandlerCommandHound::
	HandleMontageInterrupted()
{
	MontageTask = nullptr;
	// Native timing owns command issue and recovery; presentation may end early.
}

void USovGameplayAbility_DominionHandlerCommandHound::
	BindCancellationTags(UAbilitySystemComponent* AbilitySystem)
{
	UnbindCancellationTags();
	if (!IsValid(AbilitySystem))
	{
		return;
	}

	BoundAbilitySystem = AbilitySystem;
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const TArray<FGameplayTag> TagsToWatch = {
		NarrativeTags.State_IsDead,
		NarrativeTags.State_Interacting,
		NarrativeTags.State_SequencerControlled,
		NarrativeTags.State_Movement_Ragdoll,
		NarrativeTags.State_Weapon_Equipping,
		NarrativeTags.State_Weapon_IsFiring,
		SovTags.State_Fatal,
		SovTags.State_Poise_Broken,
		SovTags.State_Status_Frozen,
		SovTags.State_Status_DeviceDisabled,
		SovTags.State_CommandLink_Active,
		SovTags.State_CommandLink_Severed};
	for (const FGameplayTag& Tag : TagsToWatch)
	{
		FDelegateHandle Handle = BoundAbilitySystem
			->RegisterGameplayTagEvent(
				Tag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(
				this,
				&ThisClass::HandleCancellationTagChanged);
		CancellationTagHandles.Emplace(Tag, Handle);
	}
}

void USovGameplayAbility_DominionHandlerCommandHound::
	UnbindCancellationTags()
{
	if (IsValid(BoundAbilitySystem))
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Binding :
			CancellationTagHandles)
		{
			BoundAbilitySystem
				->RegisterGameplayTagEvent(
					Binding.Key,
					EGameplayTagEventType::NewOrRemoved)
				.Remove(Binding.Value);
		}
	}

	CancellationTagHandles.Reset();
	BoundAbilitySystem = nullptr;
}

void USovGameplayAbility_DominionHandlerCommandHound::
	HandleCancellationTagChanged(
		const FGameplayTag CallbackTag,
		const int32 NewCount)
{
	const FGameplayTag ActiveLinkTag =
		FSovGameplayTags::Get().State_CommandLink_Active;
	const bool bLostRequiredActiveLink =
		CallbackTag == ActiveLinkTag && NewCount <= 0;
	const bool bGainedCancellationTag =
		CallbackTag != ActiveLinkTag && NewCount > 0;
	if ((bLostRequiredActiveLink || bGainedCancellationTag) && IsActive())
	{
		CancelCommandAbility();
	}
}
