// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovDeflectionComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Actor.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovDeflection, Log, All);

USovDeflectionComponent::USovDeflectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USovDeflectionComponent::BeginPlay()
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

void USovDeflectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}

	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovDeflectionComponent::InitializeWithAbilitySystem(
	UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent)
		|| !IsValid(GetOwner())
		|| !InAbilitySystemComponent->GetSet<UNarrativeAttributeSetBase>())
	{
		return false;
	}

	const USovDeflectionComponent* CanonicalDeflection =
		GetOwner()->FindComponentByClass<USovDeflectionComponent>();
	if (CanonicalDeflection != this)
	{
		UE_LOG(
			LogSovDeflection,
			Error,
			TEXT("%s has a duplicate Deflection component. Ignoring %s."),
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
	if (UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.AddUniqueDynamic(
			this,
			&ThisClass::HandleDamageResolvedAsTarget);
	}
	return true;
}

bool USovDeflectionComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent);
}

bool USovDeflectionComponent::BeginDeflection()
{
	if (!IsInitialized()
		|| !IsValid(GetOwner())
		|| PerfectDeflectionWindow <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& NarrativeTags = FNarrativeGameplayTags::Get();
	const float CurrentStamina = AbilitySystemComponent->GetNumericAttribute(
		UNarrativeAttributeSetBase::GetStaminaAttribute());
	if (!AbilitySystemComponent->HasMatchingGameplayTag(SovTags.Character_Player_Selene)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.Character_Player_Tarrik)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Deflecting)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Guarding)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Fatal)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Poise_Broken)
		|| AbilitySystemComponent->HasMatchingGameplayTag(NarrativeTags.State_IsDead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(NarrativeTags.State_Interacting)
		|| AbilitySystemComponent->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
		|| AbilitySystemComponent->HasMatchingGameplayTag(NarrativeTags.State_Movement_Ragdoll)
		|| CurrentStamina + KINDA_SMALL_NUMBER < FMath::Max(MinimumDeflectionStartStamina, 0.f))
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
	for (const FGameplayTag& OwnedTag : OwnedTags)
	{
		if (OwnedTag != SovTags.Character_Player
			&& OwnedTag != SovTags.Character_Player_Selene
			&& OwnedTag.MatchesTag(SovTags.Character_Player))
		{
			return false;
		}
	}

	SetOwnedLooseTag(SovTags.State_Deflecting, true, bAppliedDeflectingTag);
	if (!bAppliedDeflectingTag)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(DeflectionWindowTimerHandle);
		TimerManager.SetTimer(
			DeflectionWindowTimerHandle,
			this,
			&ThisClass::CloseDeflectionWindow,
			PerfectDeflectionWindow,
			false);
	}

	OnDeflectionStarted.Broadcast();
	return true;
}

void USovDeflectionComponent::EndDeflection()
{
	CloseDeflectionWindow();
}

bool USovDeflectionComponent::IsDeflectionWindowOpen() const
{
	return IsInitialized()
		&& AbilitySystemComponent->HasMatchingGameplayTag(
			FSovGameplayTags::Get().State_Deflecting);
}

void USovDeflectionComponent::TryInitializeFromOwner()
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

void USovDeflectionComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovDeflectionComponent::UninitializeFromAbilitySystem()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeflectionWindowTimerHandle);
	}

	if (UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDamageResolvedAsTarget.RemoveDynamic(
			this,
			&ThisClass::HandleDamageResolvedAsTarget);
	}

	if (IsValid(AbilitySystemComponent))
	{
		SetOwnedLooseTag(
			FSovGameplayTags::Get().State_Deflecting,
			false,
			bAppliedDeflectingTag);
	}

	AbilitySystemComponent = nullptr;
	bAppliedDeflectingTag = false;
}

void USovDeflectionComponent::CloseDeflectionWindow()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeflectionWindowTimerHandle);
	}

	const bool bWasOpen = bAppliedDeflectingTag;
	SetOwnedLooseTag(
		FSovGameplayTags::Get().State_Deflecting,
		false,
		bAppliedDeflectingTag);
	if (bWasOpen)
	{
		OnDeflectionWindowClosed.Broadcast();
	}
}

void USovDeflectionComponent::SetOwnedLooseTag(
	const FGameplayTag& Tag,
	const bool bShouldApply,
	bool& bAppliedFlag)
{
	if (!IsValid(AbilitySystemComponent)
		|| !Tag.IsValid()
		|| bShouldApply == bAppliedFlag)
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

void USovDeflectionComponent::HandleDamageResolvedAsTarget(
	const FSovDamageResult& Result)
{
	if (!IsInitialized()
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| Result.TargetActor != GetOwner()
		|| Result.DefenseKind != ESovDefenseKind::Deflection
		|| !Result.bDeflected
		|| !Result.bPerfectDefense
		|| !IsDeflectionWindowOpen())
	{
		return;
	}

	// Consume the authoritative tag before presentation or reward callbacks. A
	// second hit in the same frame therefore cannot reuse the successful window.
	CloseDeflectionWindow();

	FGameplayEventData Payload;
	Payload.EventTag = FSovGameplayTags::Get().Event_Deflection_Perfect;
	Payload.Instigator = Result.SourceActor;
	Payload.Target = GetOwner();
	Payload.ContextHandle = Result.EffectContext;
	Payload.EventMagnitude = Result.ResolvedDamage;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwner(),
		Payload.EventTag,
		Payload);

	MulticastPerfectDeflection(Result);
}

void USovDeflectionComponent::MulticastPerfectDeflection_Implementation(
	const FSovDamageResult& Result)
{
	// Clear the predicted client window immediately. Its local timer remains an
	// unreliable-multicast fallback rather than a second gameplay authority.
	CloseDeflectionWindow();
	OnPerfectDeflection.Broadcast(Result);
}
