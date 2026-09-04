// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovEchoComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/SovEchoAwardPolicy.h"
#include "Abilities/SovGameplayAbility_SeleneEcho.h"
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "Items/WeaponItem.h"
#include "Sovereign/SovGameplayTags.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovEcho, Log, All);

USovEchoComponent::USovEchoComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(false);
}

void USovEchoComponent::BeginPlay()
{
	Super::BeginPlay();

	LastActivityWorldTime = GetWorldTimeSeconds();
	LastDecayUpdateWorldTime = LastActivityWorldTime;
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovEchoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void USovEchoComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Equipment/spec replication can change readiness without changing Echo.
	RefreshThresholdStates(GetEcho(), true);
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
	SetComponentTickEnabled(true);
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
	return FMath::IsFinite(Cost) && Cost >= 0.0f && IsInitialized() && GetEcho() + KINDA_SMALL_NUMBER >= Cost;
}

float USovEchoComponent::GetSignatureEchoRequirement() const
{
	if (!IsInitialized() || !IsValid(AbilitySystemComponent->GetOwnerActor())
		|| !IsValid(AbilitySystemComponent->GetAvatarActor())
		|| AbilitySystemComponent->GetAvatarActor() != GetOwner()) return -1.0f;
	FGameplayAbilityActorInfo ActorInfo;
	ActorInfo.InitFromActor(AbilitySystemComponent->GetOwnerActor(), AbilitySystemComponent->GetAvatarActor(), AbilitySystemComponent);
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	float Requirement = -1.0f;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		const USovGameplayAbility_EchoBase* Ability = Cast<USovGameplayAbility_EchoBase>(Spec.Ability);
		if (!Ability || Spec.PendingRemove || !Ability->CanUseEchoWeaponContext(Spec.Handle, &ActorInfo)) continue;
		const bool bTarrik = Ability->IsA<USovGameplayAbility_TarrikCinderlineRequiem>()
			&& AbilitySystemComponent->HasMatchingGameplayTag(Tags.Character_Player_Tarrik)
			&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.Character_Player_Selene);
		const bool bSelene = Ability->IsA<USovGameplayAbility_SeleneDispatch>()
			&& AbilitySystemComponent->HasMatchingGameplayTag(Tags.Character_Player_Selene)
			&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.Character_Player_Tarrik);
		if (!bTarrik && !bSelene) continue;
		const float Cost = Ability->GetEchoCost();
		const float Minimum = Ability->GetMinimumEchoRequired();
		if (!FMath::IsFinite(Cost) || !FMath::IsFinite(Minimum)) continue;
		const float Candidate = FMath::Max(Cost, Minimum);
		Requirement = Requirement < 0.0f ? Candidate : FMath::Min(Requirement, Candidate);
	}
	return Requirement;
}

bool USovEchoComponent::IsSignatureReady() const
{
	const float Requirement = GetSignatureEchoRequirement();
	return IsInitialized() && SovEchoAwardPolicy::IsMeterReady(GetEcho(), Requirement);
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
	if (!CanWriteEcho() || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	RecordCombatActivity(SourceTag);

	UAbilitySystemComponent* OriginalASC = AbilitySystemComponent.Get();
	const float OldEcho = GetEcho();
	const float CommittedEcho = FMath::Clamp(OldEcho + Amount, 0.0f, GetMaxEcho());
	const float AppliedAmount = FMath::Max(CommittedEcho - OldEcho, 0.0f);
	SetEchoInternal(CommittedEcho);
	// Attribute callbacks may spend/grant recursively. Report this write, not the nested net delta.
	if (AppliedAmount > KINDA_SMALL_NUMBER && CanWriteEcho() && AbilitySystemComponent.Get() == OriginalASC)
	{
		OnEchoGranted.Broadcast(SourceTag, AppliedAmount, CommittedEcho);
	}

	return AppliedAmount;
}

bool USovEchoComponent::TrySpendEcho(const float Cost, const FGameplayTag& SpendTag)
{
	if (!CanWriteEcho() || !FMath::IsFinite(Cost) || Cost < 0.0f || !CanAffordEcho(Cost))
	{
		return false;
	}

	if (Cost <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	RecordCombatActivity(SpendTag);

	UAbilitySystemComponent* OriginalASC = AbilitySystemComponent.Get();
	const float OldEcho = GetEcho();
	const float CommittedEcho = FMath::Clamp(OldEcho - Cost, 0.0f, GetMaxEcho());
	const float AppliedCost = FMath::Max(OldEcho - CommittedEcho, 0.0f);
	SetEchoInternal(CommittedEcho);
	if (AppliedCost > KINDA_SMALL_NUMBER && CanWriteEcho() && AbilitySystemComponent.Get() == OriginalASC)
	{
		OnEchoSpent.Broadcast(SpendTag, AppliedCost, CommittedEcho);
	}

	return AppliedCost + KINDA_SMALL_NUMBER >= Cost;
}

float USovEchoComponent::RestoreEchoFromCheckpoint(const float AuthoredValue)
{
	if (!CanWriteEcho() || !FMath::IsFinite(AuthoredValue))
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
	OnEncounterScopeChanged.Broadcast(true);
}

float USovEchoComponent::EndEncounter(const float ReserveOverride)
{
	if (!CanWriteEcho() || !FMath::IsFinite(ReserveOverride))
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
	OnEncounterScopeChanged.Broadcast(false);
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

void USovEchoComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovEchoComponent::UninitializeFromAbilitySystem()
{
	bIsResonant = false;
	bIsSignatureReady = false;
	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		EchoChangedDelegateHandle.Reset();
		MaxEchoChangedDelegateHandle.Reset();
		SetComponentTickEnabled(false);
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
	SetComponentTickEnabled(false);
}

void USovEchoComponent::SetEchoInternal(const float NewEcho)
{
	if (!CanWriteEcho() || !FMath::IsFinite(NewEcho))
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
	const bool bNewSignatureReady = IsSignatureReady();

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
	return IsInitialized() && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
		&& GetOwner()->HasAuthority() && AbilitySystemComponent->GetAvatarActor() == GetOwner();
}

float USovEchoComponent::GetWorldTimeSeconds() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.0f;
}

void USovEchoComponent::HandleEchoAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	OnEchoChanged.Broadcast(ChangeData.OldValue, ChangeData.NewValue, GetMaxEcho());
	RefreshThresholdStates(GetEcho(), true);
}

void USovEchoComponent::HandleMaxEchoAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	const float CurrentEcho = GetEcho();
	OnEchoChanged.Broadcast(CurrentEcho, CurrentEcho, ChangeData.NewValue);
	RefreshThresholdStates(GetEcho(), true);
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
