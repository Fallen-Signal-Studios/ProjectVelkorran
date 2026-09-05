// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovGuardComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovEchoComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"
#include "Settings/NarrativeCombatDeveloperSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovGuard, Log, All);

USovGuardComponent::USovGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	GuardImpactGameplayCueTag = FGameplayTag::RequestGameplayTag(
		FName(TEXT("GameplayCue.TakeDamage.Blocked")),
		false);
}

void USovGuardComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GuardImpactGameplayCueTag.IsValid())
	{
		GuardImpactGameplayCueTag = FGameplayTag::RequestGameplayTag(
			FName(TEXT("GameplayCue.TakeDamage.Blocked")),
			false);
	}
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovGuardComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOwnerASCInitialized);
	}
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovGuardComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (bUninitializing
		|| !IsValid(InAbilitySystemComponent)
		|| !IsValid(GetOwner())
		|| !InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		return false;
	}

	const USovGuardComponent* CanonicalGuard =
		GetOwner()->FindComponentByClass<USovGuardComponent>();
	if (const ASovPlayerCharacterBase* PlayerOwner =
		Cast<ASovPlayerCharacterBase>(GetOwner()))
	{
		CanonicalGuard = PlayerOwner->GetGuardComponent();
	}
	if (CanonicalGuard != this)
	{
		UE_LOG(
			LogSovGuard,
			Error,
			TEXT("%s has a non-canonical or duplicate Guard component. Ignoring %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(this));
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		return true;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	BindInterruptionTags();
	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamageResolvedAsTarget);
		NarrativeASC->OnDamageResolvedAsSource.AddUniqueDynamic(this, &ThisClass::HandleDamageResolvedAsSource);
	}
	return true;
}

bool USovGuardComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
}

bool USovGuardComponent::BeginGuard()
{
	return BeginGuardInternal(0);
}

bool USovGuardComponent::BeginGuardInternal(const int32 OwnedBusyContributions)
{
	if (!IsInitialized() || bEndingGuard || bUninitializing || !GetWorld()
		|| HasGuardInterruptState(OwnedBusyContributions))
	{
		return false;
	}

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	const UNarrativeCombatDeveloperSettings* CombatSettings =
		GetDefault<UNarrativeCombatDeveloperSettings>();
	const float MinimumStartStamina = CombatSettings
		? FMath::Max(CombatSettings->MinimumGuardStartStamina, 0.f)
		: 8.f;
	const float CurrentStamina = AbilitySystemComponent->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetStaminaAttribute());
	if (AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Guarding)
		|| AbilitySystemComponent->HasMatchingGameplayTag(Tags.State_Guard_Broken)
		|| CurrentStamina + KINDA_SMALL_NUMBER < MinimumStartStamina)
	{
		return false;
	}

	const uint32 Epoch = ++GuardEpoch;
	GuardOwnedBusyContributions = OwnedBusyContributions;
	SetOwnedLooseTag(Tags.State_Guarding, true, bAppliedGuardingTag);
	if (GuardEpoch != Epoch || !bAppliedGuardingTag) { return false; }
	SetOwnedLooseTag(Tags.State_PerfectGuard, true, bAppliedPerfectDefenseTag);
	if (GuardEpoch != Epoch || !bAppliedGuardingTag) { return false; }

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(PerfectDefenseTimerHandle);
		if (PerfectDefenseWindow <= KINDA_SMALL_NUMBER)
		{
			ClosePerfectDefenseWindow();
		}
		else
		{
			TimerManager.SetTimer(
				PerfectDefenseTimerHandle,
				this,
				&ThisClass::ClosePerfectDefenseWindow,
				PerfectDefenseWindow,
				false);
		}
	}

	OnGuardStarted.Broadcast();
	return GuardEpoch == Epoch && bAppliedGuardingTag;
}

void USovGuardComponent::EndGuard()
{
	if (bEndingGuard) { return; }
	TGuardValue<bool> EndingGuard(bEndingGuard, true);
	++GuardEpoch;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PerfectDefenseTimerHandle);
	}

	const bool bWasGuarding = bAppliedGuardingTag || bAppliedPerfectDefenseTag;
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	SetOwnedLooseTag(Tags.State_PerfectGuard, false, bAppliedPerfectDefenseTag);
	SetOwnedLooseTag(Tags.State_Guarding, false, bAppliedGuardingTag);
	GuardOwnedBusyContributions = 0;
	if (bWasGuarding)
	{
		OnGuardEnded.Broadcast();
	}
}

bool USovGuardComponent::IsGuarding() const
{
	return IsInitialized()
		&& AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Guarding);
}

bool USovGuardComponent::IsPerfectDefenseWindowOpen() const
{
	return IsInitialized()
		&& AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_PerfectGuard);
}

bool USovGuardComponent::IsCounterWindowOpen() const
{
	return IsInitialized()
		&& AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Guard_CounterWindow);
}

void USovGuardComponent::TryInitializeFromOwner()
{
	if (IsValid(GetOwner()))
	{
		if (UAbilitySystemComponent* OwnerASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
		{
			if (OwnerASC != AbilitySystemComponent)
			{
				InitializeWithAbilitySystem(OwnerASC);
			}
		}
	}
}

void USovGuardComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovGuardComponent::UninitializeFromAbilitySystem()
{
	if (bUninitializing) { return; }
	TGuardValue<bool> Uninitializing(bUninitializing, true);
	UnbindInterruptionTags();
	EndGuard();
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(PerfectDefenseTimerHandle);
		TimerManager.ClearTimer(CounterWindowTimerHandle);
		TimerManager.ClearTimer(GuardBrokenTimerHandle);
		TimerManager.ClearTimer(GuardBrokenBroadcastTimerHandle);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamageResolvedAsTarget);
		NarrativeASC->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::HandleDamageResolvedAsSource);
	}

	if (IsValid(AbilitySystemComponent))
	{
		const FSovGameplayTags& Tags = FSovGameplayTags::Get();
		SetOwnedLooseTag(Tags.State_PerfectGuard, false, bAppliedPerfectDefenseTag);
		SetOwnedLooseTag(Tags.State_Guarding, false, bAppliedGuardingTag);
		SetOwnedLooseTag(Tags.State_Guard_CounterWindow, false, bAppliedCounterWindowTag);
		SetOwnedLooseTag(Tags.State_Guard_Broken, false, bAppliedGuardBrokenTag);
	}

	AbilitySystemComponent = nullptr;
	bAppliedGuardingTag = false;
	bAppliedPerfectDefenseTag = false;
	bAppliedCounterWindowTag = false;
	bAppliedGuardBrokenTag = false;
}

bool USovGuardComponent::HasGuardInterruptState(const int32 OwnedBusyContributions) const
{
	if (!IsInitialized()) { return true; }
	const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& Sov = FSovGameplayTags::Get();
	FGameplayTagContainer Blocking;
	Blocking.AddTag(Narrative.State_IsDead);
	Blocking.AddTag(Narrative.State_Interacting);
	Blocking.AddTag(Narrative.State_SequencerControlled);
	Blocking.AddTag(Narrative.State_Movement_Ragdoll);
	Blocking.AddTag(Sov.State_Fatal);
	Blocking.AddTag(Sov.State_Poise_Broken);
	Blocking.AddTag(Sov.State_Guard_Broken);
	Blocking.AddTag(Sov.State_EchoAbility_Active);
	Blocking.AddTag(Sov.State_Deflecting);
	return AbilitySystemComponent->HasAnyMatchingGameplayTags(Blocking)
		|| AbilitySystemComponent->GetGameplayTagCount(Narrative.State_Busy) > OwnedBusyContributions;
}

void USovGuardComponent::BindInterruptionTags()
{
	UnbindInterruptionTags();
	if (!IsInitialized()) { return; }
	const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& Sov = FSovGameplayTags::Get();
	const FGameplayTag Tags[] = {
		Narrative.State_IsDead, Narrative.State_Interacting,
		Narrative.State_SequencerControlled, Narrative.State_Movement_Ragdoll,
		Sov.State_Fatal, Sov.State_Poise_Broken, Sov.State_Guard_Broken,
		Sov.State_EchoAbility_Active, Sov.State_Deflecting };
	for (const FGameplayTag& Tag : Tags)
	{
		InterruptionTagHandles.Add(Tag, AbilitySystemComponent
			->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleInterruptionTagChanged));
	}
	BusyTagChangedHandle = AbilitySystemComponent
		->RegisterGameplayTagEvent(Narrative.State_Busy, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &ThisClass::HandleInterruptionTagChanged);
}

void USovGuardComponent::UnbindInterruptionTags()
{
	if (IsInitialized())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Entry : InterruptionTagHandles)
		{
			AbilitySystemComponent->RegisterGameplayTagEvent(Entry.Key, EGameplayTagEventType::NewOrRemoved)
				.Remove(Entry.Value);
		}
		AbilitySystemComponent
			->RegisterGameplayTagEvent(FNarrativeGameplayTags::Get().State_Busy, EGameplayTagEventType::AnyCountChange)
			.Remove(BusyTagChangedHandle);
	}
	InterruptionTagHandles.Reset();
	BusyTagChangedHandle.Reset();
}

void USovGuardComponent::HandleInterruptionTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	if (NewCount > 0 && HasGuardInterruptState(GuardOwnedBusyContributions))
	{
		// Direct Blueprint entry and GAS entry share the same interruption contract.
		// A counter attack may itself become Busy. Preserve its short opportunity
		// through that transition, but never through incapacity or another defense.
		if (Tag != FNarrativeGameplayTags::Get().State_Busy) { CloseCounterWindow(); }
		EndGuard();
	}
}

void USovGuardComponent::ClosePerfectDefenseWindow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PerfectDefenseTimerHandle);
	}

	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_PerfectGuard,
		false,
		bAppliedPerfectDefenseTag);
}

void USovGuardComponent::OpenCounterWindow()
{
	if (!IsInitialized() || !GetOwner()->HasAuthority() || !bAppliedGuardingTag
		|| HasGuardInterruptState(GuardOwnedBusyContributions)
		|| AbilitySystemComponent->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()) <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_Guard_CounterWindow,
		true,
		bAppliedCounterWindowTag);
	if (!bAppliedCounterWindowTag) { return; }

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(CounterWindowTimerHandle);
		if (CounterWindowDuration <= KINDA_SMALL_NUMBER)
		{
			CloseCounterWindow();
		}
		else
		{
			TimerManager.SetTimer(
				CounterWindowTimerHandle,
				this,
				&ThisClass::CloseCounterWindow,
				CounterWindowDuration,
				false);
		}
	}
	// Arm expiry before dispatch. A gameplay-event listener can immediately
	// consume the window, cancel Guard, or uninitialize this component.
	if (bAppliedCounterWindowTag)
	{
		FGameplayEventData CounterPayload;
		CounterPayload.EventTag = FSovGameplayTags::Get().Event_Guard_CounterWindowOpened;
		CounterPayload.Instigator = GetOwner();
		CounterPayload.Target = GetOwner();
		CounterPayload.EventMagnitude = CounterWindowDuration;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), CounterPayload.EventTag, CounterPayload);
	}
}

bool USovGuardComponent::ExtendCounterWindow(float AdditionalSeconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !bAppliedCounterWindowTag
		|| !IsCounterWindowOpen() || !FMath::IsFinite(AdditionalSeconds) || AdditionalSeconds <= 0.f) { return false; }
	auto& Timers = GetWorld()->GetTimerManager();
	const float Remaining = Timers.GetTimerRemaining(CounterWindowTimerHandle);
	if (Remaining <= 0.f) { return false; }
	Timers.SetTimer(CounterWindowTimerHandle, this, &ThisClass::CloseCounterWindow,
		Remaining + FMath::Min(AdditionalSeconds, 3.f), false);
	return true;
}

void USovGuardComponent::CloseCounterWindow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CounterWindowTimerHandle);
	}

	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_Guard_CounterWindow,
		false,
		bAppliedCounterWindowTag);
}

void USovGuardComponent::ClearGuardBrokenState()
{
	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_Guard_Broken,
		false,
		bAppliedGuardBrokenTag);
}

void USovGuardComponent::BroadcastPendingGuardBroken()
{
	MulticastGuardBroken(PendingGuardBrokenResult);
}

void USovGuardComponent::SetOwnedLooseTag(
	const FGameplayTag& Tag,
	const bool bShouldApply,
	bool& bAppliedFlag)
{
	if (!IsValid(AbilitySystemComponent) || !Tag.IsValid() || bShouldApply == bAppliedFlag)
	{
		return;
	}
	// GAS tag delegates fire synchronously. Publish ownership before dispatch so
	// a reentrant cancellation can remove the contribution currently being added.
	bAppliedFlag = bShouldApply;

	if (bShouldApply)
	{
		if (GetOwner() && GetOwner()->HasAuthority())
		{
			AbilitySystemComponent->AddLooseGameplayTag(
				Tag,
				1,
				EGameplayTagReplicationState::TagAndCountToAll);
		}
		else
		{
			AbilitySystemComponent->AddLooseGameplayTag(Tag);
		}
	}
	else if (GetOwner() && GetOwner()->HasAuthority())
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(
			Tag,
			1,
			EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		AbilitySystemComponent->RemoveLooseGameplayTag(Tag);
	}
}

void USovGuardComponent::HandleDamageResolvedAsTarget(const FSovDamageResult& Result)
{
	if (!IsInitialized() || Result.TargetActor != GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	USovEchoComponent* EchoComponent = GetOwner()->FindComponentByClass<USovEchoComponent>();
	if (Result.bPerfectDefense && Result.DefenseKind == ESovDefenseKind::Guard)
	{
		// Consume the timing state before gameplay cues can reenter damage routing.
		ClosePerfectDefenseWindow();
	}
	if (Result.bGuarded)
	{
		ExecuteGuardImpactGameplayCue(Result);
	}
	if (Result.bPerfectDefense && Result.DefenseKind == ESovDefenseKind::Guard)
	{
		if (EchoComponent)
		{
			EchoComponent->AddEcho(PerfectGuardEchoReward, FSovGameplayTags::Get().Echo_Source_PerfectGuard);
		}
		if (!Result.bGuardBroken)
		{
			OpenCounterWindow();
		}
		FGameplayEventData PerfectPayload;
		PerfectPayload.EventTag = FSovGameplayTags::Get().Event_Guard_Perfect;
		PerfectPayload.Instigator = Result.SourceActor;
		PerfectPayload.Target = GetOwner();
		PerfectPayload.ContextHandle = Result.EffectContext;
		PerfectPayload.EventMagnitude = Result.ResolvedDamage;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			GetOwner(),
			PerfectPayload.EventTag,
			PerfectPayload);
		MulticastPerfectDefense(Result);
	}
	else if (Result.bGuarded)
	{
		if (EchoComponent)
		{
			EchoComponent->RecordCombatActivity(FSovGameplayTags::Get().Echo_Source_GuardPressure);
		}
		MulticastGuardImpact(Result);
	}

	if (Result.bGuardBroken)
	{
		CloseCounterWindow();
		EndGuard();
		SetOwnedLooseTag(FSovGameplayTags::Get().State_Guard_Broken, true, bAppliedGuardBrokenTag);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				GuardBrokenTimerHandle,
				this,
				&ThisClass::ClearGuardBrokenState,
				FMath::Max(GuardBreakDuration, KINDA_SMALL_NUMBER),
				false);
		}
		FGameplayEventData BrokenPayload;
		BrokenPayload.EventTag = FSovGameplayTags::Get().Event_Guard_Broken;
		BrokenPayload.Instigator = Result.SourceActor;
		BrokenPayload.Target = GetOwner();
		BrokenPayload.ContextHandle = Result.EffectContext;
		BrokenPayload.EventMagnitude = Result.AppliedStaminaDamage;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
			GetOwner(),
			BrokenPayload.EventTag,
			BrokenPayload);
		PendingGuardBrokenResult = Result;
		if (UWorld* World = GetWorld())
		{
			const uint32 Epoch = GuardEpoch;
			World->GetTimerManager().ClearTimer(GuardBrokenBroadcastTimerHandle);
			GuardBrokenBroadcastTimerHandle = World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateWeakLambda(this, [this, Epoch]()
				{
					if (IsInitialized() && GuardEpoch == Epoch) { BroadcastPendingGuardBroken(); }
				}));
		}
	}
}

void USovGuardComponent::HandleDamageResolvedAsSource(const FSovDamageResult& Result)
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (!IsInitialized()
		|| Result.SourceActor != GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !bAppliedCounterWindowTag
		|| !IsCounterWindowOpen()
		|| !Result.AttackClassifications.HasTagExact(Tags.Damage_Source_GuardCounter)
		|| Result.AppliedShieldDamage + Result.AppliedHealthDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	// Consume before reward delegates. Reentrant damage cannot reward twice.
	CloseCounterWindow();
	if (USovEchoComponent* EchoComponent = GetOwner()->FindComponentByClass<USovEchoComponent>())
	{
		EchoComponent->AddEcho(GuardCounterEchoReward, Tags.Echo_Source_GuardCounter);
	}
	FGameplayEventData CounterPayload;
	CounterPayload.EventTag = Tags.Event_Guard_CounterConsumed;
	CounterPayload.Instigator = GetOwner();
	CounterPayload.Target = Result.TargetActor;
	CounterPayload.ContextHandle = Result.EffectContext;
	CounterPayload.EventMagnitude = GuardCounterEchoReward;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwner(),
		CounterPayload.EventTag,
		CounterPayload);
	MulticastCounterLanded(Result);
}

void USovGuardComponent::ExecuteGuardImpactGameplayCue(const FSovDamageResult& Result) const
{
	if (!bExecuteGuardImpactGameplayCue
		|| !IsValid(AbilitySystemComponent)
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !GuardImpactGameplayCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.EffectContext = Result.EffectContext;
	CueParameters.RawMagnitude = Result.ResolvedDamage;
	CueParameters.Instigator = Result.SourceActor;
	CueParameters.EffectCauser = Result.EffectContext.GetEffectCauser()
		? Result.EffectContext.GetEffectCauser()
		: Result.SourceActor.Get();
	CueParameters.AggregatedSourceTags = Result.AttackClassifications;
	CueParameters.AggregatedSourceTags.AppendTags(Result.DamageChannels);
	AbilitySystemComponent->ExecuteGameplayCue(GuardImpactGameplayCueTag, CueParameters);
}

void USovGuardComponent::MulticastGuardImpact_Implementation(
	const FSovDamageResult& Result)
{
	OnGuardImpact.Broadcast(Result);
}

void USovGuardComponent::MulticastPerfectDefense_Implementation(
	const FSovDamageResult& Result)
{
	// Remove the predicted timing tag immediately on the owning client. The
	// local timer remains a fallback if this cosmetic multicast is dropped.
	ClosePerfectDefenseWindow();
	OnPerfectDefense.Broadcast(Result);
}

void USovGuardComponent::MulticastGuardBroken_Implementation(
	const FSovDamageResult& Result)
{
	OnGuardBroken.Broadcast(Result);
}

void USovGuardComponent::MulticastCounterLanded_Implementation(
	const FSovDamageResult& Result)
{
	OnCounterLanded.Broadcast(Result);
}
