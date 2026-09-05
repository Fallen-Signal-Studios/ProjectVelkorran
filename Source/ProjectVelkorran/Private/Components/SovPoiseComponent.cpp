// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovPoiseComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
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
	if (!IsValid(InAbilitySystemComponent))
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
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

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	bWarnedMissingAttributeSet = false;

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	PressuredTag = Tags.State_Poise_Pressured;
	BrokenTag = Tags.State_Poise_Broken;
	RecoveringTag = Tags.State_Poise_Recovering;
	RegenerationBlockedTag = Tags.State_Poise_RegenBlocked;

	PoiseChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetPoiseAttribute())
		.AddUObject(this, &ThisClass::HandlePoiseAttributeChanged);

	MaxPoiseChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxPoiseAttribute())
		.AddUObject(this, &ThisClass::HandleMaxPoiseAttributeChanged);

	if (RegenerationBlockedTag.IsValid())
	{
		RegenerationBlockedTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RegenerationBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleRegenerationBlockedTagChanged);
	}

	if (BrokenTag.IsValid())
	{
		BrokenTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				BrokenTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleReplicatedStateTagChanged);
	}

	if (RecoveringTag.IsValid())
	{
		RecoveringTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RecoveringTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleReplicatedStateTagChanged);
	}

	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = true;
	LastPoiseDamageWorldTime = GetWorldTimeSeconds();
	LastRegenerationUpdateWorldTime = LastPoiseDamageWorldTime;
	PoiseState = ESovPoiseState::Stable;
	RefreshPoiseState(false);

	TryStartRegeneration();
	return true;
}

void USovPoiseComponent::ResetForCheckpoint()
{
	if (!CanWritePoise()) { return; }
	ClearLifecycleTimers();
	RemoveOwnedStateTags();
	PoiseState = ESovPoiseState::Stable;
	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = true;
	LastPoiseDamageWorldTime = GetWorldTimeSeconds();
	LastRegenerationUpdateWorldTime = LastPoiseDamageWorldTime;
	RefreshPoiseState(false);
	if (GetPoise() > KINDA_SMALL_NUMBER && GetPoise() + KINDA_SMALL_NUMBER < GetMaxPoise()) { RecordPoiseDamage(); }
}

bool USovPoiseComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
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
	if (!CanWritePoise() || PoiseState != ESovPoiseState::Broken)
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
	SetPoiseInternal(GetMaxPoise());
	SetPoiseState(ESovPoiseState::Recovering, true);
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
		if (OwnerASC != AbilitySystemComponent)
		{
			InitializeWithAbilitySystem(OwnerASC);
		}
	}
}

void USovPoiseComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovPoiseComponent::UninitializeFromAbilitySystem()
{
	StopRegeneration();

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RegenerationDelayTimerHandle);
		TimerManager.ClearTimer(BrokenFallbackTimerHandle);
		TimerManager.ClearTimer(RecoveryTimerHandle);
	}

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		PoiseChangedDelegateHandle.Reset();
		MaxPoiseChangedDelegateHandle.Reset();
		RegenerationBlockedTagChangedDelegateHandle.Reset();
		BrokenTagChangedDelegateHandle.Reset();
		RecoveringTagChangedDelegateHandle.Reset();
		PressuredTag = FGameplayTag();
		BrokenTag = FGameplayTag();
		RecoveringTag = FGameplayTag();
		RegenerationBlockedTag = FGameplayTag();
		PoiseState = ESovPoiseState::Stable;
		bHasRecordedPoiseDamage = false;
		bRegenerationDelayElapsed = false;
		bAppliedPressuredTag = false;
		bAppliedBrokenTag = false;
		bAppliedRecoveringTag = false;
		return;
	}

	if (PoiseChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetPoiseAttribute())
			.Remove(PoiseChangedDelegateHandle);
	}

	if (MaxPoiseChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxPoiseAttribute())
			.Remove(MaxPoiseChangedDelegateHandle);
	}

	if (RegenerationBlockedTag.IsValid() && RegenerationBlockedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RegenerationBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(RegenerationBlockedTagChangedDelegateHandle);
	}

	if (BrokenTag.IsValid() && BrokenTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(
				BrokenTag,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(BrokenTagChangedDelegateHandle);
	}

	if (RecoveringTag.IsValid() && RecoveringTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RecoveringTag,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(RecoveringTagChangedDelegateHandle);
	}

	RemoveOwnedStateTags();

	AbilitySystemComponent = nullptr;
	PoiseChangedDelegateHandle.Reset();
	MaxPoiseChangedDelegateHandle.Reset();
	RegenerationBlockedTagChangedDelegateHandle.Reset();
	BrokenTagChangedDelegateHandle.Reset();
	RecoveringTagChangedDelegateHandle.Reset();
	PressuredTag = FGameplayTag();
	BrokenTag = FGameplayTag();
	RecoveringTag = FGameplayTag();
	RegenerationBlockedTag = FGameplayTag();
	PoiseState = ESovPoiseState::Stable;
	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = false;
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

	OnPoiseChanged.Broadcast(OldPoise, NewPoise, GetMaxPoise());

	if (!CanWritePoise())
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

	OnPoiseChanged.Broadcast(CurrentPoise, CurrentPoise, CurrentMaxPoise);

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

	if (!CanWritePoise())
	{
		return;
	}

	if (NewCount > 0)
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

	if (!CanWritePoise())
	{
		RefreshPoiseState(true);
	}
}

void USovPoiseComponent::RecordPoiseDamage()
{
	if (!CanWritePoise())
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
			this,
			&ThisClass::HandleRegenerationDelayElapsed,
			DelaySeconds,
			false,
			DelaySeconds);
	}
}

void USovPoiseComponent::HandleRegenerationDelayElapsed()
{
	bRegenerationDelayElapsed = true;
	TryStartRegeneration();
}

void USovPoiseComponent::TryStartRegeneration()
{
	if (!CanWritePoise()
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
			this,
			&ThisClass::HandleRegenerationTimerElapsed,
			EffectiveInterval,
			true,
			EffectiveInterval);
	}
}

void USovPoiseComponent::HandleRegenerationTimerElapsed()
{
	if (!CanWritePoise()
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
	SetPoiseInternal(NewPoise);

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
	if (PoiseState == ESovPoiseState::Broken)
	{
		return;
	}

	StopRegeneration();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenerationDelayTimerHandle);
	}

	SetPoiseState(ESovPoiseState::Broken, bBroadcastChanges);
	if (CanWritePoise())
	{
		ScheduleBrokenFallback();
	}
}

void USovPoiseComponent::ScheduleBrokenFallback()
{
	if (!CanWritePoise() || PoiseState != ESovPoiseState::Broken)
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
			this,
			&ThisClass::HandleBrokenFallbackElapsed,
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
	if (!CanWritePoise() || PoiseState != ESovPoiseState::Recovering)
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
			this,
			&ThisClass::HandleRecoveryElapsed,
			EffectiveDuration,
			false,
			EffectiveDuration);
	}
}

void USovPoiseComponent::HandleRecoveryElapsed()
{
	if (!CanWritePoise() || PoiseState != ESovPoiseState::Recovering)
	{
		return;
	}

	SetOwnedLooseTag(RecoveringTag, false, bAppliedRecoveringTag);

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	const float PressuredThreshold = CurrentMaxPoise
		* FMath::Clamp(PressuredThresholdPercent, 0.0f, 1.0f);
	const ESovPoiseState NewState = CurrentMaxPoise > KINDA_SMALL_NUMBER
		&& CurrentPoise <= PressuredThreshold + KINDA_SMALL_NUMBER
		? ESovPoiseState::Pressured
		: ESovPoiseState::Stable;

	SetPoiseState(NewState, true);
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

	if (!bBroadcastChanges)
	{
		return;
	}

	OnPoiseStateChanged.Broadcast(PreviousState, NewState);

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
	SetOwnedLooseTag(
		PressuredTag,
		NewState == ESovPoiseState::Pressured,
		bAppliedPressuredTag);
	SetOwnedLooseTag(
		BrokenTag,
		NewState == ESovPoiseState::Broken,
		bAppliedBrokenTag);
	SetOwnedLooseTag(
		RecoveringTag,
		NewState == ESovPoiseState::Recovering,
		bAppliedRecoveringTag);
}

void USovPoiseComponent::RemoveOwnedStateTags()
{
	SetOwnedLooseTag(PressuredTag, false, bAppliedPressuredTag);
	SetOwnedLooseTag(BrokenTag, false, bAppliedBrokenTag);
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

	if (bShouldApply && !bAppliedFlag)
	{
		AbilitySystemComponent->AddLooseGameplayTag(
			Tag,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		bAppliedFlag = true;
	}
	else if (!bShouldApply && bAppliedFlag)
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(
			Tag,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
		bAppliedFlag = false;
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

bool USovPoiseComponent::CanWritePoise() const
{
	return IsInitialized() && IsValid(GetOwner()) && GetOwner()->HasAuthority();
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
