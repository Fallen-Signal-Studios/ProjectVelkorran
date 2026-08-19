// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovHealthRechargeComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameplayEffectTypes.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovHealthRecharge, Log, All);

USovHealthRechargeComponent::USovHealthRechargeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void USovHealthRechargeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovHealthRechargeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}

	ClearLifecycleTimers();
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovHealthRechargeComponent::InitializeWithAbilitySystem(
	UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent)
		|| !IsValid(GetOwner())
		|| !GetOwner()->IsA<ANarrativePlayerCharacter>())
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& HealthChangedDelegateHandle.IsValid()
		&& MaxHealthChangedDelegateHandle.IsValid())
	{
		return true;
	}

	if (!InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		if (!bWarnedMissingAttributeSet)
		{
			UE_LOG(
				LogSovHealthRecharge,
				Warning,
				TEXT("%s cannot initialize Health recharge: its Ability System has no UNarrativeAttributeSetBase."),
				*GetNameSafe(GetOwner()));
			bWarnedMissingAttributeSet = true;
		}
		return false;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	bWarnedMissingAttributeSet = false;

	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
		.AddUObject(this, &ThisClass::HandleHealthAttributeChanged);

	MaxHealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxHealthAttribute())
		.AddUObject(this, &ThisClass::HandleMaxHealthAttributeChanged);

	if (UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.AddUniqueDynamic(
			this,
			&ThisClass::HandleDamageResolved);
		NarrativeASC->OnDeathStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}

	bHasRecordedAppliedHit = false;
	bRechargeDelayElapsed = true;
	LastAppliedHitWorldTime = GetWorldTimeSeconds();
	LastRechargeUpdateWorldTime = LastAppliedHitWorldTime;
	TryStartRecharge();
	return true;
}

bool USovHealthRechargeComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
}

float USovHealthRechargeComponent::GetHealth() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetHealthAttribute()),
			0.0f)
		: 0.0f;
}

float USovHealthRechargeComponent::GetMaxHealth() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(
				UNarrativeAttributeSetBase::GetMaxHealthAttribute()),
			0.0f)
		: 0.0f;
}

bool USovHealthRechargeComponent::IsRecharging() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetTimerManager().IsTimerActive(RechargeTimerHandle);
	}

	return false;
}

float USovHealthRechargeComponent::GetSecondsUntilRecharge() const
{
	if (!bHasRecordedAppliedHit || bRechargeDelayElapsed)
	{
		return 0.0f;
	}

	return FMath::Max(
		(LastAppliedHitWorldTime + FMath::Max(RechargeDelay, 0.0f))
			- GetWorldTimeSeconds(),
		0.0f);
}

void USovHealthRechargeComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	if (UAbilitySystemComponent* OwnerASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		if (OwnerASC != AbilitySystemComponent)
		{
			InitializeWithAbilitySystem(OwnerASC);
		}
	}
}

void USovHealthRechargeComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovHealthRechargeComponent::UninitializeFromAbilitySystem()
{
	ClearLifecycleTimers();

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		HealthChangedDelegateHandle.Reset();
		MaxHealthChangedDelegateHandle.Reset();
		bHasRecordedAppliedHit = false;
		bRechargeDelayElapsed = false;
		return;
	}

	if (HealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetHealthAttribute())
			.Remove(HealthChangedDelegateHandle);
	}

	if (MaxHealthChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxHealthAttribute())
			.Remove(MaxHealthChangedDelegateHandle);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.RemoveDynamic(
			this,
			&ThisClass::HandleDamageResolved);
		NarrativeASC->OnDeathStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}

	AbilitySystemComponent = nullptr;
	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
	bHasRecordedAppliedHit = false;
	bRechargeDelayElapsed = false;
}

void USovHealthRechargeComponent::ClearLifecycleTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(RechargeDelayTimerHandle);
		TimerManager.ClearTimer(RechargeTimerHandle);
	}
	LastRechargeUpdateWorldTime = 0.0f;
}

void USovHealthRechargeComponent::HandleHealthAttributeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	const float CurrentMaxHealth = GetMaxHealth();
	const float OldHealth = FMath::Clamp(ChangeData.OldValue, 0.0f, CurrentMaxHealth);
	const float NewHealth = FMath::Clamp(ChangeData.NewValue, 0.0f, CurrentMaxHealth);

	OnHealthChanged.Broadcast(OldHealth, NewHealth, CurrentMaxHealth);
	if (!CanWriteHealth())
	{
		return;
	}

	if (NewHealth <= KINDA_SMALL_NUMBER)
	{
		ClearLifecycleTimers();
		return;
	}

	if (NewHealth + KINDA_SMALL_NUMBER >= CurrentMaxHealth)
	{
		ResetAtFullHealth();
		return;
	}

	TryStartRecharge();
}

void USovHealthRechargeComponent::HandleMaxHealthAttributeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	static_cast<void>(ChangeData.OldValue);

	const float CurrentMaxHealth = FMath::Max(ChangeData.NewValue, 0.0f);
	const float CurrentHealth = FMath::Clamp(GetHealth(), 0.0f, CurrentMaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, CurrentHealth, CurrentMaxHealth);

	if (!CanWriteHealth())
	{
		return;
	}

	if (CurrentHealth <= KINDA_SMALL_NUMBER)
	{
		ClearLifecycleTimers();
	}
	else if (CurrentHealth + KINDA_SMALL_NUMBER >= CurrentMaxHealth)
	{
		ResetAtFullHealth();
	}
	else
	{
		TryStartRecharge();
	}
}

void USovHealthRechargeComponent::HandleDamageResolved(
	const FSovDamageResult& Result)
{
	if (!CanWriteHealth()
		|| Result.TargetActor.Get() != GetOwner()
		|| GetHealth() <= KINDA_SMALL_NUMBER
		|| GetHealth() + KINDA_SMALL_NUMBER >= GetMaxHealth())
	{
		return;
	}

	const float AppliedHitMagnitude = Result.AppliedShieldDamage
		+ Result.AppliedHealthDamage
		+ Result.AppliedPoiseDamage
		+ Result.AppliedStaminaDamage;
	if (AppliedHitMagnitude > KINDA_SMALL_NUMBER)
	{
		RecordAppliedHit();
	}
}

void USovHealthRechargeComponent::HandleDeathStateChanged(
	AActor* KilledActor,
	UNarrativeAbilitySystemComponent* KilledActorASC,
	const bool bIsDead)
{
	if (KilledActor != GetOwner() || KilledActorASC != AbilitySystemComponent.Get())
	{
		return;
	}

	if (bIsDead)
	{
		ClearLifecycleTimers();
		bHasRecordedAppliedHit = false;
		bRechargeDelayElapsed = false;
		LastAppliedHitWorldTime = 0.0f;
	}
	else
	{
		bHasRecordedAppliedHit = false;
		bRechargeDelayElapsed = true;
		TryStartRecharge();
	}
}

void USovHealthRechargeComponent::RecordAppliedHit()
{
	if (!CanWriteHealth()
		|| GetHealth() <= KINDA_SMALL_NUMBER
		|| GetHealth() + KINDA_SMALL_NUMBER >= GetMaxHealth())
	{
		return;
	}

	bHasRecordedAppliedHit = true;
	bRechargeDelayElapsed = false;
	LastAppliedHitWorldTime = GetWorldTimeSeconds();
	StopRecharge();
	ScheduleRechargeDelay(FMath::Max(RechargeDelay, 0.0f));
}

void USovHealthRechargeComponent::ScheduleRechargeDelay(const float DelaySeconds)
{
	if (!CanWriteHealth())
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

void USovHealthRechargeComponent::HandleRechargeDelayElapsed()
{
	bRechargeDelayElapsed = true;
	TryStartRecharge();
}

void USovHealthRechargeComponent::TryStartRecharge()
{
	if (!CanWriteHealth()
		|| RechargePercentPerSecond <= 0.0f
		|| RechargeTimerInterval <= 0.0f)
	{
		StopRecharge();
		return;
	}

	const float CurrentHealth = GetHealth();
	const float CurrentMaxHealth = GetMaxHealth();
	if (CurrentMaxHealth <= KINDA_SMALL_NUMBER
		|| CurrentHealth <= KINDA_SMALL_NUMBER
		|| CurrentHealth + KINDA_SMALL_NUMBER >= CurrentMaxHealth)
	{
		StopRecharge();
		return;
	}

	if (!bRechargeDelayElapsed)
	{
		const float RemainingDelay = bHasRecordedAppliedHit
			? (LastAppliedHitWorldTime + FMath::Max(RechargeDelay, 0.0f))
				- GetWorldTimeSeconds()
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

void USovHealthRechargeComponent::HandleRechargeTimerElapsed()
{
	if (!CanWriteHealth())
	{
		StopRecharge();
		return;
	}

	const float CurrentHealth = GetHealth();
	const float CurrentMaxHealth = GetMaxHealth();
	if (CurrentMaxHealth <= KINDA_SMALL_NUMBER
		|| CurrentHealth <= KINDA_SMALL_NUMBER
		|| CurrentHealth + KINDA_SMALL_NUMBER >= CurrentMaxHealth)
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

	const float RechargeAmount = CurrentMaxHealth
		* FMath::Max(RechargePercentPerSecond, 0.0f)
		* RechargeSeconds;
	const float NewHealth = FMath::Min(
		CurrentHealth + RechargeAmount,
		CurrentMaxHealth);

	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetHealthAttribute(),
		NewHealth);

	if (NewHealth + KINDA_SMALL_NUMBER >= CurrentMaxHealth)
	{
		ResetAtFullHealth();
	}
}

void USovHealthRechargeComponent::StopRecharge()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeTimerHandle);
	}
	LastRechargeUpdateWorldTime = 0.0f;
}

void USovHealthRechargeComponent::ResetAtFullHealth()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RechargeDelayTimerHandle);
	}
	StopRecharge();
	bHasRecordedAppliedHit = false;
	bRechargeDelayElapsed = true;
}

bool USovHealthRechargeComponent::CanWriteHealth() const
{
	if (!IsInitialized() || !IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	return !NarrativeASC || !NarrativeASC->IsDead();
}

float USovHealthRechargeComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}
