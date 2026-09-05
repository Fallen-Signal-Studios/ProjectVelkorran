// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/NarrativeNPCController.h"
#include "AI/NarrativeThreatPolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

namespace
{
	constexpr int32 MaximumObservations = 128;
	bool IsFinitePosition(const FVector& Position)
	{
		return !Position.ContainsNaN() && FMath::IsFinite(Position.X)
			&& FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
	}
	float SourceConfidenceCap(const ENarrativeThreatSource Source)
	{
		switch (Source)
		{
		case ENarrativeThreatSource::Sight: return 1.f;
		case ENarrativeThreatSource::Damage: return .9f;
		case ENarrativeThreatSource::NetworkSensor: return .8f;
		case ENarrativeThreatSource::EchoCorruption: return .8f;
		case ENarrativeThreatSource::Hearing: return .5f;
		case ENarrativeThreatSource::AllyAlert: return .55f;
		case ENarrativeThreatSource::Command: return .6f;
		case ENarrativeThreatSource::ViewmakerSpoof: return .4f;
		default: return 0.f;
		}
	}
}

bool ANarrativeNPCController::IsThreatMemoryManaged() const
{
	return bRequireThreatMemoryForTargeting || bThreatMemoryManaged || IsValid(GetPerceptionComponent());
}

bool ANarrativeNPCController::IsThreatTargetEligible(AActor* Target) const
{
	if (!IsValid(Target) || Target == GetPawn() || !GetPawn() || !GetWorld()
		|| IsThreatMemorySuspended() || GetPawn()->IsHidden() || Target->IsHidden()
		|| Target->GetWorld() != GetWorld() || Target->IsActorBeingDestroyed()
		|| UArsenalStatics::GetAttitude(GetPawn(), Target) != ETeamAttitude::Hostile) { return false; }
	if (const UAbilitySystemComponent* OwnerASC = GetAbilitySystemComponent())
	{
		if (OwnerASC->GetAvatarActor() != GetPawn()
			|| OwnerASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			|| (OwnerASC->GetSet<UNarrativeAttributeSetBase>()
				&& OwnerASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= KINDA_SMALL_NUMBER)) { return false; }
	}
	if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		return ASC->GetAvatarActor() == Target && !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
			&& (!ASC->GetSet<UNarrativeAttributeSetBase>()
				|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > KINDA_SMALL_NUMBER);
	}
	return true; // Hostile objective actors need not be characters or have GAS.
}

bool ANarrativeNPCController::IsThreatTargetCloaked(AActor* Target) const
{
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	return ASC && ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies);
}

bool ANarrativeNPCController::IsThreatPerceptionReady() const
{
	const UAIPerceptionComponent* Component = GetPerceptionComponent();
	// AI Perception normally uses event-driven sensing. Tick-disabled only implies suspension
	// for a subclass that actually declares a component tick; campaign holds an explicit lease.
	return Component && Component->IsRegistered() && Component->IsActive()
		&& (!Component->PrimaryComponentTick.bCanEverTick || Component->IsComponentTickEnabled())
		&& Component->IsSenseEnabled(UAISense_Sight::StaticClass());
}

void ANarrativeNPCController::BindThreatPerception()
{
	UAIPerceptionComponent* Current = GetPerceptionComponent();
	if (ThreatPerception.Get() == Current) { return; }
	if (ThreatPerception.IsValid())
	{
		ThreatPerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandleThreatPerception);
		ThreatPerception->OnComponentActivated.RemoveDynamic(this, &ThisClass::HandleThreatPerceptionActivated);
		ThreatPerception->OnComponentDeactivated.RemoveDynamic(this, &ThisClass::HandleThreatPerceptionDeactivated);
	}
	ThreatPerception = Current;
	bThreatPerceptionWasReady = IsThreatPerceptionReady();
	if (Current && HasAuthority())
	{
		bThreatMemoryManaged = true;
		Current->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ThisClass::HandleThreatPerception);
		Current->OnComponentActivated.AddUniqueDynamic(this, &ThisClass::HandleThreatPerceptionActivated);
		Current->OnComponentDeactivated.AddUniqueDynamic(this, &ThisClass::HandleThreatPerceptionDeactivated);
	}
}

void ANarrativeNPCController::HandleThreatPerceptionActivated(UActorComponent* Component, const bool bReset)
{
	if (Component == ThreatPerception.Get()) { InvalidateCachedThreatPerception(); }
}

void ANarrativeNPCController::HandleThreatPerceptionDeactivated(UActorComponent* Component)
{
	if (Component == ThreatPerception.Get()) { InvalidateCachedThreatPerception(); }
}

void ANarrativeNPCController::InvalidateCachedThreatPerception()
{
	if (!HasAuthority() || bInvalidatingThreatPerception) { return; }
	TGuardValue<bool> InvalidationGuard(bInvalidatingThreatPerception, true);
	const uint64 Generation = ++ThreatMemoryGeneration;
	const TWeakObjectPtr<ANarrativeNPCController> Self = this;
	const TWeakObjectPtr<APawn> OriginalPawn = GetPawn();
	bThreatPerceptionWasReady = IsThreatPerceptionReady();
	for (FNarrativeThreatMemory& Memory : ThreatMemory)
	{
		if (Memory.Source == ENarrativeThreatSource::Sight) { Memory.bDirectObservation = false; }
	}
	// Re-activation cannot reinterpret a retained successful stimulus as a new sight
	// observation. Own finite memories remain available for investigation.
	if (ThreatPerception.IsValid()) { ThreatPerception->ForgetAll(); }
	if (!Self.IsValid() || Self->IsActorBeingDestroyed() || OriginalPawn.IsStale()
		|| Self->GetPawn() != OriginalPawn.Get() || Self->ThreatMemoryGeneration != Generation) { return; }
	ClearInvalidThreatTarget();
}

void ANarrativeNPCController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority()) { return; }
	ThreatUpdateAccumulator += DeltaSeconds;
	// Supporting tiers can throttle their existing perception component. Do not poll faster.
	const float Interval = GetPerceptionComponent()
		? FMath::Max(.2f, GetPerceptionComponent()->GetComponentTickInterval()) : .2f;
	if (ThreatUpdateAccumulator >= Interval)
	{
		ThreatUpdateAccumulator = 0.f;
		RefreshThreatMemory();
	}
}

bool ANarrativeNPCController::ReportThreatObservation(AActor* Target, const ENarrativeThreatSource Source,
	const FVector Position, const float Strength, const float Confidence, const float Lifetime)
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	if (!HasAuthority() || !IsThreatTargetEligible(Target) || !IsFinitePosition(Position)
		|| !NarrativeThreat::ValidObservation(Now, Strength, Confidence, Lifetime)
		|| SourceConfidenceCap(Source) <= 0.f || Source == ENarrativeThreatSource::AllyAlert
		|| (Source == ENarrativeThreatSource::NetworkSensor && !bAcceptNetworkThreats)
		|| (Source == ENarrativeThreatSource::EchoCorruption && !bAcceptEchoThreats)) { return false; }
	if (Source == ENarrativeThreatSource::Sight
		&& (IsThreatTargetCloaked(Target) || (GetPerceptionComponent() && !IsThreatPerceptionReady()))) { return false; }
	bThreatMemoryManaged = true;
	++ThreatMemoryGeneration;
	ThreatMemory.RemoveAll([Now](const FNarrativeThreatMemory& Memory)
	{ return !Memory.Target.IsValid() || Now >= Memory.ExpiresAt; });
	FNarrativeThreatMemory* Existing = ThreatMemory.FindByPredicate([Target, Source](const FNarrativeThreatMemory& Memory)
	{ return Memory.Target.Get() == Target && Memory.Source == Source; });
	if (!Existing)
	{
		if (ThreatMemory.Num() >= MaximumObservations) { return false; }
		Existing = &ThreatMemory.AddDefaulted_GetRef();
	}
	Existing->Target = Target;
	Existing->Source = Source;
	Existing->Strength = FMath::Min(Strength, 10.f);
	Existing->Confidence = FMath::Min(Confidence, SourceConfidenceCap(Source));
	Existing->LastKnownPosition = Position;
	Existing->ObservedAt = Now;
	Existing->ExpiresAt = Now + Lifetime;
	Existing->bDirectObservation = Source == ENarrativeThreatSource::Sight || Source == ENarrativeThreatSource::Damage
		|| Source == ENarrativeThreatSource::NetworkSensor || Source == ENarrativeThreatSource::EchoCorruption;
	Existing->SharedBy.Reset();
	Existing->SharedFactions.Reset();
	return true;
}

void ANarrativeNPCController::HandleThreatPerception(AActor* Actor, FAIStimulus Stimulus)
{
	if (!HasAuthority()) { return; }
	ENarrativeThreatSource Source;
	float Lifetime = 8.f;
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>()) { Source = ENarrativeThreatSource::Sight; }
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>()) { Source = ENarrativeThreatSource::Hearing; Lifetime = 6.f; }
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>()) { Source = ENarrativeThreatSource::Damage; Lifetime = 4.f; }
	else { return; } // Unknown senses require an explicit producer adapter, not an implicit sight upgrade.
	if (Stimulus.WasSuccessfullySensed() && !Stimulus.IsExpired())
	{
		ReportThreatObservation(Actor, Source, Stimulus.StimulusLocation, Stimulus.Strength, 1.f, Lifetime);
	}
	else
	{
		++ThreatMemoryGeneration;
		for (FNarrativeThreatMemory& Memory : ThreatMemory)
		{
			if (Memory.Target.Get() == Actor && Memory.Source == Source) { Memory.bDirectObservation = false; }
		}
	}
	ClearInvalidThreatTarget();
}

TArray<FNarrativeThreatMemory> ANarrativeNPCController::GetThreatDebugSnapshot() const
{
	TArray<FNarrativeThreatMemory> Result;
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	for (const FNarrativeThreatMemory& Memory : ThreatMemory)
	{
		if (!IsThreatTargetEligible(Memory.Target.Get())) { continue; }
		FNarrativeThreatMemory Copy = Memory;
		Copy.Confidence = NarrativeThreat::ConfidenceAt(Memory.Confidence, Memory.ObservedAt, Memory.ExpiresAt, Now);
		Copy.bDirectObservation &= !IsThreatTargetCloaked(Memory.Target.Get());
		if (Copy.Confidence > 0.f) { Result.Add(Copy); }
	}
	return Result;
}

bool ANarrativeNPCController::GetBestThreatMemory(AActor* Target, FNarrativeThreatMemory& OutMemory) const
{
	OutMemory = FNarrativeThreatMemory();
	double BestScore = 0.0;
	for (const FNarrativeThreatMemory& Memory : GetThreatDebugSnapshot())
	{
		if (Target && Memory.Target.Get() != Target) { continue; }
		const double Score = NarrativeThreat::Score(Memory.Strength, Memory.Confidence);
		if (Score > BestScore || (Score == BestScore && Memory.ObservedAt > OutMemory.ObservedAt))
		{
			BestScore = Score;
			OutMemory = Memory;
		}
	}
	return BestScore > 0.0;
}

bool ANarrativeNPCController::CanDirectlyTargetThreat(AActor* Target) const
{
	if (!IsThreatTargetEligible(Target) || IsThreatTargetCloaked(Target)) { return false; }
	if (!IsThreatMemoryManaged()) { return true; } // Preserve authored nonperception NPC controllers.
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	for (const FNarrativeThreatMemory& Memory : ThreatMemory)
	{
		if (Memory.Source == ENarrativeThreatSource::Sight && GetPerceptionComponent() && !IsThreatPerceptionReady()) { continue; }
		if (Memory.Target.Get() == Target && NarrativeThreat::CanDirectTarget(
			NarrativeThreat::ConfidenceAt(Memory.Confidence, Memory.ObservedAt, Memory.ExpiresAt, Now),
			Memory.bDirectObservation, false)) { return true; }
	}
	return false;
}

bool ANarrativeNPCController::ShareThreatWith(ANarrativeNPCController* Recipient, AActor* Target)
{
	if (!HasAuthority() || !IsValid(Recipient) || Recipient == this || !Recipient->HasAuthority()
		|| Recipient->GetWorld() != GetWorld() || !GetPawn() || !Recipient->GetPawn()
		|| !bShareThreatsWithFaction || !Recipient->bShareThreatsWithFaction
		|| !Recipient->IsThreatMemoryManaged()
		|| !IsThreatTargetEligible(Target) || !Recipient->IsThreatTargetEligible(Target)
		|| !FMath::IsFinite(ThreatShareRadius) || ThreatShareRadius <= 0.f
		|| FVector::DistSquared(GetPawn()->GetActorLocation(), Recipient->GetPawn()->GetActorLocation()) > FMath::Square(ThreatShareRadius)
		|| UArsenalStatics::GetAttitude(GetPawn(), Recipient->GetPawn()) != ETeamAttitude::Friendly
		|| UArsenalStatics::GetAttitude(Recipient->GetPawn(), GetPawn()) != ETeamAttitude::Friendly) { return false; }
	const FGameplayTagContainer SharedFactions = GetFactions().FilterExact(Recipient->GetFactions());
	if (SharedFactions.IsEmpty()) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	const FNarrativeThreatMemory* Observation = nullptr;
	double BestScore = 0.0;
	for (const FNarrativeThreatMemory& Memory : ThreatMemory)
	{
		// No alert rebroadcast chains, no spoof laundering, no fresh position from a live target pointer.
		if (Memory.Target.Get() != Target || Memory.Source == ENarrativeThreatSource::AllyAlert
			|| Memory.Source == ENarrativeThreatSource::ViewmakerSpoof || Memory.SharedBy.IsValid()) { continue; }
		const double Score = NarrativeThreat::Score(Memory.Strength,
			NarrativeThreat::ConfidenceAt(Memory.Confidence, Memory.ObservedAt, Memory.ExpiresAt, Now));
		if (Score > BestScore) { Observation = &Memory; BestScore = Score; }
	}
	if (!Observation) { return false; }
	const double Lifetime = NarrativeThreat::SharedLifetime(Observation->ExpiresAt, Now, 4.0);
	if (Lifetime <= 0.0) { return false; }
	Recipient->ThreatMemory.RemoveAll([Now](const FNarrativeThreatMemory& Memory)
	{ return !Memory.Target.IsValid() || Now >= Memory.ExpiresAt; });
	FNarrativeThreatMemory* Shared = Recipient->ThreatMemory.FindByPredicate([Target](const FNarrativeThreatMemory& Memory)
	{ return Memory.Target.Get() == Target && Memory.Source == ENarrativeThreatSource::AllyAlert; });
	if (!Shared)
	{
		if (Recipient->ThreatMemory.Num() >= MaximumObservations) { return false; }
		Shared = &Recipient->ThreatMemory.AddDefaulted_GetRef();
	}
	const float Confidence = FMath::Min(.55f, static_cast<float>(NarrativeThreat::ConfidenceAt(
		Observation->Confidence, Observation->ObservedAt, Observation->ExpiresAt, Now)) * .75f);
	// An older alert must not overwrite fresher, stronger information or extend its source lifetime.
	if (Shared->ObservedAt > Observation->ObservedAt && Shared->Confidence > Confidence) { return false; }
	*Shared = *Observation;
	Shared->Source = ENarrativeThreatSource::AllyAlert;
	Shared->Confidence = Confidence;
	Shared->ObservedAt = Now;
	Shared->ExpiresAt = Now + Lifetime;
	Shared->bDirectObservation = false;
	Shared->SharedBy = GetPawn();
	Shared->SharedFactions = SharedFactions;
	++Recipient->ThreatMemoryGeneration;
	return true;
}

void ANarrativeNPCController::ShareFreshThreats()
{
	if (!bShareThreatsWithFaction || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<TWeakObjectPtr<AActor>> Targets;
	for (FNarrativeThreatMemory& Memory : ThreatMemory)
	{
		if (Memory.Source == ENarrativeThreatSource::AllyAlert || Memory.Source == ENarrativeThreatSource::ViewmakerSpoof
			|| Memory.SharedBy.IsValid() || Now - Memory.LastSharedAt < 2.0) { continue; }
		Memory.LastSharedAt = Now;
		Targets.AddUnique(Memory.Target);
	}
	if (Targets.IsEmpty()) { return; }
	int32 SharedCount = 0;
	int32 InspectedControllers = 0;
	for (TActorIterator<ANarrativeNPCController> It(GetWorld()); It && SharedCount < 16 && InspectedControllers < 256; ++It, ++InspectedControllers)
	{
		for (const TWeakObjectPtr<AActor>& Target : Targets)
		{
			if (ShareThreatWith(*It, Target.Get())) { ++SharedCount; }
			if (SharedCount >= 16) { break; }
		}
	}
}

void ANarrativeNPCController::RefreshThreatMemory()
{
	if (!HasAuthority() || !GetWorld() || bRefreshingThreatMemory) { return; }
	TGuardValue<bool> RefreshGuard(bRefreshingThreatMemory, true);
	const TWeakObjectPtr<ANarrativeNPCController> Self = this;
	const uint64 RefreshAssignment = PawnAssignmentGeneration;
	const auto StillOwnsRefresh = [Self, RefreshAssignment]()
	{
		return Self.IsValid() && !Self->IsActorBeingDestroyed()
			&& Self->PawnAssignmentGeneration == RefreshAssignment;
	};
	BindThreatPerception();
	if (!IsThreatMemoryManaged()) { return; }
	if (IsThreatMemorySuspended() || (GetPawn() && GetPawn()->IsHidden())) { ClearThreatMemory(); return; }
	const bool bPerceptionReady = IsThreatPerceptionReady();
	if (ThreatPerception.IsValid() && bThreatPerceptionWasReady && !bPerceptionReady)
	{
		InvalidateCachedThreatPerception();
		if (!StillOwnsRefresh()) { return; }
	}
	bThreatPerceptionWasReady = bPerceptionReady;
	const UAbilitySystemComponent* OwnerASC = GetAbilitySystemComponent();
	if (!GetPawn() || (OwnerASC && OwnerASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)))
	{ ClearThreatMemory(); return; }
	TArray<AActor*> Visible;
	if (ThreatPerception.IsValid() && IsThreatPerceptionReady()
		&& ThreatPerception->IsSenseEnabled(UAISense_Sight::StaticClass()))
	{
		ThreatPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Visible);
	}
	for (FNarrativeThreatMemory& Memory : ThreatMemory)
	{
		if (Memory.Source == ENarrativeThreatSource::Sight && ThreatPerception.IsValid()
			&& (!Visible.Contains(Memory.Target.Get()) || IsThreatTargetCloaked(Memory.Target.Get())))
		{ Memory.bDirectObservation = false; }
	}
	for (AActor* Actor : Visible)
	{
		// Only current sight can sample the actor's new position. Hearing and lost sight retain their reported location.
		if (IsThreatTargetEligible(Actor) && !IsThreatTargetCloaked(Actor))
		{
			FActorPerceptionBlueprintInfo Information;
			if (!ThreatPerception->GetActorsPerception(Actor, Information)) { continue; }
			for (const FAIStimulus& Stimulus : Information.LastSensedStimuli)
			{
				if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>() && Stimulus.WasSuccessfullySensed() && !Stimulus.IsExpired())
				{ ReportThreatObservation(Actor, ENarrativeThreatSource::Sight, Actor->GetActorLocation(), Stimulus.Strength, 1.f, 8.f); }
			}
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<TWeakObjectPtr<AActor>> Removed;
	ThreatMemory.RemoveAll([this, Now, &Removed](const FNarrativeThreatMemory& Memory)
	{
		const bool bRemove = !IsThreatTargetEligible(Memory.Target.Get()) || Now >= Memory.ExpiresAt;
		if (bRemove) { Removed.AddUnique(Memory.Target); }
		return bRemove;
	});
	for (const TWeakObjectPtr<AActor>& Target : Removed)
	{
		FNarrativeThreatMemory Remaining;
		if (Target.IsValid() && ThreatPerception.IsValid() && !GetBestThreatMemory(Target.Get(), Remaining))
		{ ThreatPerception->ForgetActor(Target.Get()); }
		if (!StillOwnsRefresh()) { return; }
	}
	ClearInvalidThreatTarget();
	if (!StillOwnsRefresh()) { return; }
	ShareFreshThreats();
}

void ANarrativeNPCController::ClearInvalidThreatTarget()
{
	if (!IsThreatMemoryManaged() || IsThreatMemorySuspended() || bClearingThreatTarget) { return; }
	TGuardValue<bool> ClearGuard(bClearingThreatTarget, true);
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		// Dialogue and cinematic focus belong to their current Narrative activity.
		if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Interacting)
			|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)) { return; }
	}
	const UArsenalSettings* Settings = GetDefault<UArsenalSettings>();
	UBlackboardComponent* Blackboard = GetBlackboardComponent();
	const TWeakObjectPtr<ANarrativeNPCController> Self = this;
	const TWeakObjectPtr<APawn> OriginalPawn = GetPawn();
	const uint64 Generation = ThreatMemoryGeneration;
	const auto StillOwns = [Self, OriginalPawn, Blackboard, Generation]()
	{
		return Self.IsValid() && !Self->IsActorBeingDestroyed() && !OriginalPawn.IsStale()
			&& Self->GetPawn() == OriginalPawn.Get() && Self->GetBlackboardComponent() == Blackboard
			&& Self->ThreatMemoryGeneration == Generation && !Self->IsThreatMemorySuspended();
	};
	AActor* AttackTarget = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(Settings->BBKey_AttackTarget)) : nullptr;
	AActor* FocusTarget = GetFocusActor();
	const bool bFocusIsThreat = FocusTarget && (FocusTarget == AttackTarget
		|| FocusTarget == InvestigationTarget.Get()
		|| UArsenalStatics::GetAttitude(GetPawn(), FocusTarget) == ETeamAttitude::Hostile);
	AActor* Invalid = AttackTarget && !CanDirectlyTargetThreat(AttackTarget) ? AttackTarget
		: bFocusIsThreat && !CanDirectlyTargetThreat(FocusTarget) ? FocusTarget : nullptr;
	if (Invalid)
	{
		FNarrativeThreatMemory LastKnown;
		const bool bRemember = GetBestThreatMemory(Invalid, LastKnown);
		if (AttackTarget == Invalid && Blackboard) { Blackboard->ClearValue(Settings->BBKey_AttackTarget); }
		if (!StillOwns()) { return; }
		if (GetFocusActor() == Invalid) { ClearFocus(EAIFocusPriority::Gameplay); }
		if (!StillOwns()) { return; }
		const auto HasIndependentTarget = [this, Blackboard, Settings, Invalid]()
		{
			AActor* OtherFocus = GetFocusActor();
			AActor* OtherAttack = Blackboard ? Cast<AActor>(Blackboard->GetValueAsObject(Settings->BBKey_AttackTarget)) : nullptr;
			return (OtherFocus && OtherFocus != Invalid) || (OtherAttack && OtherAttack != Invalid && CanDirectlyTargetThreat(OtherAttack));
		};
		if (HasIndependentTarget()) { return; }
		StopMovement(); // Stop chasing a live hidden actor; authored activity may investigate the finite location.
		if (!StillOwns() || HasIndependentTarget()) { return; }
		if (bRemember)
		{
			InvestigationTarget = Invalid;
			InvestigationPosition = LastKnown.LastKnownPosition;
			bOwnsInvestigationLocation = true;
			if (Blackboard) { Blackboard->SetValueAsVector(Settings->BBKey_TargetLocation, InvestigationPosition); }
			if (!StillOwns() || HasIndependentTarget()) { return; }
			SetFocalPoint(InvestigationPosition, EAIFocusPriority::Gameplay);
		}
	}
	if (bOwnsInvestigationLocation)
	{
		FNarrativeThreatMemory Remaining;
		const bool bRemember = InvestigationTarget.IsValid() && GetBestThreatMemory(InvestigationTarget.Get(), Remaining);
		const bool bReacquired = InvestigationTarget.IsValid() && CanDirectlyTargetThreat(InvestigationTarget.Get());
		if (!bRemember || bReacquired)
		{
			if (Blackboard && Blackboard->GetValueAsVector(Settings->BBKey_TargetLocation).Equals(InvestigationPosition))
			{ Blackboard->ClearValue(Settings->BBKey_TargetLocation); }
			if (!StillOwns()) { return; }
			if (!GetFocusActor() && GetFocalPoint().Equals(InvestigationPosition)) { ClearFocus(EAIFocusPriority::Gameplay); }
			bOwnsInvestigationLocation = false;
			InvestigationTarget.Reset();
		}
	}
}

void ANarrativeNPCController::ForgetThreat(AActor* Target)
{
	if (!HasAuthority() || !IsValid(Target)) { return; }
	++ThreatMemoryGeneration;
	ThreatMemory.RemoveAll([Target](const FNarrativeThreatMemory& Memory) { return Memory.Target.Get() == Target; });
	if (ThreatPerception.IsValid()) { ThreatPerception->ForgetActor(Target); }
	ClearInvalidThreatTarget();
}

void ANarrativeNPCController::ClearThreatMemory()
{
	if (!HasAuthority()) { return; }
	const uint64 Generation = ++ThreatMemoryGeneration;
	const TWeakObjectPtr<ANarrativeNPCController> Self = this;
	const TWeakObjectPtr<APawn> OriginalPawn = GetPawn();
	ThreatMemory.Reset();
	if (ThreatPerception.IsValid()) { ThreatPerception->ForgetAll(); }
	if (!Self.IsValid() || Self->IsActorBeingDestroyed() || OriginalPawn.IsStale()
		|| Self->GetPawn() != OriginalPawn.Get() || Self->ThreatMemoryGeneration != Generation) { return; }
	ClearInvalidThreatTarget();
}

bool ANarrativeNPCController::IsThreatMemorySuspended() const
{
	return ThreatSuspensionOwners.ContainsByPredicate([](const TWeakObjectPtr<UObject>& Owner) { return Owner.IsValid(); });
}

void ANarrativeNPCController::SetThreatMemorySuspended(UObject* SuspensionOwner, const bool bSuspend)
{
	if (!HasAuthority() || !IsValid(SuspensionOwner)) { return; }
	ThreatSuspensionOwners.RemoveAll([](const TWeakObjectPtr<UObject>& Owner) { return !Owner.IsValid(); });
	if (bSuspend)
	{
		if (ThreatSuspensionOwners.Contains(SuspensionOwner)) { return; }
		ThreatSuspensionOwners.Add(SuspensionOwner);
		ClearThreatMemory();
	}
	else if (ThreatSuspensionOwners.Remove(SuspensionOwner) > 0)
	{
		++ThreatMemoryGeneration;
		ClearInvalidThreatTarget();
	}
}
