// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovEchoComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovEcho, Log, All);

USovEchoComponent::USovEchoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(false);
}

void USovEchoComponent::BeginPlay()
{
	Super::BeginPlay();

	LastActivityWorldTime = GetWorldTimeSeconds();
	LastDecayUpdateWorldTime = LastActivityWorldTime;
	TryInitializeFromOwner();
}

void USovEchoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void USovEchoComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsInitialized())
	{
		TryInitializeFromOwner();
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	if (!CanWriteEcho() || !bEncounterActive || DecayRate <= 0.0f)
	{
		LastDecayUpdateWorldTime = CurrentWorldTime;
		return;
	}

	const float DecayStartTime = LastActivityWorldTime + FMath::Max(InactivityDelay, 0.0f);
	if (CurrentWorldTime <= DecayStartTime)
	{
		LastDecayUpdateWorldTime = CurrentWorldTime;
		return;
	}

	const float CurrentEcho = GetEcho();
	const float EffectiveFloor = FMath::Clamp(DecayFloor, 0.0f, GetMaxEcho());
	if (CurrentEcho <= EffectiveFloor + KINDA_SMALL_NUMBER)
	{
		LastDecayUpdateWorldTime = CurrentWorldTime;
		return;
	}

	const float EffectiveDecayStart = FMath::Max(DecayStartTime, LastDecayUpdateWorldTime);
	const float DecaySeconds = FMath::Max(CurrentWorldTime - EffectiveDecayStart, 0.0f);
	LastDecayUpdateWorldTime = CurrentWorldTime;

	if (DecaySeconds > 0.0f)
	{
		SetEchoInternal(FMath::Max(EffectiveFloor, CurrentEcho - (DecayRate * DecaySeconds)));
	}
}

bool USovEchoComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent))
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& EchoChangedDelegateHandle.IsValid()
		&& MaxEchoChangedDelegateHandle.IsValid())
	{
		return true;
	}

	if (!InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		if (!bWarnedMissingAttributeSet)
		{
			UE_LOG(
				LogSovEcho,
				Warning,
				TEXT("%s cannot initialize Echo: its Ability System has no UNarrativeAttributeSetBase."),
				*GetNameSafe(GetOwner()));
			bWarnedMissingAttributeSet = true;
		}
		return false;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	bWarnedMissingAttributeSet = false;

	EchoChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetEchoAttribute())
		.AddUObject(this, &ThisClass::HandleEchoAttributeChanged);

	MaxEchoChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxEchoAttribute())
		.AddUObject(this, &ThisClass::HandleMaxEchoAttributeChanged);

	if (CanWriteEcho())
	{
		if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
		{
			NarrativeASC->OnDealtDamage.AddUniqueDynamic(this, &ThisClass::HandleDealtDamage);
			NarrativeASC->OnDamagedBy.AddUniqueDynamic(this, &ThisClass::HandleReceivedDamage);
		}
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	LastActivityWorldTime = CurrentWorldTime;
	LastDecayUpdateWorldTime = CurrentWorldTime;
	RefreshThresholdStates(GetEcho(), false);
	return true;
}

bool USovEchoComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
}

float USovEchoComponent::GetEcho() const
{
	return IsInitialized()
		? AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetEchoAttribute())
		: 0.0f;
}

float USovEchoComponent::GetMaxEcho() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxEchoAttribute()),
			0.0f)
		: 0.0f;
}

bool USovEchoComponent::CanAffordEcho(const float Cost) const
{
	return Cost >= 0.0f && IsInitialized() && GetEcho() + KINDA_SMALL_NUMBER >= Cost;
}

float USovEchoComponent::GetSecondsUntilDecay() const
{
	if (!bEncounterActive)
	{
		return 0.0f;
	}

	return FMath::Max(
		(LastActivityWorldTime + FMath::Max(InactivityDelay, 0.0f)) - GetWorldTimeSeconds(),
		0.0f);
}

float USovEchoComponent::AddEcho(const float Amount, const FGameplayTag& SourceTag)
{
	if (!CanWriteEcho() || Amount <= 0.0f)
	{
		return 0.0f;
	}

	RecordCombatActivity(SourceTag);

	const float OldEcho = GetEcho();
	SetEchoInternal(OldEcho + Amount);
	const float AppliedAmount = FMath::Max(GetEcho() - OldEcho, 0.0f);

	if (AppliedAmount > KINDA_SMALL_NUMBER)
	{
		OnEchoGranted.Broadcast(SourceTag, AppliedAmount, GetEcho());
	}

	return AppliedAmount;
}

bool USovEchoComponent::TrySpendEcho(const float Cost, const FGameplayTag& SpendTag)
{
	if (!CanWriteEcho() || Cost < 0.0f || !CanAffordEcho(Cost))
	{
		return false;
	}

	if (Cost <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	RecordCombatActivity(SpendTag);

	const float OldEcho = GetEcho();
	SetEchoInternal(OldEcho - Cost);
	const float AppliedCost = FMath::Max(OldEcho - GetEcho(), 0.0f);

	if (AppliedCost > KINDA_SMALL_NUMBER)
	{
		OnEchoSpent.Broadcast(SpendTag, AppliedCost, GetEcho());
	}

	return AppliedCost + KINDA_SMALL_NUMBER >= Cost;
}

float USovEchoComponent::RestoreEchoFromCheckpoint(const float AuthoredValue)
{
	if (!CanWriteEcho())
	{
		return GetEcho();
	}

	const float CurrentWorldTime = GetWorldTimeSeconds();
	LastActivityWorldTime = CurrentWorldTime;
	LastDecayUpdateWorldTime = CurrentWorldTime;
	SetEchoInternal(AuthoredValue);
	return GetEcho();
}

void USovEchoComponent::BeginEncounter()
{
	if (!CanWriteEcho())
	{
		return;
	}

	bEncounterActive = true;
	const float CurrentWorldTime = GetWorldTimeSeconds();
	LastActivityWorldTime = CurrentWorldTime;
	LastDecayUpdateWorldTime = CurrentWorldTime;
}

float USovEchoComponent::EndEncounter(const float ReserveOverride)
{
	if (!CanWriteEcho())
	{
		return GetEcho();
	}

	bEncounterActive = false;
	LastActivityTag = FGameplayTag();

	const float CurrentWorldTime = GetWorldTimeSeconds();
	LastActivityWorldTime = CurrentWorldTime;
	LastDecayUpdateWorldTime = CurrentWorldTime;

	const float Reserve = ReserveOverride >= 0.0f
		? ReserveOverride
		: DefaultEncounterReserve;

	SetEchoInternal(Reserve);
	return GetEcho();
}

void USovEchoComponent::RecordCombatActivity(const FGameplayTag& ActivityTag)
{
	if (!CanWriteEcho())
	{
		return;
	}

	bEncounterActive = true;
	LastActivityTag = ActivityTag;

	const float CurrentWorldTime = GetWorldTimeSeconds();
	LastActivityWorldTime = CurrentWorldTime;
	LastDecayUpdateWorldTime = CurrentWorldTime;
}

void USovEchoComponent::TryInitializeFromOwner()
{
	if (IsInitialized() || !IsValid(GetOwner()))
	{
		return;
	}

	if (UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		InitializeWithAbilitySystem(OwnerASC);
	}
}

void USovEchoComponent::UninitializeFromAbilitySystem()
{
	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		EchoChangedDelegateHandle.Reset();
		MaxEchoChangedDelegateHandle.Reset();
		return;
	}

	if (EchoChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetEchoAttribute())
			.Remove(EchoChangedDelegateHandle);
	}

	if (MaxEchoChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(UNarrativeAttributeSetBase::GetMaxEchoAttribute())
			.Remove(MaxEchoChangedDelegateHandle);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDealtDamage.RemoveDynamic(this, &ThisClass::HandleDealtDamage);
		NarrativeASC->OnDamagedBy.RemoveDynamic(this, &ThisClass::HandleReceivedDamage);
	}

	AbilitySystemComponent = nullptr;
	EchoChangedDelegateHandle.Reset();
	MaxEchoChangedDelegateHandle.Reset();
}

void USovEchoComponent::SetEchoInternal(const float NewEcho)
{
	if (!CanWriteEcho())
	{
		return;
	}

	const float ClampedEcho = FMath::Clamp(NewEcho, 0.0f, GetMaxEcho());
	AbilitySystemComponent->SetNumericAttributeBase(
		UNarrativeAttributeSetBase::GetEchoAttribute(),
		ClampedEcho);
}

void USovEchoComponent::RefreshThresholdStates(const float CurrentEcho, const bool bBroadcastChanges)
{
	const bool bNewResonant = CurrentEcho + KINDA_SMALL_NUMBER >= ResonantThreshold;
	const bool bNewSignatureReady = CurrentEcho + KINDA_SMALL_NUMBER >= SignatureReadyThreshold;

	if (bNewResonant != bIsResonant)
	{
		bIsResonant = bNewResonant;
		if (bBroadcastChanges)
		{
			OnResonantStateChanged.Broadcast(bIsResonant, CurrentEcho);
		}
	}

	if (bNewSignatureReady != bIsSignatureReady)
	{
		bIsSignatureReady = bNewSignatureReady;
		if (bBroadcastChanges)
		{
			OnSignatureReadyStateChanged.Broadcast(bIsSignatureReady, CurrentEcho);
		}
	}
}

bool USovEchoComponent::CanWriteEcho() const
{
	return IsInitialized() && IsValid(GetOwner()) && GetOwner()->HasAuthority();
}

float USovEchoComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void USovEchoComponent::HandleEchoAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	OnEchoChanged.Broadcast(ChangeData.OldValue, ChangeData.NewValue, GetMaxEcho());
	RefreshThresholdStates(ChangeData.NewValue, true);
}

void USovEchoComponent::HandleMaxEchoAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	const float CurrentEcho = GetEcho();
	OnEchoChanged.Broadcast(CurrentEcho, CurrentEcho, ChangeData.NewValue);
	RefreshThresholdStates(CurrentEcho, true);
}

void USovEchoComponent::HandleDealtDamage(
	UNarrativeAbilitySystemComponent* DamagedAbilitySystem,
	const float Damage,
	const FGameplayEffectSpec& EffectSpec)
{
	static_cast<void>(DamagedAbilitySystem);
	static_cast<void>(EffectSpec);

	if (Damage > KINDA_SMALL_NUMBER)
	{
		RecordCombatActivity(FGameplayTag());
	}
}

void USovEchoComponent::HandleReceivedDamage(
	UNarrativeAbilitySystemComponent* DamageSourceAbilitySystem,
	const float Damage,
	const FGameplayEffectSpec& EffectSpec)
{
	static_cast<void>(DamageSourceAbilitySystem);
	static_cast<void>(EffectSpec);

	if (Damage > KINDA_SMALL_NUMBER)
	{
		// Receiving damage participates in the inactivity clock, but intentionally
		// grants no Echo. Protagonist rules decide whether a guarded/intercepted hit
		// deserves a separate award.
		RecordCombatActivity(FGameplayTag());
	}
}
