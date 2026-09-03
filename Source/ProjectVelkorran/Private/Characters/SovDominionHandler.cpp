// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovDominionHandler.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/SovGameplayAbility_DominionHound.h"
#include "CollisionQueryParams.h"
#include "Components/SovCommandLinkComponent.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Pawn.h"
#include "GameplayAbilitySpec.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace
{
	UNarrativeAbilitySystemComponent* ResolveNarrativeAbilitySystem(
		AActor* Actor)
	{
		return IsValid(Actor)
			? Cast<UNarrativeAbilitySystemComponent>(
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
			: nullptr;
	}

	bool HasExactHornChargeAbility(
		const UNarrativeAbilitySystemComponent* AbilitySystem)
	{
		if (!IsValid(AbilitySystem))
		{
			return false;
		}

		const FGameplayTag HornChargeTag =
			FSovGameplayTags::Get().Ability_NPC_DominionHound_HornCharge;
		for (const FGameplayAbilitySpec& Spec :
			AbilitySystem->GetActivatableAbilities())
		{
			if (!Spec.IsActive()
				&& IsValid(Cast<USovGameplayAbility_DominionHoundHornCharge>(
					Spec.Ability))
				&& Spec.Ability->GetAssetTags().HasTagExact(HornChargeTag))
			{
				return true;
			}
		}

		return false;
	}
}

ASovDominionHandler::ASovDominionHandler(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommandLinkComponent = CreateDefaultSubobject<USovCommandLinkComponent>(
		TEXT("SovCommandLinkComponent"));
}

TArray<AActor*> ASovDominionHandler::GetCommandableHounds() const
{
	return BuildCommandableHoundCandidates();
}

AActor* ASovDominionHandler::FindBestCommandableHound() const
{
	const TArray<AActor*> Candidates = BuildCommandableHoundCandidates();
	return Candidates.IsEmpty() ? nullptr : Candidates[0];
}

bool ASovDominionHandler::TryOrderLinkedHoundHornCharge(
	const FGuid& ExpectedLinkInstanceId,
	AActor* ExpectedHound,
	AActor*& OutOrderedHound,
	AActor*& OutChargeTarget)
{
	OutOrderedHound = nullptr;
	OutChargeTarget = nullptr;
	if (!CanIssueHoundCommands()
		|| !ExpectedLinkInstanceId.IsValid()
		|| CommandLinkComponent->GetLinkInstanceId()
			!= ExpectedLinkInstanceId
		|| !IsCommandableHound(ExpectedHound))
	{
		return false;
	}

	// The reliable anticipation cue named this exact Hound. Never replace it at
	// the release frame with an untelegraphed pack member; a later command may
	// select and visibly anticipate a different candidate.
	AActor* ChargeTarget = nullptr;
	if (!TryActivateExactHornCharge(
			ExpectedHound,
			ExpectedLinkInstanceId,
			ChargeTarget))
	{
		return false;
	}

	LastCommandedHound = ExpectedHound;
	OutOrderedHound = ExpectedHound;
	OutChargeTarget = ChargeTarget;
	return true;
}

bool ASovDominionHandler::CanIssueHoundCommands() const
{
	if (!HasAuthority()
		|| !IsValid(CommandLinkComponent)
		|| !CommandLinkComponent->IsCommandLinkActive()
		|| CommandLinkComponent->GetCommandSource() != this
		|| !CommandLinkComponent->GetLinkInstanceId().IsValid()
		|| !FMath::IsFinite(MaximumCommandDistance)
		|| MaximumCommandDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* HandlerAbilitySystem =
		GetNarrativeAbilitySystemComponent();
	if (!IsValid(HandlerAbilitySystem)
		|| HandlerAbilitySystem->IsDead()
		|| !HandlerAbilitySystem->GetSet<UNarrativeAttributeSetBase>())
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags =
		FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	return HandlerAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute())
			> KINDA_SMALL_NUMBER
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_IsDead)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Interacting)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_SequencerControlled)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Movement_Ragdoll)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Weapon_Equipping)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_Weapon_IsFiring)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(SovTags.State_Fatal)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_Poise_Broken)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_Status_Frozen)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_Status_DeviceDisabled)
		&& !HandlerAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_CommandLink_Severed)
		&& HandlerAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_CommandLink_Active);
}

bool ASovDominionHandler::IsCommandableHound(
	AActor* Candidate,
	const bool bAllowTransientAuthorization) const
{
	if (!CanIssueHoundCommands()
		|| !IsValid(Candidate)
		|| Candidate == this
		|| Candidate->GetWorld() != GetWorld()
		|| !CommandLinkComponent->ContainsLinkedActor(Candidate)
		|| !IsValid(Cast<ANarrativeCharacter>(Candidate)))
	{
		return false;
	}

	const FVector HandlerLocation = GetActorLocation();
	const FVector CandidateLocation = Candidate->GetActorLocation();
	if (HandlerLocation.ContainsNaN()
		|| CandidateLocation.ContainsNaN()
		|| FVector::DistSquared(HandlerLocation, CandidateLocation)
			> FMath::Square(MaximumCommandDistance))
	{
		return false;
	}

	const INarrativeTeamAgentInterface* HandlerTeam =
		Cast<const INarrativeTeamAgentInterface>(this);
	if (!HandlerTeam
		|| HandlerTeam->GetTeamAttitudeTowards(*Candidate)
			!= ETeamAttitude::Friendly)
	{
		return false;
	}

	UNarrativeAbilitySystemComponent* HoundAbilitySystem =
		ResolveNarrativeAbilitySystem(Candidate);
	if (!IsValid(HoundAbilitySystem)
		|| HoundAbilitySystem->GetAvatarActor() != Candidate
		|| HoundAbilitySystem->IsDead()
		|| !HoundAbilitySystem->GetSet<UNarrativeAttributeSetBase>())
	{
		return false;
	}

	const FNarrativeGameplayTags& NarrativeTags =
		FNarrativeGameplayTags::Get();
	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	return HoundAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute())
			> KINDA_SMALL_NUMBER
		&& !HoundAbilitySystem->HasMatchingGameplayTag(
			NarrativeTags.State_IsDead)
		&& !HoundAbilitySystem->HasMatchingGameplayTag(SovTags.State_Fatal)
		&& !HoundAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_CommandLink_Severed)
		&& (bAllowTransientAuthorization
			|| !HoundAbilitySystem->HasMatchingGameplayTag(
				SovTags.State_CommandLink_HoundChargeAuthorized))
		&& HoundAbilitySystem->HasMatchingGameplayTag(
			SovTags.State_CommandLink_Active)
		&& HasExactHornChargeAbility(HoundAbilitySystem)
		&& HasCommandLineOfSightTo(Candidate);
}

bool ASovDominionHandler::HasCommandLineOfSightTo(
	const AActor* Candidate) const
{
	if (!bRequireLineOfSightToHound)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(Candidate))
	{
		return false;
	}

	FVector TraceStart;
	FRotator UnusedViewRotation;
	GetActorEyesViewPoint(TraceStart, UnusedViewRotation);
	FVector TraceEnd = Candidate->GetActorLocation();
	if (const APawn* CandidatePawn = Cast<APawn>(Candidate))
	{
		CandidatePawn->GetActorEyesViewPoint(
			TraceEnd,
			UnusedViewRotation);
	}
	if (TraceStart.ContainsNaN() || TraceEnd.ContainsNaN())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(SovDominionHandlerCommandLOS),
		true,
		this);
	if (IsValid(CommandLinkComponent))
	{
		// Formation members must not occlude one another's command beam; world
		// geometry and unrelated blockers still interrupt the order.
		QueryParams.AddIgnoredActors(CommandLinkComponent->GetLinkedActors());
	}
	else
	{
		QueryParams.AddIgnoredActor(Candidate);
	}
	return !World->LineTraceTestByChannel(
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams);
}

bool ASovDominionHandler::TryActivateExactHornCharge(
	AActor* Candidate,
	const FGuid& ExpectedLinkInstanceId,
	AActor*& OutChargeTarget) const
{
	OutChargeTarget = nullptr;
	if (!ExpectedLinkInstanceId.IsValid()
		|| !CanIssueHoundCommands()
		|| CommandLinkComponent->GetLinkInstanceId()
			!= ExpectedLinkInstanceId
		|| !IsCommandableHound(Candidate))
	{
		return false;
	}

	UNarrativeAbilitySystemComponent* HoundAbilitySystem =
		ResolveNarrativeAbilitySystem(Candidate);
	if (!IsValid(HoundAbilitySystem))
	{
		return false;
	}

	const FGameplayTag HornChargeTag =
		FSovGameplayTags::Get().Ability_NPC_DominionHound_HornCharge;
	const FGameplayTag AuthorizationTag =
		FSovGameplayTags::Get().State_CommandLink_HoundChargeAuthorized;
	TArray<FGameplayAbilitySpecHandle> HornChargeHandles;
	for (const FGameplayAbilitySpec& Spec :
		HoundAbilitySystem->GetActivatableAbilities())
	{
		if (!Spec.IsActive()
			&& IsValid(Cast<USovGameplayAbility_DominionHoundHornCharge>(
				Spec.Ability))
			&& Spec.Ability->GetAssetTags().HasTagExact(HornChargeTag))
		{
			HornChargeHandles.Add(Spec.Handle);
		}
	}

	for (const FGameplayAbilitySpecHandle& Handle : HornChargeHandles)
	{
		if (!CanIssueHoundCommands()
			|| CommandLinkComponent->GetLinkInstanceId()
				!= ExpectedLinkInstanceId
			|| !IsCommandableHound(Candidate))
		{
			return false;
		}

		// The active link makes the specialist move eligible. The short-lived tag
		// satisfies GAS requirements, while a private native scope opened below
		// proves this exact activation came from the Handler command pipeline.
		HoundAbilitySystem->AddLooseGameplayTag(AuthorizationTag);
		if (!IsValid(HoundAbilitySystem)
			|| !IsValid(Candidate)
			|| HoundAbilitySystem->GetAvatarActor() != Candidate
			|| !CanIssueHoundCommands()
			|| CommandLinkComponent->GetLinkInstanceId()
				!= ExpectedLinkInstanceId
			|| !IsCommandableHound(Candidate, true))
		{
			if (IsValid(HoundAbilitySystem))
			{
				HoundAbilitySystem->RemoveLooseGameplayTag(AuthorizationTag);
			}
			return false;
		}

		// Tag listeners execute synchronously while the private native dispatch
		// scope is closed. They may observe the tag, but cannot activate the move.
		FGameplayAbilitySpec* PendingSpec =
			HoundAbilitySystem->FindAbilitySpecFromHandle(Handle);
		USovGameplayAbility_DominionHoundHornCharge* PendingHornAbility =
			PendingSpec
			? Cast<USovGameplayAbility_DominionHoundHornCharge>(
				PendingSpec->GetPrimaryInstance())
			: nullptr;
		if (!PendingSpec
			|| PendingSpec->IsActive()
			|| !IsValid(Cast<USovGameplayAbility_DominionHoundHornCharge>(
				PendingSpec->Ability))
			|| !PendingSpec->Ability->GetAssetTags().HasTagExact(HornChargeTag)
			|| !IsValid(PendingHornAbility))
		{
			if (PendingSpec && PendingSpec->IsActive())
			{
				HoundAbilitySystem->CancelAbilityHandle(Handle);
			}
			if (!IsValid(HoundAbilitySystem))
			{
				return false;
			}
			HoundAbilitySystem->RemoveLooseGameplayTag(AuthorizationTag);
			if (!IsValid(HoundAbilitySystem))
			{
				return false;
			}
			continue;
		}

		PendingHornAbility->SetHandlerOrderDispatchInProgress(true);
		const bool bActivated =
			HoundAbilitySystem->TryActivateAbility(Handle, false);
		if (IsValid(PendingHornAbility))
		{
			PendingHornAbility->SetHandlerOrderDispatchInProgress(false);
		}
		if (IsValid(HoundAbilitySystem))
		{
			HoundAbilitySystem->RemoveLooseGameplayTag(AuthorizationTag);
		}
		else
		{
			return false;
		}
		if (!IsValid(HoundAbilitySystem))
		{
			return false;
		}

		const bool bLinkStillCurrent = CanIssueHoundCommands()
			&& CommandLinkComponent->GetLinkInstanceId()
				== ExpectedLinkInstanceId;
		const bool bCandidateStillLinked = IsValid(Candidate)
			&& HoundAbilitySystem->GetAvatarActor() == Candidate
			&& IsValid(CommandLinkComponent)
			&& CommandLinkComponent->ContainsLinkedActor(Candidate);
		if (!bLinkStillCurrent || !bCandidateStillLinked)
		{
			if (bActivated)
			{
				HoundAbilitySystem->CancelAbilityHandle(Handle);
			}
			if (!bLinkStillCurrent)
			{
				return false;
			}
			continue;
		}
		if (!bActivated)
		{
			continue;
		}

		const FGameplayAbilitySpec* ActivatedSpec =
			HoundAbilitySystem->FindAbilitySpecFromHandle(Handle);
		const USovGameplayAbility_DominionHoundHornCharge* HoundAbility =
			ActivatedSpec
			? Cast<USovGameplayAbility_DominionHoundHornCharge>(
				ActivatedSpec->GetPrimaryInstance())
			: nullptr;
		if (!ActivatedSpec
			|| !ActivatedSpec->IsActive()
			|| !IsValid(Cast<USovGameplayAbility_DominionHoundHornCharge>(
				ActivatedSpec->Ability))
			|| !ActivatedSpec->Ability->GetAssetTags().HasTagExact(HornChargeTag)
			|| !IsValid(HoundAbility)
			|| !HoundAbility->IsActive())
		{
			continue;
		}

		OutChargeTarget = HoundAbility->GetCurrentAttackTarget();
		if (!IsValid(OutChargeTarget))
		{
			HoundAbilitySystem->CancelAbilityHandle(Handle);
			OutChargeTarget = nullptr;
			continue;
		}
		if (!CanIssueHoundCommands()
			|| CommandLinkComponent->GetLinkInstanceId()
				!= ExpectedLinkInstanceId
			|| !CommandLinkComponent->ContainsLinkedActor(Candidate))
		{
			HoundAbilitySystem->CancelAbilityHandle(Handle);
			OutChargeTarget = nullptr;
			return false;
		}
		return true;
	}

	return false;
}

TArray<AActor*> ASovDominionHandler::BuildCommandableHoundCandidates() const
{
	TArray<AActor*> Candidates;
	if (!CanIssueHoundCommands())
	{
		return Candidates;
	}

	for (AActor* LinkedActor : CommandLinkComponent->GetLinkedActors())
	{
		if (IsCommandableHound(LinkedActor))
		{
			Candidates.Add(LinkedActor);
		}
	}

	const FGuid ActiveLinkInstanceId =
		CommandLinkComponent->GetLinkInstanceId();
	Candidates.Sort(
		[this, ActiveLinkInstanceId](const AActor& Left, const AActor& Right)
		{
			const uint64 LeftSequence = GetCommandAttemptSequenceFor(
				&Left,
				ActiveLinkInstanceId);
			const uint64 RightSequence = GetCommandAttemptSequenceFor(
				&Right,
				ActiveLinkInstanceId);
			if (LeftSequence != RightSequence)
			{
				return LeftSequence < RightSequence;
			}

			const float LeftDistanceSquared = FVector::DistSquared(
				GetActorLocation(),
				Left.GetActorLocation());
			const float RightDistanceSquared = FVector::DistSquared(
				GetActorLocation(),
				Right.GetActorLocation());
			if (!FMath::IsNearlyEqual(
				LeftDistanceSquared,
				RightDistanceSquared,
				1.0f))
			{
				return LeftDistanceSquared < RightDistanceSquared;
			}

			return Left.GetPathName().Compare(
				Right.GetPathName(),
				ESearchCase::CaseSensitive) < 0;
		});
	return Candidates;
}

uint64 ASovDominionHandler::GetCommandAttemptSequenceFor(
	const AActor* Candidate,
	const FGuid& ActiveLinkInstanceId) const
{
	if (!IsValid(Candidate)
		|| ActiveLinkInstanceId != CommandAttemptHistoryLinkInstanceId)
	{
		return 0;
	}

	const uint64* Sequence = CommandAttemptSequenceByHound.Find(
		TWeakObjectPtr<AActor>(const_cast<AActor*>(Candidate)));
	return Sequence ? *Sequence : 0;
}

void ASovDominionHandler::RecordCommandAttempt(
	AActor* Candidate,
	const FGuid& ActiveLinkInstanceId)
{
	if (!IsValid(Candidate) || !ActiveLinkInstanceId.IsValid())
	{
		return;
	}
	if (CommandAttemptHistoryLinkInstanceId != ActiveLinkInstanceId)
	{
		CommandAttemptSequenceByHound.Reset();
		CommandAttemptHistoryLinkInstanceId = ActiveLinkInstanceId;
		NextCommandAttemptSequence = 1;
	}

	for (auto It = CommandAttemptSequenceByHound.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	CommandAttemptSequenceByHound.FindOrAdd(
		TWeakObjectPtr<AActor>(Candidate)) = NextCommandAttemptSequence++;
	if (NextCommandAttemptSequence == 0)
	{
		CommandAttemptSequenceByHound.Reset();
		NextCommandAttemptSequence = 1;
	}
}

void ASovDominionHandler::
	MulticastPresentHoundHornChargeAnticipation_Implementation(
		AActor* IntendedHound,
		const FGuid LinkInstanceId)
{
	ReceiveHoundHornChargeAnticipation(IntendedHound, LinkInstanceId);
}

void ASovDominionHandler::MulticastPresentHoundHornChargeOrder_Implementation(
	AActor* OrderedHound,
	AActor* ChargeTarget,
	const FGuid LinkInstanceId)
{
	ReceiveHoundHornChargeOrdered(
		OrderedHound,
		ChargeTarget,
		LinkInstanceId);
}
