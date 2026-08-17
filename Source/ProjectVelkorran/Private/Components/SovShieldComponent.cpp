// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovShieldComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/SovCombatTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovShield, Log, All);

USovShieldComponent::USovShieldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void USovShieldComponent::BeginPlay()
{
	Super::BeginPlay();

	// Compatibility fallback for non-Narrative owners. The deterministic player
	// readiness path explicitly supplies the ASC and never polls.
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovShieldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	ClearLifecycleTimers();
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovShieldComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent))
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& ShieldChangedDelegateHandle.IsValid()
		&& MaxShieldChangedDelegateHandle.IsValid())
	{
		return true;
	}

	if (!InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		if (!bWarnedMissingAttributeSet)
		{
			UE_LOG(
				LogSovShield,
				Warning,
				TEXT("%s cannot initialize Shield: its Ability System has no UNarrativeAttributeSetBase."),
				*GetNameSafe(GetOwner()));
			bWarnedMissingAttributeSet = true;
		}
		return false;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	bWarnedMissingAttributeSet = false;

	ShieldBrokenTag = FSovGameplayTags::Get().State_Shield_Broken;
	RechargeBlockedTag = FSovGameplayTags::Get().State_Shield_RechargeBlocked;

	ShieldChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute())
		.AddUObject(this, &ThisClass::HandleShieldAttributeChanged);

	MaxShieldChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxShieldAttribute())
		.AddUObject(this, &ThisClass::HandleMaxShieldAttributeChanged);

	if (RechargeBlockedTag.IsValid())
	{
		RechargeBlockedTagChangedDelegateHandle = AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RechargeBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleRechargeBlockedTagChanged);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamageResolved);
	}

	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = true;
	LastShieldDamageWorldTime = GetWorldTimeSeconds();
	LastRechargeUpdateWorldTime = LastShieldDamageWorldTime;
	RefreshShieldBrokenState(GetShield(), false);

	TryStartRecharge();
	return true;
}

bool USovShieldComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
}

float USovShieldComponent::GetShield() const
{
	return IsInitialized()
		? AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute())
		: 0.0f;
}

float USovShieldComponent::GetMaxShield() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxShieldAttribute()),
			0.0f)
		: 0.0f;
}

bool USovShieldComponent::IsRechargeBlocked() const
{
	return IsInitialized()
		&& RechargeBlockedTag.IsValid()
		&& AbilitySystemComponent->HasMatchingGameplayTag(RechargeBlockedTag);
}

bool USovShieldComponent::IsRecharging() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(RechargeTimerHandle);
	}

	return false;
}

float USovShieldComponent::GetSecondsUntilRecharge() const
{
	if (!bHasRecordedShieldDamage || bRechargeDelayElapsed)
	{
		return 0.0f;
	}

	return FMath::Max(
		(LastShieldDamageWorldTime + FMath::Max(RechargeDelay, 0.0f)) - GetWorldTimeSeconds(),
		0.0f);
}

void USovShieldComponent::TryInitializeFromOwner()
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

void USovShieldComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovShieldComponent::UninitializeFromAbilitySystem()
{
	StopRecharge();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeDelayTimerHandle);
	}

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		ShieldChangedDelegateHandle.Reset();
		MaxShieldChangedDelegateHandle.Reset();
		RechargeBlockedTagChangedDelegateHandle.Reset();
		ShieldBrokenTag = FGameplayTag();
		RechargeBlockedTag = FGameplayTag();
		bShieldBroken = false;
		bAppliedShieldBrokenTag = false;
		bHasRecordedShieldDamage = false;
		bRechargeDelayElapsed = false;
		return;
	}

	if (ShieldChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetShieldAttribute())
			.Remove(ShieldChangedDelegateHandle);
	}

	if (MaxShieldChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxShieldAttribute())
			.Remove(MaxShieldChangedDelegateHandle);
	}

	if (RechargeBlockedTag.IsValid() && RechargeBlockedTagChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->RegisterGameplayTagEvent(
				RechargeBlockedTag,
				EGameplayTagEventType::NewOrRemoved)
			.Remove(RechargeBlockedTagChangedDelegateHandle);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamageResolved);
	}

	RemoveShieldBrokenTag();

	AbilitySystemComponent = nullptr;
	ShieldChangedDelegateHandle.Reset();
	MaxShieldChangedDelegateHandle.Reset();
	RechargeBlockedTagChangedDelegateHandle.Reset();
	ShieldBrokenTag = FGameplayTag();
	RechargeBlockedTag = FGameplayTag();
	bShieldBroken = false;
	bHasRecordedShieldDamage = false;
	bRechargeDelayElapsed = false;
}

void USovShieldComponent::ClearLifecycleTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RechargeDelayTimerHandle);
		TimerManager.ClearTimer(RechargeTimerHandle);
	}
}

void USovShieldComponent::HandleShieldAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	const float OldShield = FMath::Max(ChangeData.OldValue, 0.0f);
	const float NewShield = FMath::Clamp(ChangeData.NewValue, 0.0f, GetMaxShield());

	if (CanWriteShield() && NewShield + KINDA_SMALL_NUMBER < OldShield)
	{
		RecordShieldDamage();
	}

	RefreshShieldBrokenState(NewShield, true);
	OnShieldChanged.Broadcast(OldShield, NewShield, GetMaxShield());

	if (!CanWriteShield())
	{
		return;
	}

	if (NewShield + KINDA_SMALL_NUMBER >= GetMaxShield())
	{
		StopRecharge();
	}
	else if (NewShield > OldShield)
	{
		TryStartRecharge();
	}
}

void USovShieldComponent::HandleMaxShieldAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	const float CurrentShield = GetShield();
	const float CurrentMaxShield = FMath::Max(ChangeData.NewValue, 0.0f);

	RefreshShieldBrokenState(CurrentShield, false);
	OnShieldChanged.Broadcast(CurrentShield, CurrentShield, CurrentMaxShield);

	if (!CanWriteShield() || CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	TryStartRecharge();
}

void USovShieldComponent::HandleRechargeBlockedTagChanged(
	const FGameplayTag CallbackTag,
	const int32 NewCount)
{
	static_cast<void>(CallbackTag);

	if (!CanWriteShield())
	{
		return;
	}

	if (NewCount > 0)
	{
		StopRecharge();
	}
	else
	{
		TryStartRecharge();
	}
}

void USovShieldComponent::HandleDamageResolved(const FSovDamageResult& Result)
{
	// Actual Shield decreases are already observed by the attribute delegate.
	// This path covers hits against an already-depleted Shield and explicit
	// recharge-reset packets, which otherwise have no attribute transition.
	if (CanWriteShield()
		&& Result.bShouldRestartShieldRecharge
		&& Result.AppliedShieldDamage <= KINDA_SMALL_NUMBER)
	{
		RecordShieldDamage();
	}
}

void USovShieldComponent::RecordShieldDamage()
{
	if (!CanWriteShield())
	{
		return;
	}

	bHasRecordedShieldDamage = true;
	bRechargeDelayElapsed = false;
	LastShieldDamageWorldTime = GetWorldTimeSeconds();
	StopRecharge();
	ScheduleRechargeDelay(FMath::Max(RechargeDelay, 0.0f));
}

void USovShieldComponent::ScheduleRechargeDelay(const float DelaySeconds)
{
	if (!CanWriteShield())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RechargeDelayTimerHandle);

		if (DelaySeconds <= KINDA_SMALL_NUMBER)
		{
			HandleRechargeDelayElapsed();
			return;
		}

		TimerManager.SetTimer(
			RechargeDelayTimerHandle,
			this,
			&ThisClass::HandleRechargeDelayElapsed,
			DelaySeconds,
			false,
			DelaySeconds);
	}
}

void USovShieldComponent::HandleRechargeDelayElapsed()
{
	bRechargeDelayElapsed = true;
	TryStartRecharge();
}

void USovShieldComponent::TryStartRecharge()
{
	if (!CanWriteShield()
		|| IsRechargeBlocked()
		|| RechargePercentPerSecond <= 0.0f
		|| RechargeTimerInterval <= 0.0f)
	{
		StopRecharge();
		return;
	}

	const float CurrentShield = GetShield();
	const float CurrentMaxShield = GetMaxShield();
	if (CurrentMaxShield <= KINDA_SMALL_NUMBER
		|| CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	if (!bRechargeDelayElapsed)
	{
		const float RemainingDelay = bHasRecordedShieldDamage
			? (LastShieldDamageWorldTime + FMath::Max(RechargeDelay, 0.0f)) - GetWorldTimeSeconds()
			: 0.0f;

		if (RemainingDelay > KINDA_SMALL_NUMBER)
		{
			ScheduleRechargeDelay(RemainingDelay);
			return;
		}

		bRechargeDelayElapsed = true;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (TimerManager.IsTimerActive(RechargeTimerHandle))
		{
			return;
		}

		const float EffectiveInterval = FMath::Max(RechargeTimerInterval, 0.01f);
		LastRechargeUpdateWorldTime = GetWorldTimeSeconds();
		TimerManager.SetTimer(
			RechargeTimerHandle,
			this,
			&ThisClass::HandleRechargeTimerElapsed,
			EffectiveInterval,
			true,
			EffectiveInterval);
	}
}

void USovShieldComponent::HandleRechargeTimerElapsed()
{
	if (!CanWriteShield() || IsRechargeBlocked())
	{
		StopRecharge();
		return;
	}

	const float CurrentShield = GetShield();
	const float CurrentMaxShield = GetMaxShield();
	if (CurrentMaxShield <= KINDA_SMALL_NUMBER
		|| CurrentShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
		return;
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	const float RechargeSeconds = FMath::Max(
		CurrentWorldTime - LastRechargeUpdateWorldTime,
		0.0f);
	LastRechargeUpdateWorldTime = CurrentWorldTime;

	if (RechargeSeconds <= 0.0f)
	{
		return;
	}

	const float RechargeAmount = CurrentMaxShield
		* FMath::Max(RechargePercentPerSecond, 0.0f)
		* RechargeSeconds;
	const float NewShield = FMath::Min(CurrentShield + RechargeAmount, CurrentMaxShield);

	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetShieldAttribute(),
		NewShield);

	if (NewShield + KINDA_SMALL_NUMBER >= CurrentMaxShield)
	{
		StopRecharge();
	}
}

void USovShieldComponent::StopRecharge()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeTimerHandle);
	}
	LastRechargeUpdateWorldTime = 0.0f;
}

void USovShieldComponent::RefreshShieldBrokenState(
	const float CurrentShield,
	const bool bBroadcastBreak)
{
	const bool bNewShieldBroken = GetMaxShield() > KINDA_SMALL_NUMBER
		&& CurrentShield <= KINDA_SMALL_NUMBER;

	if (bNewShieldBroken == bShieldBroken)
	{
		return;
	}

	bShieldBroken = bNewShieldBroken;
	if (bShieldBroken)
	{
		ApplyShieldBrokenTag();
		if (bBroadcastBreak)
		{
			OnShieldBroken.Broadcast();
		}
	}
	else
	{
		RemoveShieldBrokenTag();
	}
}

void USovShieldComponent::ApplyShieldBrokenTag()
{
	if (!CanWriteShield()
		|| !ShieldBrokenTag.IsValid()
		|| bAppliedShieldBrokenTag)
	{
		return;
	}

	AbilitySystemComponent->AddLooseGameplayTag(
		ShieldBrokenTag,
		1,
		EGameplayTagReplicationState::TagAndCountToAll);
	bAppliedShieldBrokenTag = true;
}

void USovShieldComponent::RemoveShieldBrokenTag()
{
	if (!IsValid(AbilitySystemComponent)
		|| !ShieldBrokenTag.IsValid()
		|| !bAppliedShieldBrokenTag)
	{
		return;
	}

	AbilitySystemComponent->RemoveLooseGameplayTag(
		ShieldBrokenTag,
		1,
		EGameplayTagReplicationState::TagAndCountToAll);
	bAppliedShieldBrokenTag = false;
}

bool USovShieldComponent::CanWriteShield() const
{
	return IsInitialized() && IsValid(GetOwner()) && GetOwner()->HasAuthority();
}

float USovShieldComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}
