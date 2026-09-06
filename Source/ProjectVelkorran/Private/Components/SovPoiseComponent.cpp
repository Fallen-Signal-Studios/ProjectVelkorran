// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovPoiseComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovPoise, Log, All);

USovPoiseComponent::USovPoiseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void USovPoiseComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovPoiseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	ClearLifecycleTimers();
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovPoiseComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (bEndingPlay || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| !IsValid(InAbilitySystemComponent) || InAbilitySystemComponent->GetAvatarActor() != GetOwner())
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent && IsInitialized()
		&& PoiseChangedDelegateHandle.IsValid()
		&& MaxPoiseChangedDelegateHandle.IsValid())
	{
		return true;
	}

	if (!InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		if (!bWarnedMissingAttributeSet)
		{
			UE_LOG(
				LogSovPoise,
				Warning,
				TEXT("%s cannot initialize Poise: its Ability System has no UNarrativeAttributeSetBase."),
				*GetNameSafe(GetOwner()));
			bWarnedMissingAttributeSet = true;
		}
		return false;
	}

	const uint64 RetiredGeneration = BindingGeneration;
	UninitializeFromAbilitySystem();
	// Releasing owned tags can synchronously bind another ASC. That binding wins.
	if (BindingGeneration != RetiredGeneration + 1 || bEndingPlay
		|| !IsValid(InAbilitySystemComponent) || InAbilitySystemComponent->GetAvatarActor() != GetOwner()) { return false; }
	AbilitySystemComponent = InAbilitySystemComponent;
	if (const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		BoundActorInfoEpoch = NarrativeASC->GetCombatActorInfoEpoch();
		BoundReadyEpoch = NarrativeASC->GetCharacterReadyEpoch();
	}
	const uint64 ExpectedBinding = BindingGeneration;
	const uint64 ExpectedOperation = LifecycleGeneration;
	bWarnedMissingAttributeSet = false;

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	PressuredTag = Tags.State_Poise_Pressured;
	BrokenTag = Tags.State_Poise_Broken;
	RecoveringTag = Tags.State_Poise_Recovering;
	RegenerationBlockedTag = Tags.State_Poise_RegenBlocked;

	PoiseChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetPoiseAttribute())
		.AddWeakLambda(this, [this, ExpectedBinding](const FOnAttributeChangeData& Data)
		{
			if (IsCurrentBinding(ExpectedBinding)) { HandlePoiseAttributeChanged(Data); }
		});

	MaxPoiseChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxPoiseAttribute())
		.AddWeakLambda(this, [this, ExpectedBinding](const FOnAttributeChangeData& Data)
		{
			if (IsCurrentBinding(ExpectedBinding)) { HandleMaxPoiseAttributeChanged(Data); }
		});

	if (RegenerationBlockedTag.IsValid())
	{
		RegenerationBlockedTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RegenerationBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddWeakLambda(this, [this, ExpectedBinding](FGameplayTag Tag, int32 Count)
			{
				if (IsCurrentBinding(ExpectedBinding)) { HandleRegenerationBlockedTagChanged(Tag, Count); }
			});
	}

	if (BrokenTag.IsValid())
	{
		BrokenTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				BrokenTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddWeakLambda(this, [this, ExpectedBinding](FGameplayTag Tag, int32 Count)
			{
				if (IsCurrentBinding(ExpectedBinding)) { HandleReplicatedStateTagChanged(Tag, Count); }
			});
	}

	if (RecoveringTag.IsValid())
	{
		RecoveringTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RecoveringTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddWeakLambda(this, [this, ExpectedBinding](FGameplayTag Tag, int32 Count)
			{
				if (IsCurrentBinding(ExpectedBinding)) { HandleReplicatedStateTagChanged(Tag, Count); }
			});
	}

	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddWeakLambda(this, [this, ExpectedBinding](const FOnAttributeChangeData& Data)
		{
			if (IsCurrentBinding(ExpectedBinding)) { HandleHealthAttributeChanged(Data); }
		});

	if (auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnerDeathChanged);
		NarrativeASC->OnCharacterReadyEpochChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnerReadyEpochChanged);
	}

	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = true;
	LastPoiseDamageWorldTime = GetWorldTimeSeconds();
	LastRegenerationUpdateWorldTime = LastPoiseDamageWorldTime;
	PoiseState = ESovPoiseState::Stable;
	RefreshPoiseState(false);
	if (!IsCurrentOperation(ExpectedOperation)) { return false; }
	TryStartRegeneration();
	return IsCurrentBinding(ExpectedBinding);
}

void USovPoiseComponent::SetCheckpointRestoreInProgress(const bool bInProgress)
{
	SetCheckpointRestoreInProgress(bInProgress, BindingGeneration);
}

void USovPoiseComponent::SetCheckpointRestoreInProgress(const bool bInProgress, const uint64 ExpectedBindingGeneration)
{
	// A retired restore scope must never release the replacement binding's hold.
	if (!IsCurrentBinding(ExpectedBindingGeneration) || bRestoringCheckpoint == bInProgress) { return; }
	++LifecycleGeneration;
	ClearLifecycleTimers();
	bRestoringCheckpoint = bInProgress;
	if (bInProgress) { bCheckpointResetPending = false; }
	else if (bCheckpointResetPending)
	{
		bCheckpointResetPending = false;
		ResetForCheckpoint();
	}
}

void USovPoiseComponent::ResetForCheckpoint()
{
	if (!CanWritePoise()) { return; }
	const uint64 ExpectedOperation = ++LifecycleGeneration;
	ClearLifecycleTimers();
	RemoveOwnedStateTags();
	if (!IsCurrentOperation(ExpectedOperation)) { return; }
	PoiseState = ESovPoiseState::Stable;
	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = true;
	LastPoiseDamageWorldTime = GetWorldTimeSeconds();
	LastRegenerationUpdateWorldTime = LastPoiseDamageWorldTime;
	RefreshPoiseState(false);
	if (!IsCurrentOperation(ExpectedOperation)) { return; }
	if (bRestoringCheckpoint) { bCheckpointResetPending = true; return; }
	if (GetPoise() > KINDA_SMALL_NUMBER && GetPoise() + KINDA_SMALL_NUMBER < GetMaxPoise()) { RecordPoiseDamage(); }
}

bool USovPoiseComponent::IsInitialized() const
{
	return IsCurrentBinding(BindingGeneration);
}

float USovPoiseComponent::GetPoise() const
{
	return IsInitialized()
		? AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetPoiseAttribute())
		: 0.0f;
}

float USovPoiseComponent::GetMaxPoise() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxPoiseAttribute()),
			0.0f)
		: 0.0f;
}

bool USovPoiseComponent::IsRegenerationBlocked() const
{
	return IsInitialized()
		&& RegenerationBlockedTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(RegenerationBlockedTag);
}

bool USovPoiseComponent::IsRegenerating() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(RegenerationTimerHandle);
	}

	return false;
}

float USovPoiseComponent::GetSecondsUntilRegeneration() const
{
	if (!bHasRecordedPoiseDamage || bRegenerationDelayElapsed)
	{
		return 0.0f;
	}

	return FMath::Max(
		(LastPoiseDamageWorldTime + FMath::Max(RegenerationDelay, 0.0f)) - GetWorldTimeSeconds(),
		0.0f);
}

float USovPoiseComponent::GetSecondsUntilBreakRecovery() const
{
	return GetTimerRemaining(BrokenFallbackTimerHandle);
}

float USovPoiseComponent::GetSecondsUntilRecoveryComplete() const
{
	return GetTimerRemaining(RecoveryTimerHandle);
}

bool USovPoiseComponent::RecoverFromPoiseBreak()
{
	if (!CanWritePoise() || bRestoringCheckpoint || PoiseState != ESovPoiseState::Broken)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(BrokenFallbackTimerHandle);
		TimerManager.ClearTimer(RegenerationDelayTimerHandle);
	}

	StopRegeneration();
	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = true;
	const uint64 ExpectedOperation = LifecycleGeneration;
	SetPoiseInternal(GetMaxPoise());
	if (!IsCurrentOperation(ExpectedOperation) || PoiseState != ESovPoiseState::Broken) { return false; }
	SetPoiseState(ESovPoiseState::Recovering, true);
	if (!IsCurrentOperation(ExpectedOperation) || PoiseState != ESovPoiseState::Recovering) { return false; }
	ScheduleRecoveryEnd();
	return true;
}

void USovPoiseComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		InitializeWithAbilitySystem(OwnerASC);
	}
}

void USovPoiseComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovPoiseComponent::UninitializeFromAbilitySystem()
{
	++BindingGeneration;
	++LifecycleGeneration;
	ClearLifecycleTimers();
	// Detach every local field before removing tags. GAS tag callbacks may rebind us.
	TStrongObjectPtr<UAbilitySystemComponent> PreviousASC(AbilitySystemComponent.Get());
	const FDelegateHandle PreviousPoiseDelegate = PoiseChangedDelegateHandle;
	PoiseChangedDelegateHandle.Reset();
	const FDelegateHandle PreviousMaxPoiseDelegate = MaxPoiseChangedDelegateHandle;
	MaxPoiseChangedDelegateHandle.Reset();
	const FDelegateHandle PreviousHealthDelegate = HealthChangedDelegateHandle;
	HealthChangedDelegateHandle.Reset();
	const FGameplayTag PreviousRegenerationBlockedTag = RegenerationBlockedTag;
	RegenerationBlockedTag = FGameplayTag();
	const FDelegateHandle PreviousRegenerationBlockedDelegate = RegenerationBlockedTagChangedDelegateHandle;
	RegenerationBlockedTagChangedDelegateHandle.Reset();
	const FGameplayTag PreviousBrokenTag = BrokenTag;
	BrokenTag = FGameplayTag();
	const FDelegateHandle PreviousBrokenDelegate = BrokenTagChangedDelegateHandle;
	BrokenTagChangedDelegateHandle.Reset();
	const bool bPreviouslyAppliedBroken = bAppliedBrokenTag;
	bAppliedBrokenTag = false;
	const FGameplayTag PreviousRecoveringTag = RecoveringTag;
	RecoveringTag = FGameplayTag();
	const FDelegateHandle PreviousRecoveringDelegate = RecoveringTagChangedDelegateHandle;
	RecoveringTagChangedDelegateHandle.Reset();
	const bool bPreviouslyAppliedRecovering = bAppliedRecoveringTag;
	bAppliedRecoveringTag = false;
	const FGameplayTag PreviousPressuredTag = PressuredTag;
	PressuredTag = FGameplayTag();
	const bool bPreviouslyAppliedPressured = bAppliedPressuredTag;
	bAppliedPressuredTag = false;
	AbilitySystemComponent = nullptr;
	BoundActorInfoEpoch = 0;
	BoundReadyEpoch = 0;
	bRestoringCheckpoint = false;
	bCheckpointResetPending = false;
	bHasRecordedPoiseDamage = false;
	PoiseState = ESovPoiseState::Stable;
	bRegenerationDelayElapsed = false;
	LastRegenerationUpdateWorldTime = 0.f;
	if (!PreviousASC.IsValid()) { return; }

	PreviousASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetPoiseAttribute()).Remove(PreviousPoiseDelegate);
	PreviousASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxPoiseAttribute()).Remove(PreviousMaxPoiseDelegate);
	PreviousASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute()).Remove(PreviousHealthDelegate);
	if (PreviousRegenerationBlockedTag.IsValid())
	{
		PreviousASC->RegisterGameplayTagEvent(PreviousRegenerationBlockedTag, EGameplayTagEventType::NewOrRemoved).Remove(PreviousRegenerationBlockedDelegate);
	}
	if (PreviousBrokenTag.IsValid())
	{
		PreviousASC->RegisterGameplayTagEvent(PreviousBrokenTag, EGameplayTagEventType::NewOrRemoved).Remove(PreviousBrokenDelegate);
	}
	if (PreviousRecoveringTag.IsValid())
	{
		PreviousASC->RegisterGameplayTagEvent(PreviousRecoveringTag, EGameplayTagEventType::NewOrRemoved).Remove(PreviousRecoveringDelegate);
	}
	if (auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(PreviousASC.Get()))
	{
		NarrativeASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleOwnerDeathChanged);
		NarrativeASC->OnCharacterReadyEpochChanged.RemoveDynamic(this, &ThisClass::HandleOwnerReadyEpochChanged);
	}

	if (bPreviouslyAppliedBroken && PreviousBrokenTag.IsValid())
	{
		PreviousASC->RemoveLooseGameplayTag(PreviousBrokenTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	if (bPreviouslyAppliedRecovering && PreviousRecoveringTag.IsValid())
	{
		PreviousASC->RemoveLooseGameplayTag(PreviousRecoveringTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	if (bPreviouslyAppliedPressured && PreviousPressuredTag.IsValid())
	{
		PreviousASC->RemoveLooseGameplayTag(PreviousPressuredTag, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void USovPoiseComponent::ClearLifecycleTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RegenerationDelayTimerHandle);
		TimerManager.ClearTimer(RegenerationTimerHandle);
		TimerManager.ClearTimer(BrokenFallbackTimerHandle);
		TimerManager.ClearTimer(RecoveryTimerHandle);
	}
}

void USovPoiseComponent::HandlePoiseAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasLiveOwner() || !FMath::IsNearlyEqual(ChangeData.NewValue, GetPoise())) { return; }
	const uint64 ExpectedOperation = LifecycleGeneration;
	const float OldPoise = FMath::Max(ChangeData.OldValue, 0.0f);
	const float NewPoise = FMath::Clamp(ChangeData.NewValue, 0.0f, GetMaxPoise());

	if (CanWritePoise() && !bRestoringCheckpoint && NewPoise + KINDA_SMALL_NUMBER < OldPoise)
	{
		RecordPoiseDamage();
	}

	if (NewPoise <= KINDA_SMALL_NUMBER
		&& GetMaxPoise() > KINDA_SMALL_NUMBER
		&& PoiseState != ESovPoiseState::Recovering)
	{
		EnterBrokenState(!bRestoringCheckpoint);
	}
	else
	{
		RefreshPoiseState(!bRestoringCheckpoint);
	}
	if (!IsCurrentOperation(ExpectedOperation)) { return; }

	OnPoiseChanged.Broadcast(OldPoise, NewPoise, GetMaxPoise());

	if (!IsCurrentOperation(ExpectedOperation) || !CanWritePoise() || bRestoringCheckpoint)
	{
		return;
	}

	if (PoiseState == ESovPoiseState::Broken
		|| NewPoise + KINDA_SMALL_NUMBER >= GetMaxPoise())
	{
		StopRegeneration();
	}
	else if (NewPoise > OldPoise)
	{
		TryStartRegeneration();
	}
}

void USovPoiseComponent::HandleMaxPoiseAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasLiveOwner() || !FMath::IsNearlyEqual(ChangeData.NewValue, GetMaxPoise())) { return; }
	const uint64 ExpectedOperation = LifecycleGeneration;
	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = FMath::Max(ChangeData.NewValue, 0.0f);

	if (CurrentMaxPoise <= KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			TimerManager.ClearTimer(RegenerationDelayTimerHandle);
			TimerManager.ClearTimer(BrokenFallbackTimerHandle);
			TimerManager.ClearTimer(RecoveryTimerHandle);
		}

		StopRegeneration();
		SetPoiseState(ESovPoiseState::Stable, true);
	}
	else
	{
		RefreshPoiseState(true);
	}
	if (!IsCurrentOperation(ExpectedOperation)) { return; }

	OnPoiseChanged.Broadcast(CurrentPoise, CurrentPoise, CurrentMaxPoise);
	if (!IsCurrentOperation(ExpectedOperation) || bRestoringCheckpoint) { return; }

	if (!CanWritePoise()
		|| PoiseState == ESovPoiseState::Broken
		|| CurrentPoise + KINDA_SMALL_NUMBER >= CurrentMaxPoise)
	{
		StopRegeneration();
		return;
	}

	TryStartRegeneration();
}

void USovPoiseComponent::HandleRegenerationBlockedTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	static_cast<void>(NewCount);

	if (!CanWritePoise())
	{
		return;
	}

	if (IsRegenerationBlocked())
	{
		StopRegeneration();
	}
	else
	{
		TryStartRegeneration();
	}
}

void USovPoiseComponent::HandleReplicatedStateTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);
	static_cast<void>(NewCount);

	if (HasLiveOwner() && !GetOwner()->HasAuthority())
	{
		RefreshPoiseState(true);
	}
}

void USovPoiseComponent::RecordPoiseDamage()
{
	if (!CanWritePoise() || bRestoringCheckpoint)
	{
		return;
	}

	bHasRecordedPoiseDamage = true;
	bRegenerationDelayElapsed = false;
	LastPoiseDamageWorldTime = GetWorldTimeSeconds();
	StopRegeneration();
	ScheduleRegenerationDelay(FMath::Max(RegenerationDelay, 0.0f));
}

void USovPoiseComponent::ScheduleRegenerationDelay(const float DelaySeconds)
{
	if (!CanWritePoise() || PoiseState == ESovPoiseState::Broken)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RegenerationDelayTimerHandle);

		if (DelaySeconds <= KINDA_SMALL_NUMBER)
		{
			HandleRegenerationDelayElapsed();
			return;
		}

		TimerManager.SetTimer(
			RegenerationDelayTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, ExpectedOperation = LifecycleGeneration]()
			{
				if (ValidateLifecycleCallback(ExpectedOperation)) { HandleRegenerationDelayElapsed(); }
			}),
			DelaySeconds,
			false,
			DelaySeconds);
	}
}

void USovPoiseComponent::HandleRegenerationDelayElapsed()
{
	if (!CanWritePoise() || bRestoringCheckpoint) { return; }
	bRegenerationDelayElapsed = true;
	TryStartRegeneration();
}

void USovPoiseComponent::TryStartRegeneration()
{
	if (!CanWritePoise()
		|| bRestoringCheckpoint
		|| PoiseState == ESovPoiseState::Broken
		|| IsRegenerationBlocked()
		|| RegenerationPercentPerSecond <= 0.0f
		|| RegenerationTimerInterval <= 0.0f)
	{
		StopRegeneration();
		return;
	}

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	if (CurrentMaxPoise <= KINDA_SMALL_NUMBER
		|| CurrentPoise + KINDA_SMALL_NUMBER >= CurrentMaxPoise)
	{
		StopRegeneration();
		return;
	}

	if (!bRegenerationDelayElapsed)
	{
		const float RemainingDelay = bHasRecordedPoiseDamage
			? (LastPoiseDamageWorldTime + FMath::Max(RegenerationDelay, 0.0f)) - GetWorldTimeSeconds()
			: 0.0f;

		if (RemainingDelay > KINDA_SMALL_NUMBER)
		{
			ScheduleRegenerationDelay(RemainingDelay);
			return;
		}

		bRegenerationDelayElapsed = true;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (TimerManager.IsTimerActive(RegenerationTimerHandle))
		{
			return;
		}

		const float EffectiveInterval = FMath::Max(RegenerationTimerInterval, 0.01f);
		LastRegenerationUpdateWorldTime = GetWorldTimeSeconds();
		TimerManager.SetTimer(
			RegenerationTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, ExpectedOperation = LifecycleGeneration]()
			{
				if (ValidateLifecycleCallback(ExpectedOperation)) { HandleRegenerationTimerElapsed(); }
			}),
			EffectiveInterval,
			true,
			EffectiveInterval);
	}
}

void USovPoiseComponent::HandleRegenerationTimerElapsed()
{
	if (!CanWritePoise()
		|| bRestoringCheckpoint
		|| PoiseState == ESovPoiseState::Broken
		|| IsRegenerationBlocked())
	{
		StopRegeneration();
		return;
	}

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	if (CurrentMaxPoise <= KINDA_SMALL_NUMBER
		|| CurrentPoise + KINDA_SMALL_NUMBER >= CurrentMaxPoise)
	{
		StopRegeneration();
		return;
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	const float RegenerationSeconds = FMath::Max(
		CurrentWorldTime - LastRegenerationUpdateWorldTime,
		0.0f);
	LastRegenerationUpdateWorldTime = CurrentWorldTime;

	if (RegenerationSeconds <= 0.0f)
	{
		return;
	}

	const float RegenerationAmount = CurrentMaxPoise
		* FMath::Max(RegenerationPercentPerSecond, 0.0f)
		* RegenerationSeconds;
	const float NewPoise = FMath::Min(CurrentPoise + RegenerationAmount, CurrentMaxPoise);
	const uint64 ExpectedOperation = LifecycleGeneration;
	SetPoiseInternal(NewPoise);
	if (!IsCurrentOperation(ExpectedOperation)) { return; }

	if (NewPoise + KINDA_SMALL_NUMBER >= CurrentMaxPoise)
	{
		StopRegeneration();
	}
}

void USovPoiseComponent::StopRegeneration()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenerationTimerHandle);
	}
	LastRegenerationUpdateWorldTime = 0.0f;
}

void USovPoiseComponent::EnterBrokenState(const bool bBroadcastChanges)
{
	if (!HasLiveOwner() || PoiseState == ESovPoiseState::Broken)
	{
		return;
	}

	StopRegeneration();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenerationDelayTimerHandle);
	}

	const uint64 ExpectedOperation = LifecycleGeneration;
	SetPoiseState(ESovPoiseState::Broken, bBroadcastChanges);
	if (IsCurrentOperation(ExpectedOperation) && CanWritePoise())
	{
		ScheduleBrokenFallback();
	}
}

void USovPoiseComponent::ScheduleBrokenFallback()
{
	if (!CanWritePoise() || bRestoringCheckpoint || PoiseState != ESovPoiseState::Broken)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(BrokenFallbackTimerHandle);

		const float EffectiveDuration = FMath::Max(BrokenFallbackDuration, 0.0f);
		if (EffectiveDuration <= KINDA_SMALL_NUMBER)
		{
			RecoverFromPoiseBreak();
			return;
		}

		TimerManager.SetTimer(
			BrokenFallbackTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, ExpectedOperation = LifecycleGeneration]()
			{
				if (ValidateLifecycleCallback(ExpectedOperation)) { HandleBrokenFallbackElapsed(); }
			}),
			EffectiveDuration,
			false,
			EffectiveDuration);
	}
}

void USovPoiseComponent::HandleBrokenFallbackElapsed()
{
	RecoverFromPoiseBreak();
}

void USovPoiseComponent::ScheduleRecoveryEnd()
{
	if (!CanWritePoise() || bRestoringCheckpoint || PoiseState != ESovPoiseState::Recovering)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RecoveryTimerHandle);

		const float EffectiveDuration = FMath::Max(RecoveryImmunityDuration, 0.0f);
		if (EffectiveDuration <= KINDA_SMALL_NUMBER)
		{
			HandleRecoveryElapsed();
			return;
		}

		TimerManager.SetTimer(
			RecoveryTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this, ExpectedOperation = LifecycleGeneration]()
			{
				if (ValidateLifecycleCallback(ExpectedOperation)) { HandleRecoveryElapsed(); }
			}),
			EffectiveDuration,
			false,
			EffectiveDuration);
	}
}

void USovPoiseComponent::HandleRecoveryElapsed()
{
	if (!CanWritePoise() || bRestoringCheckpoint || PoiseState != ESovPoiseState::Recovering)
	{
		return;
	}

	const uint64 ExpectedOperation = LifecycleGeneration;
	SetOwnedLooseTag(RecoveringTag, false, bAppliedRecoveringTag);
	if (!IsCurrentOperation(ExpectedOperation)) { return; }

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	const float PressuredThreshold = CurrentMaxPoise
		* FMath::Clamp(PressuredThresholdPercent, 0.0f, 1.0f);
	const ESovPoiseState NewState = CurrentMaxPoise > KINDA_SMALL_NUMBER
		&& CurrentPoise <= PressuredThreshold + KINDA_SMALL_NUMBER
		? ESovPoiseState::Pressured
		: ESovPoiseState::Stable;

	SetPoiseState(NewState, true);
	if (!IsCurrentOperation(ExpectedOperation)) { return; }
	TryStartRegeneration();
}

ESovPoiseState USovPoiseComponent::DeterminePoiseState() const
{
	if (!IsInitialized() || GetMaxPoise() <= KINDA_SMALL_NUMBER)
	{
		return ESovPoiseState::Stable;
	}

	if (BrokenTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(BrokenTag))
	{
		return ESovPoiseState::Broken;
	}

	if (RecoveringTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(RecoveringTag))
	{
		return ESovPoiseState::Recovering;
	}

	const float CurrentPoise = GetPoise();
	if (CurrentPoise <= KINDA_SMALL_NUMBER)
	{
		return ESovPoiseState::Broken;
	}

	const float PressuredThreshold = GetMaxPoise()
		* FMath::Clamp(PressuredThresholdPercent, 0.0f, 1.0f);
	return CurrentPoise <= PressuredThreshold + KINDA_SMALL_NUMBER
		? ESovPoiseState::Pressured
		: ESovPoiseState::Stable;
}

void USovPoiseComponent::RefreshPoiseState(const bool bBroadcastChanges)
{
	if (!HasLiveOwner()) { return; }
	const ESovPoiseState NewState = DeterminePoiseState();
	if (NewState == ESovPoiseState::Broken)
	{
		EnterBrokenState(bBroadcastChanges);
		return;
	}

	SetPoiseState(NewState, bBroadcastChanges);
}

void USovPoiseComponent::SetPoiseState(
	const ESovPoiseState NewState,
	const bool bBroadcastChanges)
{
	if (!HasLiveOwner()) { return; }
	const uint64 ExpectedOperation = LifecycleGeneration;
	if (NewState == PoiseState)
	{
		if (CanWritePoise())
		{
			UpdateOwnedStateTags(NewState);
		}
		return;
	}

	const ESovPoiseState PreviousState = PoiseState;
	PoiseState = NewState;

	if (CanWritePoise())
	{
		UpdateOwnedStateTags(NewState);
	}

	if (!IsCurrentOperation(ExpectedOperation) || PoiseState != NewState || !bBroadcastChanges)
	{
		return;
	}

	OnPoiseStateChanged.Broadcast(PreviousState, NewState);
	if (!IsCurrentOperation(ExpectedOperation) || PoiseState != NewState) { return; }

	if (NewState == ESovPoiseState::Broken)
	{
		OnPoiseBroken.Broadcast();
	}
	else if (PreviousState == ESovPoiseState::Broken)
	{
		OnPoiseRecovered.Broadcast();
	}
}

void USovPoiseComponent::UpdateOwnedStateTags(const ESovPoiseState NewState)
{
	if (!CanWritePoise()) { return; }
	const uint64 ExpectedOperation = LifecycleGeneration;
	SetOwnedLooseTag(
		PressuredTag,
		NewState == ESovPoiseState::Pressured,
		bAppliedPressuredTag);
	if (!IsCurrentOperation(ExpectedOperation) || PoiseState != NewState) { return; }
	SetOwnedLooseTag(
		BrokenTag,
		NewState == ESovPoiseState::Broken,
		bAppliedBrokenTag);
	if (!IsCurrentOperation(ExpectedOperation) || PoiseState != NewState) { return; }
	SetOwnedLooseTag(
		RecoveringTag,
		NewState == ESovPoiseState::Recovering,
		bAppliedRecoveringTag);
}

void USovPoiseComponent::RemoveOwnedStateTags()
{
	const uint64 ExpectedBinding = BindingGeneration;
	const uint64 ExpectedOperation = LifecycleGeneration;
	SetOwnedLooseTag(PressuredTag, false, bAppliedPressuredTag);
	if (BindingGeneration != ExpectedBinding || LifecycleGeneration != ExpectedOperation) { return; }
	SetOwnedLooseTag(BrokenTag, false, bAppliedBrokenTag);
	if (BindingGeneration != ExpectedBinding || LifecycleGeneration != ExpectedOperation) { return; }
	SetOwnedLooseTag(RecoveringTag, false, bAppliedRecoveringTag);
}

void USovPoiseComponent::SetOwnedLooseTag(
	const FGameplayTag& Tag,
	const bool bShouldApply,
	bool& bAppliedFlag)
{
	if (!IsValid(AbilitySystemComponent) || !Tag.IsValid())
	{
		if (!bShouldApply)
		{
			bAppliedFlag = false;
		}
		return;
	}

	if (bShouldApply && !bAppliedFlag && CanWritePoise())
	{
		bAppliedFlag = true;
		AbilitySystemComponent->AddLooseGameplayTag(
			Tag,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	else if (!bShouldApply && bAppliedFlag)
	{
		bAppliedFlag = false;
		AbilitySystemComponent->RemoveLooseGameplayTag(
			Tag,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
}

void USovPoiseComponent::SetPoiseInternal(const float NewPoise)
{
	if (!CanWritePoise())
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetPoiseAttribute(),
		FMath::Clamp(NewPoise, 0.0f, GetMaxPoise()));
}

bool USovPoiseComponent::IsCurrentBinding(const uint64 ExpectedGeneration) const
{
	if (ExpectedGeneration != BindingGeneration || bEndingPlay || !IsValid(this)
		|| !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
		|| !IsValid(AbilitySystemComponent) || AbilitySystemComponent->GetAvatarActor() != GetOwner()) { return false; }
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	return !NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == BoundActorInfoEpoch
		&& NarrativeASC->GetCharacterReadyEpoch() == BoundReadyEpoch);
}

bool USovPoiseComponent::HasLiveOwner() const
{
	if (!IsInitialized()) { return false; }
	const float Health = AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	return FMath::IsFinite(Health) && Health > 0.f && (!NarrativeASC || !NarrativeASC->IsDead());
}

bool USovPoiseComponent::IsCurrentOperation(const uint64 ExpectedGeneration) const
{
	return LifecycleGeneration == ExpectedGeneration && IsInitialized() && HasLiveOwner();
}

bool USovPoiseComponent::ValidateLifecycleCallback(const uint64 ExpectedGeneration)
{
	if (IsCurrentOperation(ExpectedGeneration) && !bRestoringCheckpoint) { return true; }
	// Clear only the retired timer generation, never timers installed by a reentrant reset.
	if (LifecycleGeneration == ExpectedGeneration)
	{
		++LifecycleGeneration;
		ClearLifecycleTimers();
	}
	return false;
}

void USovPoiseComponent::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!IsInitialized()) { return; }
	const float CurrentHealth = AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute());
	if (FMath::IsFinite(CurrentHealth) && CurrentHealth > 0.f) { return; }
	static_cast<void>(ChangeData);
	++LifecycleGeneration;
	ClearLifecycleTimers();
	bCheckpointResetPending = false;
	RemoveOwnedStateTags();
}

void USovPoiseComponent::HandleOwnerDeathChanged(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledASC, const bool bIsDead)
{
	if (!bIsDead || KilledActor != GetOwner() || KilledASC != AbilitySystemComponent
		|| !IsInitialized() || !KilledASC->IsDead()) { return; }
	++LifecycleGeneration;
	ClearLifecycleTimers();
	bCheckpointResetPending = false;
	RemoveOwnedStateTags();
}

void USovPoiseComponent::HandleOwnerReadyEpochChanged(const int32 ReadyEpoch)
{
	// Player readiness is published after the legacy OnASCInitialized callback.
	// Rebind even when the pointer is unchanged so the initial ready epoch remains usable.
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	if (!bEndingPlay && IsValid(NarrativeASC) && NarrativeASC->GetAvatarActor() == GetOwner()
		&& NarrativeASC->GetCharacterReadyEpoch() == ReadyEpoch)
	{
		InitializeWithAbilitySystem(AbilitySystemComponent);
	}
}

bool USovPoiseComponent::CanWritePoise() const
{
	return HasLiveOwner() && GetOwner()->HasAuthority();
}

float USovPoiseComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}

float USovPoiseComponent::GetTimerRemaining(const FTimerHandle& TimerHandle) const
{
	if (UWorld* World = GetWorld())
	{
		return FMath::Max(World->GetTimerManager().GetTimerRemaining(TimerHandle), 0.0f);
	}

	return 0.0f;
}
