// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovGuardComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Components/SovEchoComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

USovGuardComponent::USovGuardComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void USovGuardComponent::BeginPlay()
{
	Super::BeginPlay();
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
	if (!IsValid(InAbilitySystemComponent)
		|| !InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		return true;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
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
	if (!IsInitialized()
		|| AbilitySystemComponent->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Guard_Broken))
	{
		return false;
	}

	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	SetOwnedLooseTag(Tags.State_Guarding, true, bAppliedGuardingTag);
	SetOwnedLooseTag(Tags.State_PerfectGuard, true, bAppliedPerfectDefenseTag);

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
	return true;
}

void USovGuardComponent::EndGuard()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PerfectDefenseTimerHandle);
	}

	const bool bWasGuarding = bAppliedGuardingTag || bAppliedPerfectDefenseTag;
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	SetOwnedLooseTag(Tags.State_PerfectGuard, false, bAppliedPerfectDefenseTag);
	SetOwnedLooseTag(Tags.State_Guarding, false, bAppliedGuardingTag);
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
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(PerfectDefenseTimerHandle);
		TimerManager.ClearTimer(CounterWindowTimerHandle);
		TimerManager.ClearTimer(GuardBrokenTimerHandle);
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

void USovGuardComponent::ClosePerfectDefenseWindow()
{
	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_PerfectGuard,
		false,
		bAppliedPerfectDefenseTag);
}

void USovGuardComponent::OpenCounterWindow()
{
	if (!IsInitialized() || !GetOwner()->HasAuthority())
	{
		return;
	}

	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_Guard_CounterWindow,
		true,
		bAppliedCounterWindowTag);

	FGameplayEventData CounterPayload;
	CounterPayload.EventTag = FSovGameplayTags::Get().Event_Guard_CounterWindowOpened;
	CounterPayload.Instigator = GetOwner();
	CounterPayload.Target = GetOwner();
	CounterPayload.EventMagnitude = CounterWindowDuration;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwner(),
		CounterPayload.EventTag,
		CounterPayload);

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
}

void USovGuardComponent::CloseCounterWindow()
{
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
	OnGuardBroken.Broadcast(PendingGuardBrokenResult);
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
	bAppliedFlag = bShouldApply;
}

void USovGuardComponent::HandleDamageResolvedAsTarget(const FSovDamageResult& Result)
{
	if (!IsInitialized() || Result.TargetActor != GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	USovEchoComponent* EchoComponent = GetOwner()->FindComponentByClass<USovEchoComponent>();
	if (Result.bPerfectDefense)
	{
		if (EchoComponent)
		{
			EchoComponent->AddEcho(PerfectGuardEchoReward, FSovGameplayTags::Get().Echo_Source_PerfectGuard);
		}
		OpenCounterWindow();
		OnPerfectDefense.Broadcast(Result);
	}
	else if (Result.bGuarded)
	{
		if (EchoComponent)
		{
			EchoComponent->RecordCombatActivity(FSovGameplayTags::Get().Echo_Source_GuardPressure);
		}
		OnGuardImpact.Broadcast(Result);
	}

	if (Result.bGuardBroken)
	{
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
		PendingGuardBrokenResult = Result;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ThisClass::BroadcastPendingGuardBroken));
		}
	}
}

void USovGuardComponent::HandleDamageResolvedAsSource(const FSovDamageResult& Result)
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	if (!IsInitialized()
		|| Result.SourceActor != GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !IsCounterWindowOpen()
		|| !Result.AttackClassifications.HasTagExact(Tags.Damage_Source_GuardCounter)
		|| Result.AppliedShieldDamage + Result.AppliedHealthDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (USovEchoComponent* EchoComponent = GetOwner()->FindComponentByClass<USovEchoComponent>())
	{
		EchoComponent->AddEcho(GuardCounterEchoReward, Tags.Echo_Source_GuardCounter);
	}
	CloseCounterWindow();
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
	OnCounterLanded.Broadcast(Result);
}
