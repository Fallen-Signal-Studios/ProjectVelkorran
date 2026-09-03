// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovSeleneEchoGenerationComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovDeflectionComponent.h"
#include "Components/SovEchoComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovSeleneEchoGeneration, Log, All);

USovSeleneEchoGenerationComponent::USovSeleneEchoGenerationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USovSeleneEchoGenerationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ANarrativeCharacter* NarrativeOwner =
		Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}
	TryInitializeFromOwner();
}

void USovSeleneEchoGenerationComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner =
		Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}

	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

bool USovSeleneEchoGenerationComponent::InitializeWithAbilitySystem(
	UNarrativeAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!IsValid(InAbilitySystemComponent) || !IsValid(GetOwner()))
	{
		UninitializeFromAbilitySystem();
		return false;
	}

	if (GetOwner()->FindComponentByClass<USovSeleneEchoGenerationComponent>()
		!= this)
	{
		UE_LOG(
			LogSovSeleneEchoGeneration,
			Error,
			TEXT("%s has more than one Selene Echo generator. Ignoring duplicate %s."),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(this));
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& IsValid(EchoComponent.Get())
		&& IsValid(DeflectionComponent.Get()))
	{
		return true;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InAbilitySystemComponent;
	EchoComponent = GetOwner()->FindComponentByClass<USovEchoComponent>();
	DeflectionComponent =
		GetOwner()->FindComponentByClass<USovDeflectionComponent>();
	if (!IsValid(EchoComponent.Get())
		|| !IsValid(DeflectionComponent.Get()))
	{
		UE_LOG(
			LogSovSeleneEchoGeneration,
			Error,
			TEXT("%s cannot initialize Selene Echo generation without its Echo and Deflection components."),
			*GetNameSafe(GetOwner()));
		AbilitySystemComponent = nullptr;
		EchoComponent = nullptr;
		DeflectionComponent = nullptr;
		return false;
	}
	if (!EchoComponent->IsInitialized()
		&& !EchoComponent->InitializeWithAbilitySystem(InAbilitySystemComponent))
	{
		UE_LOG(
			LogSovSeleneEchoGeneration,
			Error,
			TEXT("%s cannot initialize Selene Echo generation because Echo storage is not ready."),
			*GetNameSafe(GetOwner()));
		AbilitySystemComponent = nullptr;
		EchoComponent = nullptr;
		DeflectionComponent = nullptr;
		return false;
	}

	DeflectionComponent->OnPerfectDeflection.AddUniqueDynamic(
		this,
		&ThisClass::HandlePerfectDeflection);
	AbilitySystemComponent->OnDamageResolvedAsSource.AddUniqueDynamic(
		this,
		&ThisClass::HandleDamageResolvedAsSource);
	return true;
}

bool USovSeleneEchoGenerationComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent.Get())
		&& IsValid(EchoComponent.Get())
		&& IsValid(DeflectionComponent.Get());
}

void USovSeleneEchoGenerationComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	UNarrativeAbilitySystemComponent* OwnerAbilitySystem =
		Cast<UNarrativeAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
	if (IsValid(OwnerAbilitySystem)
		&& OwnerAbilitySystem != AbilitySystemComponent.Get())
	{
		InitializeWithAbilitySystem(OwnerAbilitySystem);
	}
}

void USovSeleneEchoGenerationComponent::UninitializeFromAbilitySystem()
{
	if (IsValid(DeflectionComponent.Get()))
	{
		DeflectionComponent->OnPerfectDeflection.RemoveDynamic(
			this,
			&ThisClass::HandlePerfectDeflection);
	}
	if (IsValid(AbilitySystemComponent.Get()))
	{
		AbilitySystemComponent->OnDamageResolvedAsSource.RemoveDynamic(
			this,
			&ThisClass::HandleDamageResolvedAsSource);
	}

	AbilitySystemComponent = nullptr;
	EchoComponent = nullptr;
	DeflectionComponent = nullptr;
}

bool USovSeleneEchoGenerationComponent::CanGenerateSeleneEcho(
	const bool bAllowDuringEchoAbility) const
{
	if (!IsInitialized()
		|| !GetOwner()
		|| !GetOwner()->HasAuthority()
		|| EchoComponent->GetMaxEcho() <= KINDA_SMALL_NUMBER
		|| EchoComponent->GetEcho() + KINDA_SMALL_NUMBER
			>= EchoComponent->GetMaxEcho())
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags =
		FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	if (!AbilitySystemComponent->HasMatchingGameplayTag(
			SovTags.Character_Player_Selene)
		|| AbilitySystemComponent->HasMatchingGameplayTag(
			SovTags.Character_Player_Tarrik)
		|| AbilitySystemComponent->HasMatchingGameplayTag(
			NarrativeTags.State_IsDead)
		|| AbilitySystemComponent->HasMatchingGameplayTag(SovTags.State_Fatal)
		|| (!bAllowDuringEchoAbility
			&& AbilitySystemComponent->HasMatchingGameplayTag(
				SovTags.State_EchoAbility_Active)))
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
	return true;
}

bool USovSeleneEchoGenerationComponent::IsEchoAbilityDamage(
	const FSovDamageResult& DamageResult) const
{
	if (DamageResult.bFromEchoAbility)
	{
		return true;
	}

	const UNarrativeGameplayAbility* SourceAbility =
		Cast<UNarrativeGameplayAbility>(
			DamageResult.EffectContext.GetAbility());
	return IsValid(SourceAbility)
		&& SourceAbility->GetAssetTags().HasTag(
			FSovGameplayTags::Get().Ability_Echo);
}

bool USovSeleneEchoGenerationComponent::IsHostileWeakPointTarget(
	const AActor* TargetActor) const
{
	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(GetOwner());
	return IsValid(TargetActor)
		&& SourceTeam
		&& SourceTeam->GetTeamAttitudeTowards(*TargetActor)
			== ETeamAttitude::Hostile;
}

bool USovSeleneEchoGenerationComponent::ConsumeCommandLinkSever(
	const FSovCommandLinkSeverResult& SeverResult)
{
	AActor* CommandNode = SeverResult.LinkOwner.Get();
	AActor* SeverInstigator = SeverResult.SeveredBy.Get();
	if (!GetOwner()
		|| !GetOwner()->HasAuthority()
		|| !SeverResult.TransactionId.IsValid()
		|| !SeverResult.LinkInstanceId.IsValid()
		|| SeverResult.LinkId == NAME_None
		|| SeverInstigator != GetOwner()
		|| !IsValid(CommandNode)
		|| CommandNode->GetWorld() != GetWorld()
		|| ConsumedCommandLinkSeverTransactions.Contains(
			SeverResult.TransactionId))
	{
		return false;
	}

	// Consume the transaction before any resource or target policy can discard
	// its reward. A replay must never become payable after Echo later changes.
	ConsumedCommandLinkSeverTransactions.Add(SeverResult.TransactionId);
	if (!SeverResult.bEligibleForEchoReward)
	{
		return true;
	}

	AwardEcho(
		GetCommandLinkSeverEchoReward(),
		FSovGameplayTags::Get().Echo_Source_CommandLinkSever,
		ESovSeleneEchoAwardType::CommandLinkSever,
		SeverResult.LinkId,
		CommandNode,
		true);
	return true;
}

void USovSeleneEchoGenerationComponent::AwardEcho(
	const float RequestedEcho,
	const FGameplayTag& SourceTag,
	const ESovSeleneEchoAwardType AwardType,
	const FName WeakPointId,
	AActor* OtherActor,
	const bool bAllowDuringEchoAbility)
{
	if (!CanGenerateSeleneEcho(bAllowDuringEchoAbility)
		|| RequestedEcho <= KINDA_SMALL_NUMBER
		|| !SourceTag.IsValid())
	{
		return;
	}

	const float AppliedEcho = EchoComponent->AddEcho(
		RequestedEcho,
		SourceTag);
	if (AppliedEcho > KINDA_SMALL_NUMBER)
	{
		ClientNotifySeleneEchoAwarded(
			AppliedEcho,
			EchoComponent->GetEcho(),
			AwardType,
			WeakPointId,
			OtherActor);
	}
}

void USovSeleneEchoGenerationComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovSeleneEchoGenerationComponent::HandlePerfectDeflection(
	const FSovDamageResult& DamageResult)
{
	if (!CanGenerateSeleneEcho()
		|| DamageResult.TargetActor.Get() != GetOwner()
		|| !DamageResult.bPerfectDefense
		|| !DamageResult.bDeflected
		|| DamageResult.DefenseKind != ESovDefenseKind::Deflection)
	{
		return;
	}

	AwardEcho(
		GetPerfectDeflectionEchoReward(),
		FSovGameplayTags::Get().Echo_Source_PerfectDeflection,
		ESovSeleneEchoAwardType::PerfectDeflection,
		NAME_None,
		DamageResult.SourceActor.Get());
}

void USovSeleneEchoGenerationComponent::HandleDamageResolvedAsSource(
	const FSovDamageResult& DamageResult)
{
	if (DamageResult.SourceActor.Get() != GetOwner()
		|| !IsValid(DamageResult.TargetActor.Get()))
	{
		return;
	}

	USovWeakPointComponent* WeakPointComponent =
		DamageResult.TargetActor->FindComponentByClass<USovWeakPointComponent>();
	if (!IsValid(WeakPointComponent))
	{
		return;
	}

	// Consume the target-owned transaction before any resource/identity/source
	// policy can discard the award. Full Echo, death, an Echo ability, or a
	// friendly target must not leave an exact break claim pending indefinitely.
	FName BrokenWeakPointId = NAME_None;
	if (!WeakPointComponent->ConsumeWeakPointBreak(
		DamageResult,
		BrokenWeakPointId))
	{
		return;
	}
	if (!IsHostileWeakPointTarget(DamageResult.TargetActor.Get())
		|| IsEchoAbilityDamage(DamageResult))
	{
		return;
	}

	AwardEcho(
		GetWeakPointBreakEchoReward(),
		FSovGameplayTags::Get().Echo_Source_WeakPointBreak,
		ESovSeleneEchoAwardType::WeakPointBreak,
		BrokenWeakPointId,
		DamageResult.TargetActor.Get());
}

void USovSeleneEchoGenerationComponent::ClientNotifySeleneEchoAwarded_Implementation(
	const float AwardedEcho,
	const float NewEcho,
	const ESovSeleneEchoAwardType AwardType,
	const FName WeakPointId,
	AActor* OtherActor)
{
	OnSeleneEchoAwarded.Broadcast(
		AwardedEcho,
		NewEcho,
		AwardType,
		WeakPointId,
		OtherActor);
}
