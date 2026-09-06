// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovPoiseComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
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
	AActor* Owner = GetOwner();
	if (bEndingPlay || bChangingAbilitySystem || !IsValid(Owner) || Owner->IsActorBeingDestroyed()
		|| !IsValid(InAbilitySystemComponent) || InAbilitySystemComponent->GetAvatarActor() != Owner
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) != InAbilitySystemComponent
		|| Owner->FindComponentByClass<USovPoiseComponent>() != this)
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& IsInitialized()
		&& PoiseChangedDelegateHandle.IsValid()
		&& MaxPoiseChangedDelegateHandle.IsValid())
	{
		return true;
	}

	const UNarrativeAttributeSetBase* Attributes = InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>();
	if (!IsValid(Attributes) || Attributes->GetOwningAbilitySystemComponent() != InAbilitySystemComponent)
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

	const auto* RequestedNarrativeASC = Cast<UNarrativeAbilitySystemComponent>(InAbilitySystemComponent);
	const uint64 RequestedActorEpoch = RequestedNarrativeASC ? RequestedNarrativeASC->GetCombatActorInfoEpoch() : 0;
	const uint64 RequestedLifeEpoch = Attributes->GetCombatLifeEpoch();
	const int32 RequestedReadyEpoch = RequestedNarrativeASC ? RequestedNarrativeASC->GetCharacterReadyEpoch() : 0;
	TGuardValue<bool> ChangingGuard(bChangingAbilitySystem, true);
	UninitializeFromAbilitySystem();
	if (!IsValid(Owner) || Owner->IsActorBeingDestroyed() || bEndingPlay || !IsValid(InAbilitySystemComponent)
		|| InAbilitySystemComponent->GetAvatarActor() != Owner
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) != InAbilitySystemComponent
		|| InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>() != Attributes
		|| !IsValid(Attributes) || Attributes->GetCombatLifeEpoch() != RequestedLifeEpoch
		|| (RequestedNarrativeASC && (RequestedNarrativeASC->GetCombatActorInfoEpoch() != RequestedActorEpoch
			|| RequestedNarrativeASC->GetCharacterReadyEpoch() != RequestedReadyEpoch))) { return false; }
	AbilitySystemComponent = InAbilitySystemComponent;
	BoundAttributes = Attributes;
	BoundLifeEpoch = Attributes->GetCombatLifeEpoch();
	const auto* EpochASC = Cast<UNarrativeAbilitySystemComponent>(InAbilitySystemComponent);
	BoundActorInfoEpoch = EpochASC ? EpochASC->GetCombatActorInfoEpoch() : 0;
	BoundReadyEpoch = EpochASC ? EpochASC->GetCharacterReadyEpoch() : 0;
	const uint64 Generation = ++BindingGeneration;
	bWarnedMissingAttributeSet = false;

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	PressuredTag = Tags.State_Poise_Pressured;
	BrokenTag = Tags.State_Poise_Broken;
	RecoveringTag = Tags.State_Poise_Recovering;
	RegenerationBlockedTag = Tags.State_Poise_RegenBlocked;
	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthAttributeChanged);
	if (auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeathStateChanged);
		NarrativeASC->OnCharacterReadyEpochChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnerReadyEpochChanged);
	}

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
	if (!IsCurrentOperation(Generation)) { return false; }
	TryStartRegeneration();
	return true;
}

void USovPoiseComponent::ResetForCheckpoint()
{
	if (bChangingAbilitySystem || bEndingPlay) { return; }
	++ReviveRebindGeneration;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReviveRebindTimerHandle); }
	if (!IsInitialized() && !InitializeWithAbilitySystem(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))) { return; }
	if (!IsInitialized() || !GetOwner()->HasAuthority()) { return; }
	TGuardValue<bool> ChangingGuard(bChangingAbilitySystem, true);
	const uint64 Generation = ++BindingGeneration;
	ClearLifecycleTimers();
	RemoveOwnedStateTags();
	if (!IsCurrentOperation(Generation)) { return; }
	PoiseState = ESovPoiseState::Stable;
	bHasRecordedPoiseDamage = false;
	bRegenerationDelayElapsed = true;
	LastPoiseDamageWorldTime = GetWorldTimeSeconds();
	LastRegenerationUpdateWorldTime = LastPoiseDamageWorldTime;
	RefreshPoiseState(false);
	if (!IsCurrentOperation(Generation)) { return; }
	if (GetPoise() > KINDA_SMALL_NUMBER && GetPoise() + KINDA_SMALL_NUMBER < GetMaxPoise())
	{
		bHasRecordedPoiseDamage = true;
		bRegenerationDelayElapsed = false;
		if (!bRestoringCheckpoint) { TryStartRegeneration(); }
	}
	bCheckpointStateReconciled = IsCurrentOperation(Generation);
}

void USovPoiseComponent::SetCheckpointRestoreInProgress(const bool bInProgress, const bool bResumePassiveWork)
{
	if (bInProgress) { ++CheckpointRestoreGeneration; }
	if (!bInProgress && bRestoringCheckpoint == bInProgress)
	{
		if (!bInProgress && !bResumePassiveWork) { ++BindingGeneration; bCheckpointStateReconciled = false; ClearLifecycleTimers(); }
		return;
	}
	bRestoringCheckpoint = bInProgress;
	++BindingGeneration;
	if (bInProgress)
	{
		bCheckpointStateReconciled = false; ClearLifecycleTimers(); ++ReviveRebindGeneration;
		if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReviveRebindTimerHandle); }
	}
	else if (bResumePassiveWork && bCheckpointStateReconciled && CanWritePoise())
	{
		if (PoiseState == ESovPoiseState::Broken) { ScheduleBrokenFallback(); }
		else if (PoiseState == ESovPoiseState::Recovering) { ScheduleRecoveryEnd(); }
		else { TryStartRegeneration(); }
	}
	else { bCheckpointStateReconciled = false; ClearLifecycleTimers(); }
}

bool USovPoiseComponent::EndCheckpointRestore(const uint64 Generation, const bool bResumePassiveWork)
{
	if (Generation != CheckpointRestoreGeneration || (bResumePassiveWork && !bRestoringCheckpoint)) { return false; }
	SetCheckpointRestoreInProgress(false, bResumePassiveWork);
	return Generation == CheckpointRestoreGeneration;
}

bool USovPoiseComponent::IsInitialized() const
{
	const AActor* Owner = GetOwner();
	const auto* ASC = AbilitySystemComponent.Get();
	const auto* Attributes = BoundAttributes.Get();
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
	return !bEndingPlay && IsValid(Owner) && !Owner->IsActorBeingDestroyed()
		&& IsValid(ASC) && IsValid(Attributes) && ASC->GetAvatarActor() == Owner
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Owner)) == ASC
		&& Owner->FindComponentByClass<USovPoiseComponent>() == this
		&& ASC->GetSet<UNarrativeAttributeSetBase>() == Attributes
		&& Attributes->GetOwningAbilitySystemComponent() == ASC
		&& Attributes->GetCombatLifeEpoch() == BoundLifeEpoch
		&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == BoundActorInfoEpoch
			&& NarrativeASC->GetCharacterReadyEpoch() == BoundReadyEpoch));
}

bool USovPoiseComponent::IsCurrentOperation(const uint64 Generation) const
{
	return BindingGeneration == Generation && IsInitialized();
}

bool USovPoiseComponent::ValidateBindingOrRetire()
{
	if (IsInitialized()) { return true; }
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	if (IsValid(GetOwner()) && IsValid(AbilitySystemComponent) && BoundAttributes.IsValid()
		&& AbilitySystemComponent->GetAvatarActor() == GetOwner()
		&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) == AbilitySystemComponent
		&& AbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>() == BoundAttributes.Get()
		&& (!NarrativeASC || (NarrativeASC->GetCombatActorInfoEpoch() == BoundActorInfoEpoch
			&& NarrativeASC->GetCharacterReadyEpoch() == BoundReadyEpoch)))
	{
		ClearLifecycleTimers(); RemoveOwnedStateTags(); return false;
	}
	if (AbilitySystemComponent && !bChangingAbilitySystem)
	{
		TGuardValue<bool> ChangingGuard(bChangingAbilitySystem, true);
		UninitializeFromAbilitySystem();
	}
	return false;
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
	if (!ValidateBindingOrRetire() || bRestoringCheckpoint || !CanWritePoise() || PoiseState != ESovPoiseState::Broken)
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
	const uint64 Generation = BindingGeneration;
	const float RecoveryPoise = GetMaxPoise();
	SetPoiseInternal(RecoveryPoise);
	if (!IsCurrentOperation(Generation) || !CanWritePoise()) { return false; }
	if (PoiseState != ESovPoiseState::Broken
		|| !FMath::IsNearlyEqual(GetPoise(), RecoveryPoise, KINDA_SMALL_NUMBER)
		|| !FMath::IsNearlyEqual(GetMaxPoise(), RecoveryPoise, KINDA_SMALL_NUMBER))
	{
		// One-shot damage during refill invalidates this recovery, not the next
		// authored fallback. Avoid immediate recursive retries when duration is zero.
		if (PoiseState == ESovPoiseState::Broken && GetWorld())
		{
			const float RetryDelay = FMath::IsFinite(BrokenFallbackDuration) ? FMath::Max(BrokenFallbackDuration, 0.01f) : 0.8f;
			GetWorld()->GetTimerManager().SetTimer(BrokenFallbackTimerHandle, this,
				&ThisClass::HandleBrokenFallbackElapsed, RetryDelay, false);
		}
		return false;
	}
	SetPoiseState(ESovPoiseState::Recovering, true);
	if (!IsCurrentOperation(Generation) || !CanWritePoise()) { return false; }
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
		if (OwnerASC != AbilitySystemComponent || !IsInitialized())
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
	++BindingGeneration;
	++ReviveRebindGeneration;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(ReviveRebindTimerHandle); }
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
		HealthChangedDelegateHandle.Reset();
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
	UAbilitySystemComponent* PreviousASC = AbilitySystemComponent;
	if (auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(PreviousASC))
	{
		NarrativeASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeathStateChanged);
		NarrativeASC->OnCharacterReadyEpochChanged.RemoveDynamic(this, &ThisClass::HandleOwnerReadyEpochChanged);
	}
	TArray<FGameplayTag> PreviousOwnedTags;
	if (bAppliedPressuredTag) { PreviousOwnedTags.Add(PressuredTag); }
	if (bAppliedBrokenTag) { PreviousOwnedTags.Add(BrokenTag); }
	if (bAppliedRecoveringTag) { PreviousOwnedTags.Add(RecoveringTag); }
	if (HealthChangedDelegateHandle.IsValid())
	{
		PreviousASC->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
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

	AbilitySystemComponent = nullptr;
	BoundAttributes.Reset();
	PoiseChangedDelegateHandle.Reset();
	HealthChangedDelegateHandle.Reset();
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
	for (const FGameplayTag& Tag : PreviousOwnedTags)
	{
		if (Tag.IsValid() && IsValid(PreviousASC))
		{
			PreviousASC->RemoveLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::TagAndCountToAll);
		}
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
	if (!ValidateBindingOrRetire() || bRestoringCheckpoint
		|| (GetOwner()->HasAuthority() && !CanWritePoise())) { return; }
	const uint64 Generation = BindingGeneration;
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

	if (!IsCurrentOperation(Generation)) { return; }
	OnPoiseChanged.Broadcast(OldPoise, NewPoise, GetMaxPoise());

	if (!IsCurrentOperation(Generation) || !CanWritePoise())
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
	if (!ValidateBindingOrRetire() || bRestoringCheckpoint
		|| (GetOwner()->HasAuthority() && !CanWritePoise())) { return; }
	const uint64 Generation = BindingGeneration;
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

	if (!IsCurrentOperation(Generation)) { return; }
	OnPoiseChanged.Broadcast(CurrentPoise, CurrentPoise, CurrentMaxPoise);
	if (!IsCurrentOperation(Generation)) { return; }

	if (!CanWritePoise()
		|| PoiseState == ESovPoiseState::Broken
		|| CurrentPoise + KINDA_SMALL_NUMBER >= CurrentMaxPoise)
	{
		StopRegeneration();
		return;
	}

	TryStartRegeneration();
}

void USovPoiseComponent::HandleHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (bRestoringCheckpoint || CanWritePoise()) { return; }
	++BindingGeneration;
	ClearLifecycleTimers();
	RemoveOwnedStateTags();
}

void USovPoiseComponent::HandleDeathStateChanged(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, const bool bDead)
{
	if (Actor != GetOwner() || ASC != AbilitySystemComponent || bEndingPlay || bRestoringCheckpoint) { return; }
	const uint64 Request = ++ReviveRebindGeneration;
	UWorld* World = GetWorld();
	if (!World) { return; }
	World->GetTimerManager().ClearTimer(ReviveRebindTimerHandle);
	if (bDead)
	{
		++BindingGeneration; ClearLifecycleTimers(); RemoveOwnedStateTags(); return;
	}
	const auto* Attributes = ASC->GetSet<UNarrativeAttributeSetBase>();
	if (!IsValid(Attributes)) { return; }
	const uint64 ExpectedLife = Attributes->GetCombatLifeEpoch() + (Attributes->GetHealth() <= 0.f ? 1 : 0);
	const uint64 ActorEpoch = ASC->GetCombatActorInfoEpoch();
	ReviveRebindTimerHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Request, ExpectedLife, ActorEpoch, WeakASC = TWeakObjectPtr<UNarrativeAbilitySystemComponent>(ASC),
		 WeakAttributes = TWeakObjectPtr<const UNarrativeAttributeSetBase>(Attributes)]()
		{
			auto* CurrentASC = WeakASC.Get(); const auto* CurrentAttributes = WeakAttributes.Get();
			if (Request != ReviveRebindGeneration || bRestoringCheckpoint || bEndingPlay || !IsValid(CurrentASC)
				|| !IsValid(CurrentAttributes) || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()
				|| CurrentASC->GetAvatarActor() != GetOwner()
				|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()) != CurrentASC
				|| CurrentASC->GetCombatActorInfoEpoch() != ActorEpoch
				|| CurrentASC->GetSet<UNarrativeAttributeSetBase>() != CurrentAttributes
				|| CurrentAttributes->GetCombatLifeEpoch() != ExpectedLife || CurrentAttributes->GetHealth() <= 0.f
				|| CurrentASC->IsDead()) { return; }
			ResetForCheckpoint();
		}));
}

void USovPoiseComponent::HandleRegenerationBlockedTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);

	if (!ValidateBindingOrRetire() || bRestoringCheckpoint || !CanWritePoise())
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

	if (ValidateBindingOrRetire() && !GetOwner()->HasAuthority() && !bRestoringCheckpoint)
	{
		RefreshPoiseState(true);
	}
}

void USovPoiseComponent::RecordPoiseDamage()
{
	if (bRestoringCheckpoint || !CanWritePoise())
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
	if (bRestoringCheckpoint || !CanWritePoise() || !FMath::IsFinite(DelaySeconds) || PoiseState == ESovPoiseState::Broken)
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
	if (!ValidateBindingOrRetire()) { return; }
	if (bRestoringCheckpoint || !CanWritePoise()) { return; }
	bRegenerationDelayElapsed = true;
	TryStartRegeneration();
}

void USovPoiseComponent::TryStartRegeneration()
{
	if (bRestoringCheckpoint || !CanWritePoise()
		|| PoiseState == ESovPoiseState::Broken
		|| IsRegenerationBlocked()
		|| !FMath::IsFinite(RegenerationDelay)
		|| !FMath::IsFinite(RegenerationPercentPerSecond)
		|| !FMath::IsFinite(RegenerationTimerInterval)
		|| RegenerationPercentPerSecond <= 0.0f
		|| RegenerationTimerInterval <= 0.0f)
	{
		StopRegeneration();
		return;
	}

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	if (!FMath::IsFinite(CurrentPoise) || !FMath::IsFinite(CurrentMaxPoise)
		|| CurrentMaxPoise <= KINDA_SMALL_NUMBER
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
	if (!ValidateBindingOrRetire()) { return; }
	if (bRestoringCheckpoint || !CanWritePoise()
		|| PoiseState == ESovPoiseState::Broken
		|| IsRegenerationBlocked())
	{
		StopRegeneration();
		return;
	}

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	if (!FMath::IsFinite(CurrentPoise) || !FMath::IsFinite(CurrentMaxPoise)
		|| !FMath::IsFinite(RegenerationPercentPerSecond)
		|| CurrentMaxPoise <= KINDA_SMALL_NUMBER
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
	const uint64 Generation = BindingGeneration;
	SetPoiseInternal(NewPoise);
	if (!IsCurrentOperation(Generation) || !CanWritePoise()) { return; }

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
	if (!IsInitialized() || (GetOwner()->HasAuthority() && !CanWritePoise())
		|| PoiseState == ESovPoiseState::Broken)
	{
		return;
	}

	StopRegeneration();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenerationDelayTimerHandle);
	}

	const uint64 Generation = BindingGeneration;
	SetPoiseState(ESovPoiseState::Broken, bBroadcastChanges);
	if (IsCurrentOperation(Generation) && CanWritePoise())
	{
		ScheduleBrokenFallback();
	}
}

void USovPoiseComponent::ScheduleBrokenFallback()
{
	if (bRestoringCheckpoint || !CanWritePoise() || !FMath::IsFinite(BrokenFallbackDuration)
		|| PoiseState != ESovPoiseState::Broken)
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
	if (bRestoringCheckpoint || !CanWritePoise() || !FMath::IsFinite(RecoveryImmunityDuration)
		|| PoiseState != ESovPoiseState::Recovering)
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
	if (!ValidateBindingOrRetire()) { return; }
	if (bRestoringCheckpoint || !CanWritePoise() || PoiseState != ESovPoiseState::Recovering)
	{
		return;
	}

	const uint64 Generation = BindingGeneration;
	SetOwnedLooseTag(RecoveringTag, false, bAppliedRecoveringTag);
	if (!IsCurrentOperation(Generation) || !CanWritePoise()) { return; }

	const float CurrentPoise = GetPoise();
	const float CurrentMaxPoise = GetMaxPoise();
	const float PressuredThreshold = CurrentMaxPoise
		* FMath::Clamp(PressuredThresholdPercent, 0.0f, 1.0f);
	const ESovPoiseState NewState = CurrentMaxPoise > KINDA_SMALL_NUMBER
		&& CurrentPoise <= PressuredThreshold + KINDA_SMALL_NUMBER
		? ESovPoiseState::Pressured
		: ESovPoiseState::Stable;

	SetPoiseState(NewState, true);
	if (!IsCurrentOperation(Generation) || !CanWritePoise()) { return; }
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
	if (!IsInitialized() || (GetOwner()->HasAuthority() && !CanWritePoise())) { return; }
	const uint64 Generation = BindingGeneration;
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

	if (!IsCurrentOperation(Generation) || PoiseState != NewState || !bBroadcastChanges)
	{
		return;
	}

	OnPoiseStateChanged.Broadcast(PreviousState, NewState);
	if (!IsCurrentOperation(Generation) || PoiseState != NewState
		|| (GetOwner()->HasAuthority() && !CanWritePoise())) { return; }

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
	const uint64 Generation = BindingGeneration;
	SetOwnedLooseTag(
		PressuredTag,
		NewState == ESovPoiseState::Pressured,
		bAppliedPressuredTag);
	if (!IsCurrentOperation(Generation) || !CanWritePoise() || PoiseState != NewState) { return; }
	SetOwnedLooseTag(
		BrokenTag,
		NewState == ESovPoiseState::Broken,
		bAppliedBrokenTag);
	if (!IsCurrentOperation(Generation) || !CanWritePoise() || PoiseState != NewState) { return; }
	SetOwnedLooseTag(
		RecoveringTag,
		NewState == ESovPoiseState::Recovering,
		bAppliedRecoveringTag);
}

void USovPoiseComponent::RemoveOwnedStateTags()
{
	UAbilitySystemComponent* PreviousASC = AbilitySystemComponent;
	TArray<FGameplayTag> Tags;
	if (bAppliedPressuredTag) { Tags.Add(PressuredTag); }
	if (bAppliedBrokenTag) { Tags.Add(BrokenTag); }
	if (bAppliedRecoveringTag) { Tags.Add(RecoveringTag); }
	bAppliedPressuredTag = false;
	bAppliedBrokenTag = false;
	bAppliedRecoveringTag = false;
	for (const FGameplayTag& Tag : Tags)
	{
		if (IsValid(PreviousASC) && Tag.IsValid())
		{
			PreviousASC->RemoveLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::TagAndCountToAll);
		}
	}
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
	if (bRestoringCheckpoint || !CanWritePoise() || !FMath::IsFinite(NewPoise))
	{
		return;
	}

	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetPoiseAttribute(),
		FMath::Clamp(NewPoise, 0.0f, GetMaxPoise()));
}

bool USovPoiseComponent::CanWritePoise() const
{
	if (!IsInitialized() || !GetOwner()->HasAuthority()) { return false; }
	const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	const float Health = BoundAttributes->GetHealth();
	return FMath::IsFinite(Health) && Health > 0.f && (!NarrativeASC || !NarrativeASC->IsDead())
		&& !AbilitySystemComponent->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
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

void USovPoiseComponent::HandleOwnerReadyEpochChanged(const int32 ReadyEpoch)
{
    const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
    if (!bEndingPlay && IsValid(ASC) && ASC->GetAvatarActor() == GetOwner()
        && ASC->GetCharacterReadyEpoch() == ReadyEpoch && ReadyEpoch != BoundReadyEpoch)
    {
        ++CheckpointRestoreGeneration;
        InitializeWithAbilitySystem(AbilitySystemComponent);
    }
}
