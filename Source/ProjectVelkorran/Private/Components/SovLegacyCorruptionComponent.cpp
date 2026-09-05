// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovLegacyCorruptionComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Effects/SovGameplayEffect_Corruption.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/SovCorruptionAttributeSet.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovCorruption, Log, All);

bool FSovCorruptionSourceSpec::HasValidNumbers() const
{
	const FGameplayTag CleanseRoot = FSovGameplayTags::Get().Status_Cleanse;
	if (!FMath::IsFinite(ExposurePerSecond)
		|| !FMath::IsFinite(InstantExposure)
		|| !FMath::IsFinite(InnerRadius)
		|| !FMath::IsFinite(OuterRadius)
		|| ExposurePerSecond < 0.0f
		|| InstantExposure < 0.0f
		|| (ExposurePerSecond <= 0.0f && InstantExposure <= 0.0f)
		|| InnerRadius < 0.0f
		|| OuterRadius < 0.0f)
	{
		return false;
	}
	if ((FalloffPolicy != ESovCorruptionFalloffPolicy::None
			&& FalloffPolicy != ESovCorruptionFalloffPolicy::Linear)
		|| static_cast<uint8>(BandCap)
			> static_cast<uint8>(ESovLegacyCorruptionBand::OverwriteRisk)
		|| (bEnforceBandCap && BandCap == ESovLegacyCorruptionBand::None))
	{
		return false;
	}
	if (SourceId == NAME_None
		|| !RemedyTag.IsValid()
		|| !CleanseRoot.IsValid()
		|| !RemedyTag.MatchesTag(CleanseRoot)
		|| RemedyTag.MatchesTagExact(CleanseRoot)
		|| RemedyInstruction.IsEmpty()
		|| PresentationProfile == NAME_None
		|| AccessibilitySubstitute.IsEmpty())
	{
		return false;
	}

	return FalloffPolicy != ESovCorruptionFalloffPolicy::Linear
		|| OuterRadius > InnerRadius + KINDA_SMALL_NUMBER;
}

namespace SovLegacyCorruptionIsolation
{
	bool HasActiveCampaign(const AActor* Owner)
	{
		const APawn* Pawn = Cast<APawn>(Owner);
		const auto* Campaign = Pawn && Pawn->GetController()
			? Pawn->GetController()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
		return Campaign && Campaign->GetActiveMission();
	}
}

USovLegacyCorruptionComponent::USovLegacyCorruptionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	BandStateEffectClass = USovGameplayEffect_CorruptionBandState::StaticClass();
}

void USovLegacyCorruptionComponent::BeginPlay()
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

void USovLegacyCorruptionComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* NarrativeOwner = Cast<ANarrativeCharacter>(GetOwner()))
	{
		NarrativeOwner->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleOwnerASCInitialized);
	}

	ActiveSources.Reset();
	StopSourceTimer();
	ClearPendingInstantExposures();
	ClearInstantReplayGuards();
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void USovLegacyCorruptionComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovLegacyCorruptionComponent, CurrentBand);
	DOREPLIFETIME(USovLegacyCorruptionComponent, PresentationState);
	DOREPLIFETIME(USovLegacyCorruptionComponent, bMissionAllowsOverwriteRisk);
	DOREPLIFETIME(
		USovLegacyCorruptionComponent,
		bOverwriteRiskAuthorizedForCurrentExposure);
}

bool USovLegacyCorruptionComponent::InitializeWithAbilitySystem(
	UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (SovLegacyCorruptionIsolation::HasActiveCampaign(GetOwner())
		|| !IsValid(InAbilitySystemComponent)
		|| InAbilitySystemComponent->GetAvatarActor() != GetOwner())
	{
		return false;
	}

	if (AbilitySystemComponent == InAbilitySystemComponent
		&& CorruptionChangedDelegateHandle.IsValid()
		&& MaxCorruptionChangedDelegateHandle.IsValid()
		&& HasCurrentAvatarBinding())
	{
		return true;
	}

	if (!InAbilitySystemComponent->GetSet<USovCorruptionAttributeSet>())
	{
		if (!bWarnedMissingAttributeSet)
		{
			UE_LOG(
				LogSovCorruption,
				Warning,
				TEXT("%s cannot initialize Corruption: its persistent Ability System has no USovCorruptionAttributeSet."),
				*GetNameSafe(GetOwner()));
			bWarnedMissingAttributeSet = true;
		}
		return false;
	}

	const bool bReplacingAbilitySystem = AbilitySystemComponent != nullptr
		&& AbilitySystemComponent != InAbilitySystemComponent;
	UninitializeFromAbilitySystem();
	if (bReplacingAbilitySystem)
	{
		// A live-pawn PlayerState/ASC replacement starts from the incoming ASC's
		// numeric truth. Never leak the previous player's remedy, presentation,
		// authorization, checkpoint reconstruction IDs, or one-shot replay guards.
		const int32 NextPresentationRevision = PresentationState.Revision + 1;
		CurrentBand = ESovLegacyCorruptionBand::None;
		PresentationState = FSovCorruptionReplicatedPresentationState();
		PresentationState.Revision = NextPresentationRevision;
		bOverwriteRiskAuthorizedForCurrentExposure = false;
		ClearRememberedPresentationContract();
		RestoredSourceIds.Reset();
		RestoredCanonPersistentSourceIds.Reset();
		ClearInstantReplayGuards();
		WakeForReplication();
	}
	AbilitySystemComponent = InAbilitySystemComponent;
	bWarnedMissingAttributeSet = false;

	CorruptionChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			USovCorruptionAttributeSet::GetCorruptionAttribute())
		.AddUObject(this, &ThisClass::HandleCorruptionAttributeChanged);
	MaxCorruptionChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(
			USovCorruptionAttributeSet::GetMaxCorruptionAttribute())
		.AddUObject(this, &ThisClass::HandleMaxCorruptionAttributeChanged);

	if (UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDeathStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (const UNarrativeAbilitySystemComponent* NarrativeASC =
			Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
			NarrativeASC && NarrativeASC->IsDead())
		{
			// A checkpoint queued before actor-info readiness remains pending;
			// death cleanup wins until this exact avatar is revived.
			ResetForDeath();
		}
		else if (bHasPendingCheckpointRestore)
		{
			const FSovCorruptionCheckpointData PendingData =
				PendingCheckpointRestore;
			bHasPendingCheckpointRestore = false;
			PendingCheckpointRestore = FSovCorruptionCheckpointData();
			ApplyCheckpointNow(PendingData);
		}
		else
		{
			RefreshBand(false);
			ApplyBandEffect();
			RefreshPresentationState();
		}
	}

	return true;
}

bool USovLegacyCorruptionComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent)
		&& AbilitySystemComponent->GetSet<USovCorruptionAttributeSet>()
		&& HasCurrentAvatarBinding();
}

float USovLegacyCorruptionComponent::GetCorruption() const
{
	return IsInitialized()
		? AbilitySystemComponent->GetNumericAttribute(
			USovCorruptionAttributeSet::GetCorruptionAttribute())
		: 0.0f;
}

float USovLegacyCorruptionComponent::GetMaxCorruption() const
{
	return IsInitialized()
		? FMath::Max(
			AbilitySystemComponent->GetNumericAttribute(
				USovCorruptionAttributeSet::GetMaxCorruptionAttribute()),
			1.0f)
		: 100.0f;
}

float USovLegacyCorruptionComponent::GetNormalizedCorruption() const
{
	return FMath::Clamp(GetCorruption() / GetMaxCorruption(), 0.0f, 1.0f);
}

ESovLegacyCorruptionBand USovLegacyCorruptionComponent::DetermineBandForExposure(
	const float Exposure) const
{
	const float ClampedExposure = FMath::Clamp(
		Exposure,
		0.0f,
		GetMaxCorruption());
	if (ClampedExposure <= 0.0f)
	{
		return ESovLegacyCorruptionBand::None;
	}

	if (ClampedExposure
		>= GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk)
		&& bMissionAllowsOverwriteRisk
		&& bOverwriteRiskAuthorizedForCurrentExposure)
	{
		return ESovLegacyCorruptionBand::OverwriteRisk;
	}
	if (ClampedExposure
		>= GetThresholdForBand(ESovLegacyCorruptionBand::Contest))
	{
		return ESovLegacyCorruptionBand::Contest;
	}
	if (ClampedExposure
		>= GetThresholdForBand(ESovLegacyCorruptionBand::Intrusion))
	{
		return ESovLegacyCorruptionBand::Intrusion;
	}
	return ESovLegacyCorruptionBand::Trace;
}

float USovLegacyCorruptionComponent::GetExposureCeilingForBand(
	const ESovLegacyCorruptionBand Band) const
{
	const float MaxValue = GetMaxCorruption();
	const float BeforeNextBand = [MaxValue](const float Threshold)
	{
		return FMath::Clamp(
			Threshold - FMath::Max(KINDA_SMALL_NUMBER, MaxValue * 0.0001f),
			0.0f,
			MaxValue);
	};

	switch (Band)
	{
	case ESovLegacyCorruptionBand::None:
		return 0.0f;
	case ESovLegacyCorruptionBand::Trace:
		return BeforeNextBand(
			GetThresholdForBand(ESovLegacyCorruptionBand::Intrusion));
	case ESovLegacyCorruptionBand::Intrusion:
		return BeforeNextBand(
			GetThresholdForBand(ESovLegacyCorruptionBand::Contest));
	case ESovLegacyCorruptionBand::Contest:
		return BeforeNextBand(
			GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk));
	case ESovLegacyCorruptionBand::OverwriteRisk:
	default:
		return MaxValue;
	}
}

FSovCorruptionPresentationSnapshot
USovLegacyCorruptionComponent::GetPresentationSnapshot() const
{
	FSovCorruptionPresentationSnapshot Snapshot;
	Snapshot.Corruption = GetCorruption();
	Snapshot.MaxCorruption = GetMaxCorruption();
	Snapshot.NormalizedCorruption = GetNormalizedCorruption();
	Snapshot.Band = CurrentBand;
	Snapshot.SourceDirection = PresentationState.SourceDirection;
	Snapshot.bHasDirectionalSource =
		PresentationState.bHasDirectionalSource;
	Snapshot.RemedyTag = PresentationState.RemedyTag;
	Snapshot.RemedyInstruction = PresentationState.RemedyInstruction;
	Snapshot.PresentationProfile = PresentationState.PresentationProfile;
	Snapshot.AccessibilitySubstitute =
		PresentationState.AccessibilitySubstitute;
	Snapshot.bReducedEffectsPresentation = bReducedEffectsPresentation;
	Snapshot.bGameplayEquivalentInReducedEffects = true;
	Snapshot.PresentationRevision = PresentationState.Revision;
	return Snapshot;
}

FSovLegacyCorruptionSourceHandle
USovLegacyCorruptionComponent::RegisterCorruptionSource(
	AActor* SourceActor,
	const FSovCorruptionSourceSpec& SourceSpec)
{
	FSovLegacyCorruptionSourceHandle Result;
	if (!CanWriteCorruption()
		|| !IsValid(SourceActor)
		|| SourceActor->GetWorld() != GetWorld()
		|| !SourceSpec.HasValidNumbers()
		|| (SourceSpec.ExposurePerSecond <= 0.0f
			&& SourceSpec.InstantExposure <= 0.0f))
	{
		return Result;
	}

	if (!ActiveSources.IsEmpty())
	{
		EvaluateAccumulatedSources();
	}
	for (const TPair<FGuid, FSovActiveSource>& Pair : ActiveSources)
	{
		if (Pair.Value.Spec.SourceId == SourceSpec.SourceId)
		{
			UE_LOG(
				LogSovCorruption,
				Warning,
				TEXT("%s rejected duplicate active corruption SourceId %s. Placed fields require unique stable IDs."),
				*GetNameSafe(GetOwner()),
				*SourceSpec.SourceId.ToString());
			return Result;
		}
		const FGameplayTag ExistingRemedy = Pair.Value.Spec.RemedyTag;
		if (ExistingRemedy != SourceSpec.RemedyTag)
		{
			UE_LOG(
				LogSovCorruption,
				Warning,
				TEXT("%s rejected corruption source %s: simultaneous prototype sources must share one remedy tag."),
				*GetNameSafe(GetOwner()),
				*SourceSpec.SourceId.ToString());
			return Result;
		}
	}

	Result.Id = FGuid::NewGuid();
	FSovActiveSource& NewSource = ActiveSources.Add(Result.Id);
	NewSource.SourceActor = SourceActor;
	NewSource.Spec = SourceSpec;
	const bool bRestoringCapturedSource =
		InstantExposureReplayGuardSourceIds.Remove(SourceSpec.SourceId) > 0;
	if (InstantExposureReplayGuardSourceIds.IsEmpty())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(InstantReplayGuardTimerHandle);
		}
	}
	if (SourceSpec.InstantExposure > 0.0f && !bRestoringCapturedSource)
	{
		PendingInstantSourceHandles.Add(Result.Id);
		SchedulePendingInstantExposureEvaluation();
	}

	RefreshSourceTimer();
	RefreshPresentationState();
	return Result;
}

bool USovLegacyCorruptionComponent::UpdateCorruptionSource(
	const FSovLegacyCorruptionSourceHandle SourceHandle,
	const FSovCorruptionSourceSpec& SourceSpec)
{
	if (!CanWriteCorruption() || !SourceHandle.IsValid()
		|| !SourceSpec.HasValidNumbers())
	{
		return false;
	}

	EvaluatePendingInstantExposures();
	EvaluateAccumulatedSources();
	FSovActiveSource* ExistingSource = ActiveSources.Find(SourceHandle.Id);
	if (!ExistingSource || !ExistingSource->SourceActor.IsValid())
	{
		return false;
	}
	for (const TPair<FGuid, FSovActiveSource>& Pair : ActiveSources)
	{
		if (Pair.Key == SourceHandle.Id)
		{
			continue;
		}
		if (Pair.Value.Spec.SourceId == SourceSpec.SourceId)
		{
			return false;
		}
		const FGameplayTag OtherRemedy = Pair.Value.Spec.RemedyTag;
		if (OtherRemedy != SourceSpec.RemedyTag)
		{
			return false;
		}
	}

	ExistingSource->Spec = SourceSpec;
	RefreshSourceTimer();
	RefreshPresentationState();
	return true;
}

bool USovLegacyCorruptionComponent::UnregisterCorruptionSource(
	const FSovLegacyCorruptionSourceHandle SourceHandle)
{
	if (!CanWriteCorruption(true) || !SourceHandle.IsValid())
	{
		return false;
	}

	if (CanWriteCorruption())
	{
		EvaluatePendingInstantExposures();
		EvaluateAccumulatedSources();
	}
	else
	{
		PendingInstantSourceHandles.Remove(SourceHandle.Id);
	}
	const bool bRemoved = ActiveSources.Remove(SourceHandle.Id) > 0;
	if (bRemoved)
	{
		RefreshSourceTimer();
		RefreshPresentationState();
	}
	return bRemoved;
}

bool USovLegacyCorruptionComponent::HasRegisteredCorruptionSource(
	const FSovLegacyCorruptionSourceHandle SourceHandle) const
{
	return SourceHandle.IsValid()
		&& ActiveSources.Contains(SourceHandle.Id);
}

void USovLegacyCorruptionComponent::ClearCorruptionSources()
{
	if (!CanWriteCorruption(true))
	{
		return;
	}

	if (CanWriteCorruption())
	{
		EvaluatePendingInstantExposures();
		EvaluateAccumulatedSources();
	}
	ActiveSources.Reset();
	ClearPendingInstantExposures();
	StopSourceTimer();
	RefreshPresentationState();
}

float USovLegacyCorruptionComponent::ApplyInstantExposure(
	const float ExposureAmount,
	AActor* SourceActor,
	const ESovLegacyCorruptionBand BandCap)
{
	if (!CanWriteCorruption()
		|| !FMath::IsFinite(ExposureAmount)
		|| ExposureAmount <= 0.0f
		|| (IsValid(SourceActor) && SourceActor->GetWorld() != GetWorld()))
	{
		return 0.0f;
	}
	EvaluatePendingInstantExposures();
	EvaluateAccumulatedSources();

	// A raw amount has no complete authored source/remedy/presentation contract.
	// It may build pressure through Intrusion only; Contest and Overwrite Risk
	// require RegisterCorruptionSource with a validated SourceSpec.
	const ESovLegacyCorruptionBand SafeBandCap =
		static_cast<uint8>(BandCap)
			< static_cast<uint8>(ESovLegacyCorruptionBand::Intrusion)
			? BandCap
			: ESovLegacyCorruptionBand::Intrusion;

	return ApplyExposureInternal(
		ExposureAmount,
		SafeBandCap,
		true,
		false,
		nullptr,
		SourceActor);
}

float USovLegacyCorruptionComponent::ReduceCorruption(
	const float ExposureReduction,
	AActor* RemedyInstigator)
{
	if (!CanWriteCorruption()
		|| !FMath::IsFinite(ExposureReduction)
		|| ExposureReduction <= 0.0f)
	{
		return 0.0f;
	}
	EvaluatePendingInstantExposures();
	EvaluateAccumulatedSources();

	const float OldCorruption = GetCorruption();
	const float NewCorruption = FMath::Max(
		OldCorruption - ExposureReduction,
		0.0f);
	if (NewCorruption >= OldCorruption)
	{
		return 0.0f;
	}

	if (NewCorruption
		< GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk))
	{
		bOverwriteRiskAuthorizedForCurrentExposure = false;
	}
	SetCorruptionInternal(NewCorruption);
	if (NewCorruption <= 0.0f)
	{
		ClearRememberedPresentationContract();
	}
	RefreshPresentationState();

	const float AppliedReduction = OldCorruption - GetCorruption();
	OnCorruptionRemedied.Broadcast(
		FGameplayTag(),
		OldCorruption,
		GetCorruption(),
		RemedyInstigator);
	SendCorruptionEvent(
		FSovGameplayTags::Get().Event_Corruption_RemedyChanged,
		AppliedReduction,
		RemedyInstigator);
	return AppliedReduction;
}

bool USovLegacyCorruptionComponent::ClearCorruption(AActor* RemedyInstigator)
{
	if (!CanWriteCorruption())
	{
		return false;
	}
	EvaluatePendingInstantExposures();
	EvaluateAccumulatedSources();
	const float OldCorruption = GetCorruption();
	return OldCorruption > 0.0f
		&& ReduceCorruption(OldCorruption, RemedyInstigator)
			> 0.0f;
}

bool USovLegacyCorruptionComponent::ApplyRemedy(
	const FGameplayTag RemedyTag,
	const float ExposureReduction,
	const bool bClearAll,
	AActor* RemedyInstigator)
{
	if (!CanWriteCorruption()
		|| (!bClearAll
			&& (!FMath::IsFinite(ExposureReduction)
				|| ExposureReduction <= 0.0f)))
	{
		return false;
	}
	EvaluatePendingInstantExposures();
	EvaluateAccumulatedSources();

	if (bRequireMatchingRemedyWhenSpecified
		&& RememberedRemedyTag.IsValid()
		&& !RemedyTag.MatchesTagExact(RememberedRemedyTag))
	{
		return false;
	}

	const float OldCorruption = GetCorruption();
	const float NewCorruption = bClearAll
		? 0.0f
		: FMath::Max(OldCorruption - ExposureReduction, 0.0f);
	if (NewCorruption >= OldCorruption)
	{
		return false;
	}

	if (NewCorruption
		< GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk))
	{
		bOverwriteRiskAuthorizedForCurrentExposure = false;
	}
	SetCorruptionInternal(NewCorruption);
	if (NewCorruption <= 0.0f)
	{
		ClearRememberedPresentationContract();
	}
	RefreshPresentationState();

	OnCorruptionRemedied.Broadcast(
		RemedyTag,
		OldCorruption,
		GetCorruption(),
		RemedyInstigator);
	SendCorruptionEvent(
		FSovGameplayTags::Get().Event_Corruption_RemedyChanged,
		OldCorruption - GetCorruption(),
		RemedyInstigator);
	return true;
}

void USovLegacyCorruptionComponent::SetMissionAllowsOverwriteRisk(
	const bool bAllowed)
{
	if (!CanWriteCorruption(true)
		|| bMissionAllowsOverwriteRisk == bAllowed)
	{
		return;
	}
	if (!bAllowed)
	{
		// Revoke permission first so a boundary-time source evaluation cannot
		// briefly enter Overwrite Risk while the mission is disabling it.
		bMissionAllowsOverwriteRisk = false;
		bOverwriteRiskAuthorizedForCurrentExposure = false;
		if (CanWriteCorruption())
		{
			EvaluatePendingInstantExposures();
			EvaluateAccumulatedSources();
		}
		const float ContestCeiling = GetExposureCeilingForBand(
			ESovLegacyCorruptionBand::Contest);
		if (GetCorruption() > ContestCeiling)
		{
			SetCorruptionInternal(ContestCeiling, true);
		}
	}
	else
	{
		if (CanWriteCorruption())
		{
			EvaluatePendingInstantExposures();
			EvaluateAccumulatedSources();
		}
		bMissionAllowsOverwriteRisk = true;
	}
	RefreshBand(true);
	RefreshPresentationState();
	WakeForReplication();
}

void USovLegacyCorruptionComponent::SetReducedEffectsPresentationEnabled(
	const bool bEnabled)
{
	if (bReducedEffectsPresentation == bEnabled)
	{
		return;
	}
	bReducedEffectsPresentation = bEnabled;
	BroadcastPresentationSnapshot();
}

FSovCorruptionCheckpointData
USovLegacyCorruptionComponent::CaptureCorruptionCheckpoint(
	const bool bCanonCheckpoint)
{
	if (CanWriteCorruption() && !ActiveSources.IsEmpty())
	{
		EvaluatePendingInstantExposures();
		EvaluateAccumulatedSources();
	}

	FSovCorruptionCheckpointData Data;
	Data.MaxCorruption = GetMaxCorruption();
	const bool bCaptureExposure = bSaveExposureAtCheckpoints
		&& (!bCanonCheckpoint || bPersistExposureAcrossCanonCheckpoints);
	if (bCaptureExposure)
	{
		Data.Corruption = GetCorruption();
		Data.bOverwriteRiskAuthorizedForCurrentExposure =
			bOverwriteRiskAuthorizedForCurrentExposure;
		Data.RemedyTag = RememberedRemedyTag;
		Data.RemedyInstruction = RememberedRemedyInstruction;
		Data.PresentationProfile = RememberedPresentationProfile;
		Data.AccessibilitySubstitute = RememberedAccessibilitySubstitute;
		for (const FName OutstandingGuard : InstantExposureReplayGuardSourceIds)
		{
			if (OutstandingGuard != NAME_None)
			{
				Data.InstantExposureReplayGuardSourceIds.AddUnique(
					OutstandingGuard);
			}
		}
	}

	for (const TPair<FGuid, FSovActiveSource>& Pair : ActiveSources)
	{
		const FSovCorruptionSourceSpec& Spec = Pair.Value.Spec;
		if (bCaptureExposure)
		{
			Data.InstantExposureReplayGuardSourceIds.AddUnique(Spec.SourceId);
		}
		if (Spec.bSaveAtCheckpoint && Spec.SourceId != NAME_None
			&& (!bCanonCheckpoint || Spec.bPersistsAcrossCanonCheckpoints))
		{
			Data.ActiveSourceIds.AddUnique(Spec.SourceId);
			if (Spec.bPersistsAcrossCanonCheckpoints)
			{
				Data.CanonPersistentSourceIds.AddUnique(Spec.SourceId);
			}
		}
	}
	const TArray<FName>& PendingReconstructionIds = bCanonCheckpoint
		? RestoredCanonPersistentSourceIds
		: RestoredSourceIds;
	for (const FName SourceId : PendingReconstructionIds)
	{
		if (SourceId != NAME_None)
		{
			Data.ActiveSourceIds.AddUnique(SourceId);
		}
	}
	for (const FName SourceId : RestoredCanonPersistentSourceIds)
	{
		if (SourceId != NAME_None && Data.ActiveSourceIds.Contains(SourceId))
		{
			Data.CanonPersistentSourceIds.AddUnique(SourceId);
		}
	}
	Data.ActiveSourceIds.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});
	Data.CanonPersistentSourceIds.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});
	Data.InstantExposureReplayGuardSourceIds.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});
	return Data;
}

void USovLegacyCorruptionComponent::FinalizeCorruptionSourceRestore()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClearInstantReplayGuards();
		RestoredSourceIds.Reset();
		RestoredCanonPersistentSourceIds.Reset();
	}
}

bool USovLegacyCorruptionComponent::RestoreCorruptionCheckpoint(
	const FSovCorruptionCheckpointData& CheckpointData)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()
		|| CheckpointData.Version != FSovCorruptionCheckpointData::CurrentVersion
		|| !FMath::IsFinite(CheckpointData.Corruption)
		|| !FMath::IsFinite(CheckpointData.MaxCorruption)
		|| CheckpointData.Corruption < 0.0f
		|| CheckpointData.MaxCorruption < 1.0f)
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	if (!IsInitialized() || (NarrativeASC && NarrativeASC->IsDead()))
	{
		PendingCheckpointRestore = CheckpointData;
		bHasPendingCheckpointRestore = true;
		return true;
	}
	return ApplyCheckpointNow(CheckpointData);
}

void USovLegacyCorruptionComponent::TryInitializeFromOwner()
{
	if (!IsValid(GetOwner()))
	{
		return;
	}

	if (UAbilitySystemComponent* OwnerASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
	{
		if (OwnerASC != AbilitySystemComponent || !HasCurrentAvatarBinding())
		{
			InitializeWithAbilitySystem(OwnerASC);
		}
	}
}

void USovLegacyCorruptionComponent::HandleOwnerASCInitialized()
{
	TryInitializeFromOwner();
}

void USovLegacyCorruptionComponent::HandleDeathStateChanged(
	AActor* ChangedActor,
	UNarrativeAbilitySystemComponent* ChangedASC,
	const bool bIsDead)
{
	if (ChangedActor != GetOwner()
		|| ChangedASC != AbilitySystemComponent
		|| !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (bIsDead)
	{
		ResetForDeath();
		return;
	}

	if (bHasPendingCheckpointRestore)
	{
		const FSovCorruptionCheckpointData PendingData =
			PendingCheckpointRestore;
		bHasPendingCheckpointRestore = false;
		PendingCheckpointRestore = FSovCorruptionCheckpointData();
		ApplyCheckpointNow(PendingData);
	}
	else
	{
		RefreshBand(false);
		ApplyBandEffect();
		SchedulePendingInstantExposureEvaluation();
		RefreshSourceTimer();
		RefreshPresentationState();
		BroadcastPresentationSnapshot();
	}
}

void USovLegacyCorruptionComponent::UninitializeFromAbilitySystem()
{
	StopSourceTimer();
	ClearPendingInstantExposures();
	ActiveSources.Reset();
	RemoveBandEffect();

	if (!IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent = nullptr;
		CorruptionChangedDelegateHandle.Reset();
		MaxCorruptionChangedDelegateHandle.Reset();
		return;
	}

	if (CorruptionChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				USovCorruptionAttributeSet::GetCorruptionAttribute())
			.Remove(CorruptionChangedDelegateHandle);
	}
	if (MaxCorruptionChangedDelegateHandle.IsValid())
	{
		AbilitySystemComponent
			->GetGameplayAttributeValueChangeDelegate(
				USovCorruptionAttributeSet::GetMaxCorruptionAttribute())
			.Remove(MaxCorruptionChangedDelegateHandle);
	}
	if (UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent))
	{
		NarrativeASC->OnDeathStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}

	AbilitySystemComponent = nullptr;
	CorruptionChangedDelegateHandle.Reset();
	MaxCorruptionChangedDelegateHandle.Reset();
}

bool USovLegacyCorruptionComponent::HasCurrentAvatarBinding() const
{
	return IsValid(AbilitySystemComponent)
		&& IsValid(GetOwner())
		&& AbilitySystemComponent->GetAvatarActor() == GetOwner();
}

bool USovLegacyCorruptionComponent::CanWriteCorruption(const bool bAllowDead) const
{
	if (!IsInitialized() || !GetOwner() || !GetOwner()->HasAuthority()
		|| SovLegacyCorruptionIsolation::HasActiveCampaign(GetOwner()))
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	return bAllowDead || !NarrativeASC || !NarrativeASC->IsDead();
}

void USovLegacyCorruptionComponent::HandleCorruptionAttributeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	if (!HasCurrentAvatarBinding())
	{
		return;
	}

	const float OldValue = FMath::Max(ChangeData.OldValue, 0.0f);
	const float NewValue = FMath::Clamp(
		ChangeData.NewValue,
		0.0f,
		GetMaxCorruption());

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (NewValue <= 0.0f)
		{
			bOverwriteRiskAuthorizedForCurrentExposure = false;
			ClearRememberedPresentationContract();
		}
		else if (NewValue
			< GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk))
		{
			bOverwriteRiskAuthorizedForCurrentExposure = false;
		}
		RefreshBand(!bSuppressNotifications);
		RefreshPresentationState();
		if (!bSuppressNotifications)
		{
			SendCorruptionEvent(
				FSovGameplayTags::Get().Event_Corruption_ExposureChanged,
				NewValue,
				GetOwner());
		}
	}

	if (!bSuppressNotifications)
	{
		OnCorruptionChanged.Broadcast(OldValue, NewValue, GetMaxCorruption());
		BroadcastPresentationSnapshot();
	}
}

void USovLegacyCorruptionComponent::HandleMaxCorruptionAttributeChanged(
	const FOnAttributeChangeData& ChangeData)
{
	if (!HasCurrentAvatarBinding())
	{
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RefreshBand(!bSuppressNotifications);
		RefreshPresentationState();
	}
	if (!bSuppressNotifications)
	{
		OnCorruptionChanged.Broadcast(
			GetCorruption(),
			GetCorruption(),
			FMath::Max(ChangeData.NewValue, 1.0f));
		BroadcastPresentationSnapshot();
	}
}

void USovLegacyCorruptionComponent::SetCorruptionInternal(
	const float NewCorruption,
	const bool bAllowDead)
{
	if (!CanWriteCorruption(bAllowDead))
	{
		return;
	}
	AbilitySystemComponent->SetNumericAttributeBase(
		USovCorruptionAttributeSet::GetCorruptionAttribute(),
		FMath::Clamp(NewCorruption, 0.0f, GetMaxCorruption()));
	WakeForReplication();
}

void USovLegacyCorruptionComponent::SetMaxCorruptionInternal(
	const float NewMaxCorruption,
	const bool bAllowDead)
{
	if (!CanWriteCorruption(bAllowDead))
	{
		return;
	}
	AbilitySystemComponent->SetNumericAttributeBase(
		USovCorruptionAttributeSet::GetMaxCorruptionAttribute(),
		FMath::Max(NewMaxCorruption, 1.0f));
	WakeForReplication();
}

void USovLegacyCorruptionComponent::RefreshSourceTimer()
{
	if (!CanWriteCorruption())
	{
		StopSourceTimer();
		return;
	}

	bool bHasContinuousSource = false;
	for (const TPair<FGuid, FSovActiveSource>& Pair : ActiveSources)
	{
		if (Pair.Value.Spec.ExposurePerSecond > 0.0f)
		{
			bHasContinuousSource = true;
			break;
		}
	}
	if (!bHasContinuousSource)
	{
		StopSourceTimer();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& TimerManager = World->GetTimerManager();
	if (!TimerManager.IsTimerActive(SourceEvaluationTimerHandle))
	{
		LastSourceEvaluationWorldTime = World->GetTimeSeconds();
		TimerManager.SetTimer(
			SourceEvaluationTimerHandle,
			this,
			&ThisClass::EvaluateSourceTimer,
			FMath::Max(SourceEvaluationInterval, 0.02f),
			true);
	}
}

void USovLegacyCorruptionComponent::StopSourceTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SourceEvaluationTimerHandle);
		LastSourceEvaluationWorldTime = World->GetTimeSeconds();
	}
	else
	{
		LastSourceEvaluationWorldTime = 0.0f;
	}
}

void USovLegacyCorruptionComponent::EvaluateSourceTimer()
{
	// Keep same-frame one-shot impulses ahead of elapsed continuous exposure even
	// when a hitch makes both timers due in the same engine tick.
	EvaluatePendingInstantExposures();
	EvaluateAccumulatedSources();
}

void USovLegacyCorruptionComponent::EvaluateAccumulatedSources(const bool bAllowDead)
{
	if (!CanWriteCorruption(bAllowDead))
	{
		StopSourceTimer();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float CurrentWorldTime = World->GetTimeSeconds();
	const float ElapsedSeconds = FMath::Max(
		CurrentWorldTime - LastSourceEvaluationWorldTime,
		0.0f);
	LastSourceEvaluationWorldTime = CurrentWorldTime;

	struct FSourceContribution
	{
		FName SourceId = NAME_None;
		float Rate = 0.0f;
		float Ceiling = 0.0f;
		bool bAuthorizesOverwriteRisk = false;
	};

	TArray<FGuid> InvalidSourceIds;
	TArray<FSourceContribution, TInlineAllocator<8>> Contributions;
	const float StartingCorruption = GetCorruption();
	for (const TPair<FGuid, FSovActiveSource>& Pair : ActiveSources)
	{
		const FGuid& SourceId = Pair.Key;
		const FSovActiveSource& Source = Pair.Value;
		if (!Source.SourceActor.IsValid()
			|| Source.SourceActor->GetWorld() != GetWorld()
			|| !Source.Spec.HasValidNumbers())
		{
			InvalidSourceIds.Add(SourceId);
			continue;
		}
		if (ElapsedSeconds <= 0.0f
			|| Source.Spec.ExposurePerSecond <= 0.0f
			|| !SourceAllowsTarget(Source)
			|| !SourceHasLineOfSight(Source))
		{
			continue;
		}

		const float Strength = CalculateSourceStrength(Source);
		const float EffectiveRate = Source.Spec.ExposurePerSecond * Strength;
		if (EffectiveRate <= 0.0f)
		{
			continue;
		}

		float SourceCeiling = Source.Spec.bEnforceBandCap
			? GetExposureCeilingForBand(Source.Spec.BandCap)
			: GetMaxCorruption();
		const bool bMayEnterOverwrite = bMissionAllowsOverwriteRisk
			&& Source.Spec.bAuthorizesOverwriteRisk;
		if (!bMayEnterOverwrite)
		{
			SourceCeiling = FMath::Min(
				SourceCeiling,
				GetExposureCeilingForBand(ESovLegacyCorruptionBand::Contest));
		}

		if (StartingCorruption < SourceCeiling)
		{
			FSourceContribution& Contribution =
				Contributions.AddDefaulted_GetRef();
			Contribution.SourceId = Source.Spec.SourceId;
			Contribution.Rate = EffectiveRate;
			Contribution.Ceiling = SourceCeiling;
			Contribution.bAuthorizesOverwriteRisk = bMayEnterOverwrite;
		}
	}

	for (const FGuid& InvalidSourceId : InvalidSourceIds)
	{
		ActiveSources.Remove(InvalidSourceId);
	}
	Contributions.Sort([](
		const FSourceContribution& A,
		const FSourceContribution& B)
	{
		return A.SourceId.LexicalLess(B.SourceId);
	});

	// Integrate piecewise through each authored ceiling. Once a source reaches
	// its cap its rate leaves the sum for the remaining elapsed time. This makes
	// one long timer step equivalent to many short steps and is independent of
	// runtime handle or registration order.
	float CandidateCorruption = StartingCorruption;
	float RemainingSeconds = ElapsedSeconds;
	bool bAuthorizedOverwriteContribution = false;
	while (RemainingSeconds > 0.0f)
	{
		float CombinedRate = 0.0f;
		float NearestCeiling = TNumericLimits<float>::Max();
		bool bAuthorizedRateActive = false;
		for (const FSourceContribution& Contribution : Contributions)
		{
			if (Contribution.Ceiling
				<= CandidateCorruption)
			{
				continue;
			}
			CombinedRate += Contribution.Rate;
			NearestCeiling = FMath::Min(
				NearestCeiling,
				Contribution.Ceiling);
			bAuthorizedRateActive |=
				Contribution.bAuthorizesOverwriteRisk;
		}
		if (CombinedRate <= 0.0f
			|| NearestCeiling == TNumericLimits<float>::Max())
		{
			break;
		}

		const float SecondsToCeiling = FMath::Max(
			(NearestCeiling - CandidateCorruption) / CombinedRate,
			0.0f);
		if (RemainingSeconds <= SecondsToCeiling)
		{
			CandidateCorruption += CombinedRate * RemainingSeconds;
			bAuthorizedOverwriteContribution |= bAuthorizedRateActive;
			RemainingSeconds = 0.0f;
		}
		else
		{
			CandidateCorruption = NearestCeiling;
			RemainingSeconds -= SecondsToCeiling;
			bAuthorizedOverwriteContribution |= bAuthorizedRateActive;
		}
	}
	CandidateCorruption = FMath::Clamp(
		CandidateCorruption,
		StartingCorruption,
		GetMaxCorruption());
	if (bAuthorizedOverwriteContribution
		&& CandidateCorruption
			>= GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk))
	{
		bOverwriteRiskAuthorizedForCurrentExposure = true;
	}
	if (CandidateCorruption > GetCorruption())
	{
		SetCorruptionInternal(CandidateCorruption, bAllowDead);
	}

	RefreshSourceTimer();
	RefreshPresentationState();
}

void USovLegacyCorruptionComponent::SchedulePendingInstantExposureEvaluation()
{
	UWorld* World = GetWorld();
	if (!World || PendingInstantSourceHandles.IsEmpty())
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (!TimerManager.IsTimerActive(PendingInstantExposureTimerHandle))
	{
		PendingInstantExposureTimerHandle = TimerManager.SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::EvaluatePendingInstantExposures));
	}
}

void USovLegacyCorruptionComponent::EvaluatePendingInstantExposures()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingInstantExposureTimerHandle);
	}
	if (PendingInstantSourceHandles.IsEmpty())
	{
		return;
	}

	TArray<FGuid> PendingHandles = PendingInstantSourceHandles.Array();
	PendingInstantSourceHandles.Reset();
	if (!CanWriteCorruption())
	{
		return;
	}

	struct FInstantContribution
	{
		FName SourceId = NAME_None;
		float Amount = 0.0f;
		float Ceiling = 0.0f;
		bool bAuthorizesOverwriteRisk = false;
	};

	TArray<FInstantContribution, TInlineAllocator<8>> Contributions;
	const float StartingCorruption = GetCorruption();
	for (const FGuid& PendingHandle : PendingHandles)
	{
		const FSovActiveSource* Source = ActiveSources.Find(PendingHandle);
		if (!Source
			|| !Source->SourceActor.IsValid()
			|| Source->SourceActor->GetWorld() != GetWorld()
			|| !Source->Spec.HasValidNumbers()
			|| Source->Spec.InstantExposure <= 0.0f
			|| !SourceAllowsTarget(*Source)
			|| !SourceHasLineOfSight(*Source))
		{
			continue;
		}

		const float Amount =
			Source->Spec.InstantExposure * CalculateSourceStrength(*Source);
		if (Amount <= 0.0f)
		{
			continue;
		}

		float Ceiling = Source->Spec.bEnforceBandCap
			? GetExposureCeilingForBand(Source->Spec.BandCap)
			: GetMaxCorruption();
		const bool bMayEnterOverwrite = bMissionAllowsOverwriteRisk
			&& Source->Spec.bAuthorizesOverwriteRisk;
		if (!bMayEnterOverwrite)
		{
			Ceiling = FMath::Min(
				Ceiling,
				GetExposureCeilingForBand(ESovLegacyCorruptionBand::Contest));
		}
		if (StartingCorruption >= Ceiling)
		{
			continue;
		}

		FInstantContribution& Contribution = Contributions.AddDefaulted_GetRef();
		Contribution.SourceId = Source->Spec.SourceId;
		Contribution.Amount = Amount;
		Contribution.Ceiling = Ceiling;
		Contribution.bAuthorizesOverwriteRisk = bMayEnterOverwrite;
	}

	Contributions.Sort([](
		const FInstantContribution& A,
		const FInstantContribution& B)
	{
		return A.SourceId.LexicalLess(B.SourceId);
	});

	// Treat same-frame instant payloads as simultaneous one-second impulses.
	// Piecewise removal at each authored ceiling makes the outcome independent
	// of BeginPlay/overlap registration order while preserving each source cap.
	float CandidateCorruption = StartingCorruption;
	float RemainingImpulse = 1.0f;
	bool bAuthorizedOverwriteContribution = false;
	while (RemainingImpulse > 0.0f)
	{
		float CombinedAmount = 0.0f;
		float NearestCeiling = TNumericLimits<float>::Max();
		bool bAuthorizedContributionActive = false;
		for (const FInstantContribution& Contribution : Contributions)
		{
			if (Contribution.Ceiling <= CandidateCorruption)
			{
				continue;
			}
			CombinedAmount += Contribution.Amount;
			NearestCeiling = FMath::Min(NearestCeiling, Contribution.Ceiling);
			bAuthorizedContributionActive |=
				Contribution.bAuthorizesOverwriteRisk;
		}
		if (CombinedAmount <= 0.0f
			|| NearestCeiling == TNumericLimits<float>::Max())
		{
			break;
		}

		const float ImpulseToCeiling = FMath::Max(
			(NearestCeiling - CandidateCorruption) / CombinedAmount,
			0.0f);
		if (RemainingImpulse <= ImpulseToCeiling)
		{
			CandidateCorruption += CombinedAmount * RemainingImpulse;
			bAuthorizedOverwriteContribution |=
				bAuthorizedContributionActive;
			RemainingImpulse = 0.0f;
		}
		else
		{
			CandidateCorruption = NearestCeiling;
			RemainingImpulse -= ImpulseToCeiling;
			bAuthorizedOverwriteContribution |=
				bAuthorizedContributionActive;
		}
	}

	CandidateCorruption = FMath::Clamp(
		CandidateCorruption,
		StartingCorruption,
		GetMaxCorruption());
	if (bAuthorizedOverwriteContribution
		&& CandidateCorruption
			>= GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk))
	{
		bOverwriteRiskAuthorizedForCurrentExposure = true;
	}
	if (CandidateCorruption > StartingCorruption)
	{
		SetCorruptionInternal(CandidateCorruption);
	}
	RefreshPresentationState();
}

void USovLegacyCorruptionComponent::ClearPendingInstantExposures()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingInstantExposureTimerHandle);
	}
	PendingInstantSourceHandles.Reset();
}

float USovLegacyCorruptionComponent::CalculateSourceStrength(
	const FSovActiveSource& ActiveSource,
	FVector* OutDirection) const
{
	const AActor* SourceActor = ActiveSource.SourceActor.Get();
	const AActor* TargetActor = GetOwner();
	if (!IsValid(SourceActor) || !IsValid(TargetActor))
	{
		if (OutDirection)
		{
			*OutDirection = FVector::ZeroVector;
		}
		return 0.0f;
	}

	const FVector ToSource =
		SourceActor->GetActorLocation() - TargetActor->GetActorLocation();
	if (OutDirection)
	{
		*OutDirection = ToSource.GetSafeNormal();
	}
	if (ActiveSource.Spec.FalloffPolicy == ESovCorruptionFalloffPolicy::None)
	{
		return 1.0f;
	}

	const float Distance = ToSource.Size();
	if (Distance <= ActiveSource.Spec.InnerRadius)
	{
		return 1.0f;
	}
	if (Distance >= ActiveSource.Spec.OuterRadius)
	{
		return 0.0f;
	}
	return 1.0f - FMath::Clamp(
		(Distance - ActiveSource.Spec.InnerRadius)
			/ (ActiveSource.Spec.OuterRadius - ActiveSource.Spec.InnerRadius),
		0.0f,
		1.0f);
}

bool USovLegacyCorruptionComponent::SourceAllowsTarget(
	const FSovActiveSource& ActiveSource) const
{
	if (!IsInitialized() || ActiveSource.Spec.AllowedTargetQuery.IsEmpty())
	{
		return IsInitialized();
	}

	FGameplayTagContainer OwnedTags;
	AbilitySystemComponent->GetOwnedGameplayTags(OwnedTags);
	return ActiveSource.Spec.AllowedTargetQuery.Matches(OwnedTags);
}

bool USovLegacyCorruptionComponent::SourceHasLineOfSight(
	const FSovActiveSource& ActiveSource) const
{
	if (!ActiveSource.Spec.bRequireLineOfSight)
	{
		return true;
	}
	const AActor* SourceActor = ActiveSource.SourceActor.Get();
	const AActor* TargetActor = GetOwner();
	UWorld* World = GetWorld();
	if (!IsValid(SourceActor) || !IsValid(TargetActor) || !World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SovCorruptionOcclusion), false);
	QueryParams.AddIgnoredActor(SourceActor);
	QueryParams.AddIgnoredActor(TargetActor);
	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit,
		SourceActor->GetActorLocation(),
		TargetActor->GetActorLocation(),
		ActiveSource.Spec.OcclusionTraceChannel,
		QueryParams);
}

float USovLegacyCorruptionComponent::ApplyExposureInternal(
	const float ExposureAmount,
	const ESovLegacyCorruptionBand BandCap,
	const bool bEnforceBandCap,
	const bool bSourceAuthorizesOverwriteRisk,
	const FSovCorruptionSourceSpec* PresentationSpec,
	AActor* SourceActor)
{
	if (!CanWriteCorruption() || !FMath::IsFinite(ExposureAmount)
		|| ExposureAmount <= 0.0f)
	{
		return 0.0f;
	}

	float ExposureCeiling = bEnforceBandCap
		? GetExposureCeilingForBand(BandCap)
		: GetMaxCorruption();
	const bool bMayEnterOverwriteRisk = bMissionAllowsOverwriteRisk
		&& bSourceAuthorizesOverwriteRisk;
	if (!bMayEnterOverwriteRisk)
	{
		ExposureCeiling = FMath::Min(
			ExposureCeiling,
			GetExposureCeilingForBand(ESovLegacyCorruptionBand::Contest));
	}

	const float OldCorruption = GetCorruption();
	const float NewCorruption = FMath::Min(
		OldCorruption + ExposureAmount,
		ExposureCeiling);
	if (NewCorruption <= OldCorruption)
	{
		return 0.0f;
	}

	if (PresentationSpec)
	{
		RememberPresentationContract(*PresentationSpec);
	}
	if (bMayEnterOverwriteRisk
		&& NewCorruption
			>= GetThresholdForBand(ESovLegacyCorruptionBand::OverwriteRisk))
	{
		bOverwriteRiskAuthorizedForCurrentExposure = true;
	}
	SetCorruptionInternal(NewCorruption);

	if (IsValid(SourceActor) && SourceActor != GetOwner()
		&& ActiveSources.IsEmpty())
	{
		RememberedSourceDirection =
			(SourceActor->GetActorLocation() - GetOwner()->GetActorLocation())
			.GetSafeNormal();
		bHasRememberedSourceDirection =
			!RememberedSourceDirection.IsNearlyZero();
	}
	RefreshPresentationState();
	return GetCorruption() - OldCorruption;
}

void USovLegacyCorruptionComponent::RefreshBand(const bool bBroadcastChanges)
{
	if (!IsInitialized())
	{
		return;
	}
	SetBand(DetermineBandForExposure(GetCorruption()), bBroadcastChanges);
}

void USovLegacyCorruptionComponent::SetBand(
	const ESovLegacyCorruptionBand NewBand,
	const bool bBroadcastChanges)
{
	if (CurrentBand == NewBand)
	{
		return;
	}

	const ESovLegacyCorruptionBand OldBand = CurrentBand;
	RemoveBandEffect();
	CurrentBand = NewBand;
	ApplyBandEffect();
	WakeForReplication();

	if (bBroadcastChanges)
	{
		OnCorruptionBandChanged.Broadcast(OldBand, NewBand);
		SendCorruptionEvent(
			FSovGameplayTags::Get().Event_Corruption_BandChanged,
			static_cast<float>(NewBand),
			GetOwner());
		if (NewBand == ESovLegacyCorruptionBand::OverwriteRisk)
		{
			OnOverwriteRiskEntered.Broadcast();
			SendCorruptionEvent(
				FSovGameplayTags::Get().Event_Corruption_OverwriteRiskReached,
				GetCorruption(),
				GetOwner());
		}
	}
	if (!bSuppressNotifications)
	{
		BroadcastPresentationSnapshot();
	}
}

void USovLegacyCorruptionComponent::RemoveBandEffect()
{
	if (ActiveBandEffectHandle.IsValid()
		&& IsValid(AbilitySystemComponent)
		&& GetOwner() && GetOwner()->HasAuthority())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(
			ActiveBandEffectHandle,
			-1);
	}
	ActiveBandEffectHandle = FActiveGameplayEffectHandle();
	ActiveBandEffectTag = FGameplayTag();
}

void USovLegacyCorruptionComponent::ApplyBandEffect()
{
	if (!CanWriteCorruption()
		|| CurrentBand == ESovLegacyCorruptionBand::None
		|| ActiveBandEffectHandle.IsValid()
		|| !BandStateEffectClass)
	{
		return;
	}

	const FGameplayTag BandTag = GetTagForBand(CurrentBand);
	if (!BandTag.IsValid())
	{
		return;
	}
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		BandStateEffectClass,
		1.0f,
		AbilitySystemComponent->MakeEffectContext());
	if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
	{
		Spec->DynamicGrantedTags.AddTag(BandTag);
		Spec->DynamicGrantedTags.AddTag(
			FSovGameplayTags::Get().State_Status_Corrupted);
		ActiveBandEffectHandle =
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec);
		if (ActiveBandEffectHandle.IsValid())
		{
			ActiveBandEffectTag = BandTag;
		}
	}
}

FGameplayTag USovLegacyCorruptionComponent::GetTagForBand(
	const ESovLegacyCorruptionBand Band) const
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	switch (Band)
	{
	case ESovLegacyCorruptionBand::Trace:
		return Tags.State_Corruption_Trace;
	case ESovLegacyCorruptionBand::Intrusion:
		return Tags.State_Corruption_Intrusion;
	case ESovLegacyCorruptionBand::Contest:
		return Tags.State_Corruption_Contest;
	case ESovLegacyCorruptionBand::OverwriteRisk:
		return Tags.State_Corruption_OverwriteRisk;
	case ESovLegacyCorruptionBand::None:
	default:
		return FGameplayTag();
	}
}

float USovLegacyCorruptionComponent::GetThresholdForBand(
	const ESovLegacyCorruptionBand Band) const
{
	const float IntrusionFraction = FMath::Clamp(
		IntrusionThresholdFraction,
		0.0f,
		1.0f);
	const float ContestFraction = FMath::Clamp(
		FMath::Max(ContestThresholdFraction, IntrusionFraction),
		0.0f,
		1.0f);
	const float OverwriteFraction = FMath::Clamp(
		FMath::Max(OverwriteRiskThresholdFraction, ContestFraction),
		0.0f,
		1.0f);

	float Fraction = 0.0f;
	switch (Band)
	{
	case ESovLegacyCorruptionBand::Trace:
		return 0.0f;
	case ESovLegacyCorruptionBand::Intrusion:
		Fraction = IntrusionFraction;
		break;
	case ESovLegacyCorruptionBand::Contest:
		Fraction = ContestFraction;
		break;
	case ESovLegacyCorruptionBand::OverwriteRisk:
		Fraction = OverwriteFraction;
		break;
	case ESovLegacyCorruptionBand::None:
	default:
		return 0.0f;
	}
	return GetMaxCorruption() * Fraction;
}

void USovLegacyCorruptionComponent::RefreshPresentationState()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FSovCorruptionReplicatedPresentationState NewState = PresentationState;
	const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(AbilitySystemComponent);
	const bool bOwnerIsDead = NarrativeASC && NarrativeASC->IsDead();
	NewState.SourceDirection = bOwnerIsDead
		? FVector::ZeroVector
		: RememberedSourceDirection;
	NewState.bHasDirectionalSource = !bOwnerIsDead
		&& bHasRememberedSourceDirection;
	NewState.RemedyTag = RememberedRemedyTag;
	NewState.RemedyInstruction = RememberedRemedyInstruction;
	NewState.PresentationProfile = RememberedPresentationProfile;
	NewState.AccessibilitySubstitute = RememberedAccessibilitySubstitute;

	const FSovActiveSource* BestSource = nullptr;
	float BestScore = 0.0f;
	FName BestSourceId = NAME_None;
	FVector BestDirection = FVector::ZeroVector;
	for (const TPair<FGuid, FSovActiveSource>& Pair : ActiveSources)
	{
		if (bOwnerIsDead)
		{
			break;
		}
		const FSovActiveSource& Source = Pair.Value;
		if (!Source.SourceActor.IsValid()
			|| !SourceAllowsTarget(Source)
			|| !SourceHasLineOfSight(Source))
		{
			continue;
		}
		FVector Direction;
		const float Strength = CalculateSourceStrength(Source, &Direction);
		const float Score = Strength * FMath::Max(
			Source.Spec.ExposurePerSecond,
			Source.Spec.InstantExposure > 0.0f ? 0.01f : 0.0f);
		if (Score <= 0.0f)
		{
			continue;
		}
		const bool bWinsStableTie = BestSource
			&& FMath::IsNearlyEqual(Score, BestScore)
			&& Source.Spec.SourceId.LexicalLess(BestSourceId);
		if (!BestSource
			|| Score > BestScore + KINDA_SMALL_NUMBER
			|| bWinsStableTie)
		{
			BestScore = Score;
			BestSource = &Source;
			BestSourceId = Source.Spec.SourceId;
			BestDirection = Direction;
		}
	}

	if (BestSource)
	{
		NewState.SourceDirection = BestDirection;
		NewState.bHasDirectionalSource = !BestDirection.IsNearlyZero();
		NewState.RemedyTag = BestSource->Spec.RemedyTag;
		NewState.RemedyInstruction = BestSource->Spec.RemedyInstruction;
		NewState.PresentationProfile = BestSource->Spec.PresentationProfile;
		NewState.AccessibilitySubstitute =
			BestSource->Spec.AccessibilitySubstitute;
		if (GetCorruption() > 0.0f)
		{
			RememberPresentationContract(BestSource->Spec);
		}
	}

	const bool bChanged =
		NewState.bHasDirectionalSource != PresentationState.bHasDirectionalSource
		|| !NewState.SourceDirection.Equals(
			PresentationState.SourceDirection,
			0.01f)
		|| !NewState.RemedyTag.MatchesTagExact(PresentationState.RemedyTag)
		|| !NewState.RemedyInstruction.EqualTo(PresentationState.RemedyInstruction)
		|| NewState.PresentationProfile != PresentationState.PresentationProfile
		|| !NewState.AccessibilitySubstitute.EqualTo(
			PresentationState.AccessibilitySubstitute);
	if (!bChanged)
	{
		return;
	}

	NewState.Revision = PresentationState.Revision + 1;
	PresentationState = MoveTemp(NewState);
	WakeForReplication();
	if (!bSuppressNotifications)
	{
		BroadcastPresentationSnapshot();
	}
}

void USovLegacyCorruptionComponent::RememberPresentationContract(
	const FSovCorruptionSourceSpec& SourceSpec)
{
	RememberedRemedyTag = SourceSpec.RemedyTag;
	RememberedRemedyInstruction = SourceSpec.RemedyInstruction;
	RememberedPresentationProfile = SourceSpec.PresentationProfile;
	RememberedAccessibilitySubstitute = SourceSpec.AccessibilitySubstitute;
}

void USovLegacyCorruptionComponent::ClearRememberedPresentationContract()
{
	RememberedRemedyTag = FGameplayTag();
	RememberedRemedyInstruction = FText::GetEmpty();
	RememberedPresentationProfile = NAME_None;
	RememberedAccessibilitySubstitute = FText::GetEmpty();
	RememberedSourceDirection = FVector::ZeroVector;
	bHasRememberedSourceDirection = false;
}

void USovLegacyCorruptionComponent::BroadcastPresentationSnapshot()
{
	OnCorruptionPresentationChanged.Broadcast(GetPresentationSnapshot());
}

void USovLegacyCorruptionComponent::WakeForReplication() const
{
	TArray<AActor*, TInlineAllocator<2>> ReplicationOwners;
	if (IsValid(GetOwner()))
	{
		ReplicationOwners.Add(GetOwner());
	}
	if (IsValid(AbilitySystemComponent)
		&& IsValid(AbilitySystemComponent->GetOwnerActor()))
	{
		ReplicationOwners.AddUnique(AbilitySystemComponent->GetOwnerActor());
	}
	for (AActor* ReplicationOwner : ReplicationOwners)
	{
		if (ReplicationOwner->HasAuthority())
		{
			ReplicationOwner->FlushNetDormancy();
			ReplicationOwner->ForceNetUpdate();
		}
	}
}

void USovLegacyCorruptionComponent::SendCorruptionEvent(
	const FGameplayTag EventTag,
	const float Magnitude,
	AActor* Instigator) const
{
	if (!EventTag.IsValid() || !IsValid(GetOwner()))
	{
		return;
	}
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = IsValid(Instigator) ? Instigator : GetOwner();
	Payload.Target = GetOwner();
	Payload.OptionalObject = this;
	Payload.EventMagnitude = Magnitude;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwner(),
		EventTag,
		Payload);
}

void USovLegacyCorruptionComponent::ResetForDeath()
{
	if (!CanWriteCorruption(true))
	{
		return;
	}

	const float OldCorruption = GetCorruption();
	const ESovLegacyCorruptionBand OldBand = CurrentBand;
	bSuppressNotifications = true;
	if (bResetCorruptionOnDeath)
	{
		ClearPendingInstantExposures();
		ActiveSources.Reset();
		ClearInstantReplayGuards();
		RestoredSourceIds.Reset();
		RestoredCanonPersistentSourceIds.Reset();
	}
	else
	{
		// The death delegate runs after the ASC marks this avatar dead. Settle the
		// final sub-tick of continuous exposure before pausing retained sources so
		// repeated deaths cannot systematically evade that elapsed contribution.
		EvaluateAccumulatedSources(true);
		UWorld* World = GetWorld();
		if (World)
		{
			// Retained exposure keeps its already-registered live sources. Pause their
			// timers while dead, then resume the same handles on revive so one-shot
			// exposure cannot replay merely because the pawn entered the death state.
			World->GetTimerManager().ClearTimer(PendingInstantExposureTimerHandle);
		}
	}
	StopSourceTimer();
	RemoveBandEffect();
	bOverwriteRiskAuthorizedForCurrentExposure = false;
	RememberedSourceDirection = FVector::ZeroVector;
	bHasRememberedSourceDirection = false;
	float SafePostDeathCorruption = FMath::Clamp(
		bResetCorruptionOnDeath ? DeathResetCorruption : GetCorruption(),
		0.0f,
		GetMaxCorruption());
	// Death clears source authorization, so neither a configured fallback nor
	// retained exposure can remain numerically inside Overwrite Risk. Contest
	// additionally requires the full player-facing presentation contract.
	SafePostDeathCorruption = FMath::Min(
		SafePostDeathCorruption,
		GetExposureCeilingForBand(ESovLegacyCorruptionBand::Contest));
	const bool bHasPresentationContract = RememberedRemedyTag.IsValid()
		&& !RememberedRemedyInstruction.IsEmpty()
		&& RememberedPresentationProfile != NAME_None
		&& !RememberedAccessibilitySubstitute.IsEmpty();
	if (!bHasPresentationContract)
	{
		SafePostDeathCorruption = FMath::Min(
			SafePostDeathCorruption,
			GetExposureCeilingForBand(ESovLegacyCorruptionBand::Intrusion));
	}
	SetCorruptionInternal(SafePostDeathCorruption, true);
	if (SafePostDeathCorruption <= 0.0f)
	{
		ClearRememberedPresentationContract();
	}
	RefreshBand(false);
	// Corruption may retain a nonzero configurable fallback, but dead avatars
	// own no active band effect. Revive/current-avatar readiness reapplies it.
	RemoveBandEffect();
	RefreshPresentationState();
	bSuppressNotifications = false;
	if (OldBand != CurrentBand)
	{
		OnCorruptionBandChanged.Broadcast(OldBand, CurrentBand);
	}
	OnCorruptionChanged.Broadcast(
		OldCorruption,
		GetCorruption(),
		GetMaxCorruption());
	BroadcastPresentationSnapshot();
}

bool USovLegacyCorruptionComponent::ApplyCheckpointNow(
	const FSovCorruptionCheckpointData& CheckpointData)
{
	// Every caller must already have deferred while dead. Keep this private
	// mutation path fail-closed so a future call site cannot put band effects
	// back onto a dead avatar.
	if (!CanWriteCorruption())
	{
		return false;
	}

	const float OldCorruption = GetCorruption();
	const ESovLegacyCorruptionBand OldBand = CurrentBand;
	bSuppressNotifications = true;
	ClearPendingInstantExposures();
	ActiveSources.Reset();
	StopSourceTimer();
	RemoveBandEffect();
	SetMaxCorruptionInternal(CheckpointData.MaxCorruption, true);
	bOverwriteRiskAuthorizedForCurrentExposure =
		CheckpointData.bOverwriteRiskAuthorizedForCurrentExposure
		&& bMissionAllowsOverwriteRisk;

	float RestoredCorruption = FMath::Clamp(
		CheckpointData.Corruption,
		0.0f,
		GetMaxCorruption());
	const FGameplayTag CleanseRoot = FSovGameplayTags::Get().Status_Cleanse;
	const bool bHasRestoredContestContract =
		CheckpointData.RemedyTag.IsValid()
		&& CleanseRoot.IsValid()
		&& CheckpointData.RemedyTag != CleanseRoot
		&& CheckpointData.RemedyTag.MatchesTag(CleanseRoot)
		&& !CheckpointData.RemedyInstruction.IsEmpty()
		&& CheckpointData.PresentationProfile != NAME_None
		&& !CheckpointData.AccessibilitySubstitute.IsEmpty();
	if (RestoredCorruption
			>= GetThresholdForBand(ESovLegacyCorruptionBand::Contest)
		&& !bHasRestoredContestContract)
	{
		RestoredCorruption = FMath::Min(
			RestoredCorruption,
			GetExposureCeilingForBand(ESovLegacyCorruptionBand::Intrusion));
		bOverwriteRiskAuthorizedForCurrentExposure = false;
	}
	if (!bOverwriteRiskAuthorizedForCurrentExposure)
	{
		RestoredCorruption = FMath::Min(
			RestoredCorruption,
			GetExposureCeilingForBand(ESovLegacyCorruptionBand::Contest));
	}
	if (bHasRestoredContestContract)
	{
		RememberedRemedyTag = CheckpointData.RemedyTag;
		RememberedRemedyInstruction = CheckpointData.RemedyInstruction;
		RememberedPresentationProfile = CheckpointData.PresentationProfile;
		RememberedAccessibilitySubstitute =
			CheckpointData.AccessibilitySubstitute;
	}
	else
	{
		ClearRememberedPresentationContract();
	}
	RememberedSourceDirection = FVector::ZeroVector;
	bHasRememberedSourceDirection = false;
	const auto SanitizeSourceIds = [](const TArray<FName>& CandidateIds)
	{
		TArray<FName> SanitizedIds;
		for (const FName CandidateId : CandidateIds)
		{
			if (CandidateId != NAME_None)
			{
				SanitizedIds.AddUnique(CandidateId);
			}
		}
		SanitizedIds.Sort([](const FName A, const FName B)
		{
			return A.LexicalLess(B);
		});
		return SanitizedIds;
	};
	RestoredSourceIds = SanitizeSourceIds(CheckpointData.ActiveSourceIds);
	RestoredCanonPersistentSourceIds = SanitizeSourceIds(
		CheckpointData.CanonPersistentSourceIds);
	for (const FName CanonPersistentSourceId :
		RestoredCanonPersistentSourceIds)
	{
		RestoredSourceIds.AddUnique(CanonPersistentSourceId);
	}
	RestoredSourceIds.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});
	InstantExposureReplayGuardSourceIds = SanitizeSourceIds(
		CheckpointData.InstantExposureReplayGuardSourceIds);
	ScheduleInstantReplayGuardWarning();
	SetCorruptionInternal(RestoredCorruption, true);
	RefreshBand(false);
	ApplyBandEffect();
	RefreshPresentationState();
	bSuppressNotifications = false;
	if (OldBand != CurrentBand)
	{
		OnCorruptionBandChanged.Broadcast(OldBand, CurrentBand);
	}
	OnCorruptionChanged.Broadcast(
		OldCorruption,
		GetCorruption(),
		GetMaxCorruption());
	BroadcastPresentationSnapshot();
	return true;
}

void USovLegacyCorruptionComponent::ScheduleInstantReplayGuardWarning()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InstantReplayGuardTimerHandle);
		if (!InstantExposureReplayGuardSourceIds.IsEmpty())
		{
			World->GetTimerManager().SetTimer(
				InstantReplayGuardTimerHandle,
				this,
				&ThisClass::HandleInstantReplayGuardWarning,
				FMath::Max(InstantReplayGuardWarningDelay, 0.1f),
				false);
		}
	}
}

void USovLegacyCorruptionComponent::HandleInstantReplayGuardWarning()
{
	if (!InstantExposureReplayGuardSourceIds.IsEmpty())
	{
		UE_LOG(
			LogSovCorruption,
			Warning,
			TEXT("%s still has %d checkpoint instant-exposure replay guard(s). Reconstruct saved sources, then call FinalizeCorruptionSourceRestore; guards remain active to prevent timing-dependent duplicate exposure."),
			*GetNameSafe(GetOwner()),
			InstantExposureReplayGuardSourceIds.Num());
	}
}

void USovLegacyCorruptionComponent::ClearInstantReplayGuards()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InstantReplayGuardTimerHandle);
	}
	InstantExposureReplayGuardSourceIds.Reset();
}

void USovLegacyCorruptionComponent::OnRep_CurrentBand(
	const ESovLegacyCorruptionBand OldBand)
{
	if (OldBand != CurrentBand)
	{
		OnCorruptionBandChanged.Broadcast(OldBand, CurrentBand);
	}
	BroadcastPresentationSnapshot();
}

void USovLegacyCorruptionComponent::OnRep_PresentationState(
	const FSovCorruptionReplicatedPresentationState& OldState)
{
	BroadcastPresentationSnapshot();
}
