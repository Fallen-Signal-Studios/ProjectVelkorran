// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Campaign/SovEncounterCoordinationPolicy.h"
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovAurelionCrucibleDirector.h"
#include "Components/SovCommandLinkComponent.h"
#include "World/SovWorldTransitActor.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeCombatAbility.h"
#include "NarrativeGameplayTags.h"
#include "Perception/AIPerceptionComponent.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace
{
	ESovBotAttackPressure PressureOf(const UNarrativeCombatAbility* Ability)
	{
		if (!Ability) { return ESovBotAttackPressure::Support; }
		if (Ability->BotAttackPressure != ESovBotAttackPressure::Automatic) { return Ability->BotAttackPressure; }
		const float Range = Ability->GetBotAttackMaximumRange();
		return FMath::IsFinite(Range) && Range > 0.f && Range <= 600.f ? ESovBotAttackPressure::Melee : ESovBotAttackPressure::Ranged;
	}
	bool IsHostileTo(const AActor* Source, const AActor* Target)
	{
		const auto* Team = Cast<INarrativeTeamAgentInterface>(Source);
		return IsValid(Source) && IsValid(Target) && Team && Team->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
	}
	bool IsLivingTarget(const AActor* Target, const UWorld* World)
	{
		const auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true);
		const auto* NarrativeASC = Cast<UNarrativeAbilitySystemComponent>(ASC);
		return IsValid(Target) && Target->GetWorld() == World && !Target->IsActorBeingDestroyed()
			&& ASC && ASC->GetAvatarActor() == Target && (!NarrativeASC || !NarrativeASC->IsDead())
			&& ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f;
	}
	bool IsOwnedPresentationActor(const AActor* Actor, const ASovNPCCharacterBase* Character)
	{
		if (!IsValid(Actor) || !IsValid(Character) || Actor == Character || Actor->GetWorld() != Character->GetWorld()) { return false; }
		const AActor* Current = Actor->GetOwner();
		for (int32 Depth = 0; IsValid(Current) && Depth < 16; ++Depth)
		{
			if (Current == Character) { return true; }
			if (Current == Current->GetOwner()) { break; }
			Current = Current->GetOwner();
		}
		return false;
	}
	bool IsDecisionComponent(const UActorComponent* Component)
	{
		return Component && (Component->IsA<UBrainComponent>() || Component->IsA<UAIPerceptionComponent>());
	}
}

USovEncounterCoordinationComponent::USovEncounterCoordinationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(false);
}

void USovEncounterCoordinationComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeCoordination();
}

void USovEncounterCoordinationComponent::InitializeCoordination()
{
	Director = Cast<ASovEncounterDirector>(GetOwner());
	if (!Director.IsValid() || !Director->HasAuthority()) { SetComponentTickEnabled(false); return; }
	Director->OnEncounterStateChanged.AddUniqueDynamic(this, &ThisClass::HandleEncounterState);
	FString Error;
	bValidComposition = ValidateComposition(Error);
	SetComponentTickEnabled(true);
	if (Director->GetEncounterState() == ESovEncounterState::Active && BoundAttempt != Director->GetAttemptId())
	{ HandleEncounterState(ESovEncounterState::Inactive, ESovEncounterState::Active); }
}

void USovEncounterCoordinationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Director.IsValid()) { Director->OnEncounterStateChanged.RemoveDynamic(this, &ThisClass::HandleEncounterState); }
	ResetAttempt();
	Super::EndPlay(Reason);
}

FSovEncounterCompositionMember USovEncounterCoordinationComponent::Member(FName Id) const
{
	FSovEncounterCompositionMember Result; Result.ParticipantId = Id;
	for (const auto& Entry : Composition) { if (Entry.ParticipantId == Id) { Result = Entry; break; } }
	if (const auto* Tier = RuntimeTiers.Find(Id)) { Result.Tier = *Tier; }
	return Result;
}

bool USovEncounterCoordinationComponent::ValidateComposition(FString& Error) const
{
	Error.Reset();
	const auto* OwnerDirector = Cast<ASovEncounterDirector>(GetOwner());
	if (!OwnerDirector || MaximumCombatants < 1 || MaximumCombatants > 16 || MaximumSupporting < 0 || MaximumSupporting > 24
		|| MeleeAttackerSlots < 1 || MeleeAttackerSlots > MaximumCombatants + MaximumSupporting)
	{ Error = TEXT("Encounter decision budgets must respect A <= 16, B <= 24 and a positive bounded melee slot count."); return false; }
	const float Positive[] = { SupportingDecisionInterval, SupportingAttackInterval, ReliefDuration, ReliefCooldown, ReliefAttackInterval, WarningLeadSeconds };
	for (float Value : Positive)
	{
		if (!FMath::IsFinite(Value) || Value <= 0.f || Value > 120.f)
		{ Error = TEXT("Encounter coordination intervals must be finite, positive and at most 120 seconds."); return false; }
	}
	if (ReliefDuration > ReliefCooldown || WarningLeadSeconds < 0.25f)
	{ Error = TEXT("Relief must have a recovery interval and offscreen warnings need at least 0.25 seconds of lead time."); return false; }
	for (const auto& Quota : SimultaneousRoleQuotas)
	{
		if (static_cast<uint8>(Quota.Key) > static_cast<uint8>(ESovEncounterRole::Objective) || Quota.Value < 1 || Quota.Value > 40)
		{ Error = TEXT("Role quotas need a valid role and a positive bounded capacity."); return false; }
	}
	TSet<FName> Ids;
	TSet<const ASovNPCCharacterBase*> Actors;
	for (const auto& Entry : OwnerDirector->Participants)
	{
		const bool bMass = OwnerDirector->IsParticipantMassRepresented(Entry.ParticipantId);
		if (Entry.ParticipantId.IsNone() || Ids.Contains(Entry.ParticipantId)
			|| (!bMass && (!IsValid(Entry.Character) || Actors.Contains(Entry.Character) || Entry.Character->GetWorld() != GetWorld())))
		{ Error = TEXT("Composition requires unique registered participant identities and actors in this world."); return false; }
		const auto* ASC = Entry.Character ? Cast<UNarrativeAbilitySystemComponent>(Entry.Character->GetAbilitySystemComponent()) : nullptr;
		if (ASC && ASC->GetBotAttackCoordinator() && ASC->GetBotAttackCoordinator() != this)
		{ Error = TEXT("An NPC cannot belong to two encounter coordinators."); return false; }
		Ids.Add(Entry.ParticipantId); if (Entry.Character) { Actors.Add(Entry.Character); }
	}
	TSet<FName> Configured;
	TSet<int32> Waves;
	for (const auto& Entry : Composition)
	{
		if (!Ids.Contains(Entry.ParticipantId) || Configured.Contains(Entry.ParticipantId) || Entry.Wave < 0 || Entry.Wave > 63
			|| static_cast<uint8>(Entry.Tier) > static_cast<uint8>(ESovEncounterDecisionTier::Supporting)
			|| static_cast<uint8>(Entry.Role) > static_cast<uint8>(ESovEncounterRole::Objective))
		{ Error = TEXT("Composition entries require a unique registered ID, valid tier/role and a wave from 0 through 63."); return false; }
		Configured.Add(Entry.ParticipantId); Waves.Add(Entry.Wave);
	}
	if (!Composition.IsEmpty() && Configured.Num() != Ids.Num())
	{ Error = TEXT("An authored composition must classify every registered participant."); return false; }
	if (Composition.IsEmpty()) { Waves.Add(0); }
	for (int32 Wave = 0; Wave < Waves.Num(); ++Wave)
	{
		if (!Waves.Contains(Wave)) { Error = TEXT("Encounter waves must be contiguous and begin at zero."); return false; }
		int32 Combatants = 0, Supporting = 0, Required = 0;
		for (const auto& Participant : OwnerDirector->Participants)
		{
			const auto Entry = Member(Participant.ParticipantId);
			if (Entry.Wave != Wave) { continue; }
			if (!OwnerDirector->IsParticipantMassRepresented(Participant.ParticipantId))
			{ if (Entry.Tier == ESovEncounterDecisionTier::Combatant) { ++Combatants; } else { ++Supporting; } }
			if (Participant.bRequiredForVictory) { ++Required; }
		}
		if (Combatants > MaximumCombatants || Supporting > MaximumSupporting || (Waves.Num() > 1 && Required == 0))
		{ Error = TEXT("Each wave must fit the A/B budget; multiwave encounters require a defeat gate in every wave."); return false; }
	}
	TSet<int32> RuledWaves;
	for (const auto& Rule : WaveReleaseRules)
	{
		if (Rule.Wave <= 0 || !Waves.Contains(Rule.Wave) || RuledWaves.Contains(Rule.Wave)
			|| Rule.MaximumLivingReleasedHostiles < 0 || Rule.MaximumLivingReleasedHostiles > 40
			|| static_cast<uint8>(Rule.Condition) > static_cast<uint8>(ESovEncounterWaveCondition::AcceptedCrucibleLink))
		{ Error = TEXT("Event release rules require distinct existing future waves and bounded living-hostile ceilings."); return false; }
		RuledWaves.Add(Rule.Wave);
		if (Rule.Condition == ESovEncounterWaveCondition::CommandSourceDefeated)
		{
			const auto* Link = ResolveRuleLink(Rule);
			const FName SourceId = Link ? OwnerDirector->FindParticipantId(Link->GetCommandSource()) : NAME_None;
			const auto* Source = OwnerDirector->Participants.FindByPredicate([SourceId](const auto& Entry) { return Entry.ParticipantId == SourceId; });
			if (!Link || !Source || !Source->bRequiredForVictory || Member(SourceId).Wave >= Rule.Wave
				|| Member(Rule.CommandLinkParticipantId).Wave >= Rule.Wave || Rule.TransitDoor)
			{ Error = TEXT("Formation gates require an exact registered link and an earlier-wave required command source."); return false; }
			if (OwnerDirector->GetEncounterState() == ESovEncounterState::Inactive
				&& (!Link->IsCommandLinkActive() || !Link->HasValidCommandLinkConfiguration()))
			{ Error = TEXT("The authored formation must be active before capturing its entry checkpoint."); return false; }
		}
		else if (Rule.Condition == ESovEncounterWaveCondition::TransitDoorOpen)
		{
			if (!IsValid(Rule.TransitDoor) || Rule.TransitDoor->GetWorld() != GetWorld() || Rule.TransitDoor->TransitId.IsNone()
				|| Rule.TransitDoor->Kind != ESovWorldTransitKind::Door || !Rule.CommandLinkParticipantId.IsNone() || !Rule.CommandLinkComponentName.IsNone())
			{ Error = TEXT("Transit gates require a named native door in this world, without unrelated link bindings."); return false; }
		}
		else if (!OwnerDirector->IsA<ASovAurelionLinkPhaseDirector>() || Rule.TransitDoor
			|| !Rule.CommandLinkParticipantId.IsNone() || !Rule.CommandLinkComponentName.IsNone())
		{ Error = TEXT("First-link gates belong only to the native Crucible link-phase director."); return false; }
	}
	return true;
}

void USovEncounterCoordinationComponent::ResetAttempt()
{
	BoundAttempt.Invalidate(); BoundGeneration = 0; WaveActors.Reset(); BoundWaveRules.Reset(); Reservations.Reset(); Warnings.Reset(); NextAttackAt.Reset(); RuntimeTiers.Reset();
	const TArray<TWeakObjectPtr<UNarrativeAbilitySystemComponent>> OldASCs = MoveTemp(BoundASCs);
	for (auto ASC : OldASCs)
	{
		if (ASC.IsValid() && ASC->GetBotAttackCoordinator() == this) { ASC->SetBotAttackCoordinator(nullptr); }
	}
	TArray<FName> StagedIds; Staged.GetKeys(StagedIds);
	for (FName Id : StagedIds) { ReleaseStagedParticipant(Id); }
	for (const auto& Pair : PriorDecisionIntervals)
	{
		if (Pair.Key.IsValid() && FMath::IsNearlyEqual(Pair.Key->GetComponentTickInterval(), SupportingDecisionInterval))
		{ Pair.Key->SetComponentTickInterval(Pair.Value); }
	}
	PriorDecisionIntervals.Reset();
	CurrentWave = 0; ReliefUntil = 0.; NextReliefAt = 0.; NextReliefAttackAt = 0.; bReliefReported = false;
}

void USovEncounterCoordinationComponent::HandleEncounterState(ESovEncounterState Previous, ESovEncounterState Current)
{
	static_cast<void>(Previous);
	if (!Director.IsValid() || !Director->HasAuthority()) { return; }
	if (Current != ESovEncounterState::Active)
	{
		// Retain future-wave staging while failed/restoring. The director owns separate restore suspensions.
		Reservations.Reset(); Warnings.Reset(); ReliefUntil = 0.;
		if (bReliefReported) { bReliefReported = false; OnPressureChanged.Broadcast(false, 0.f); }
		return;
	}
	ResetAttempt();
	FString Error;
	bValidComposition = ValidateComposition(Error);
	if (!bValidComposition) { return; }
	BoundAttempt = Director->GetAttemptId(); BoundGeneration = Director->GetLifecycleGeneration();
	if (!BindWaveRules()) { bValidComposition = false; return; }
	RefreshComposition();
	if (IsWaveContextCurrent(BoundAttempt, BoundGeneration, CurrentWave))
	{ OnWaveChanged.Broadcast(CurrentWave); }
}

void USovEncounterCoordinationComponent::StageParticipant(FName Id, ASovNPCCharacterBase* Character)
{
	if (Staged.Contains(Id) || !IsValid(Character)) { return; }
	auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Character->GetAbilitySystemComponent());
	if (!ASC || ASC->GetAvatarActor() != Character) { return; }
	FStaged Saved; Saved.ASC = ASC; Saved.Character = Character;
	Saved.bHidden = Character->IsHidden(); Saved.bCollision = Character->GetActorEnableCollision();
	if (auto* Movement = Character->GetCharacterMovement())
	{
		Saved.MovementMode = Movement->MovementMode; Saved.CustomMovementMode = Movement->CustomMovementMode;
		Movement->StopMovementImmediately(); Movement->DisableMovement();
	}
	TInlineComponentArray<UActorComponent*> Components(Character);
	if (auto* AI = Cast<AAIController>(Character->GetController()))
	{
		AI->StopMovement();
		TInlineComponentArray<UActorComponent*> ControllerComponents(AI); Components.Append(ControllerComponents);
	}
	for (auto* Component : Components)
	{
		if ((IsDecisionComponent(Component) || Component->IsA<USkeletalMeshComponent>()) && Component->IsComponentTickEnabled())
		{ Saved.DisabledTicks.Add(Component); Component->SetComponentTickEnabled(false); }
	}
	Saved.ThreatController = Cast<ANarrativeNPCController>(Character->GetController());
	Staged.Add(Id, Saved); // Commit ownership before tag callbacks.
	Character->CharacterVisualInitialized.AddUniqueDynamic(this, &ThisClass::HandleStagedVisualReady);
	if (Saved.ThreatController.IsValid())
	{
		Saved.ThreatController->SetThreatMemorySuspended(this, true);
		const FStaged* Current = Staged.Find(Id);
		if (!Current || Current->Character.Get() != Character || !IsValid(Character) || !IsValid(ASC)
			|| ASC->GetAvatarActor() != Character) { return; }
	}
	Staged.FindChecked(Id).bOwnsBusy = true;
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
	FStaged* Live = Staged.Find(Id);
	if (!Live || Live->Character.Get() != Character || !IsValid(Character) || !IsValid(ASC)) { return; }
	Live->bOwnsInvulnerability = true;
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll);
	const FStaged* Current = Staged.Find(Id);
	if (Current && Current->Identity == Saved.Identity && IsValid(Character))
	{
		Character->SetActorHiddenInGame(true); Character->SetActorEnableCollision(false);
		Current = Staged.Find(Id);
		if (Current && Current->Identity == Saved.Identity) { RefreshStagedPresentation(Id); }
	}
}

void USovEncounterCoordinationComponent::HandleStagedVisualReady(ANarrativeCharacter* Character)
{
	TArray<FName> Ids; Staged.GetKeys(Ids);
	for (FName Id : Ids)
	{
		const FStaged* Current = Staged.Find(Id);
		if (Current && Current->Character.Get() == Character) { RefreshStagedPresentation(Id); }
	}
}

void USovEncounterCoordinationComponent::RefreshStagedPresentation(FName Id)
{
	FStaged* Saved = Staged.Find(Id);
	if (!Saved || !Saved->Character.IsValid() || !Saved->ASC.IsValid()
		|| Saved->ASC->GetAvatarActor() != Saved->Character.Get()) { return; }
	const FGuid Identity = Saved->Identity;
	ASovNPCCharacterBase* Character = Saved->Character.Get();
	TArray<AActor*> Actors;
	if (ANarrativeCharacterVisual* Visual = Character->GetCharacterVisual())
	{
		Actors.Add(Visual);
		Visual->GetAttachedActors(Actors, false, true);
	}
	Character->GetAttachedActors(Actors, false, true);
	for (AActor* Actor : Actors)
	{
		Saved = Staged.Find(Id);
		if (!Saved || Saved->Identity != Identity || Saved->Character.Get() != Character
			|| !Saved->ASC.IsValid() || Saved->ASC->GetAvatarActor() != Character) { return; }
		if (!IsOwnedPresentationActor(Actor, Character)) { continue; }
		if (!Saved->Presentation.ContainsByPredicate([Actor](const FStagedPresentation& Entry) { return Entry.Actor.Get() == Actor; }))
		{
			FStagedPresentation Entry; Entry.Actor = Actor; Entry.bHidden = Actor->IsHidden(); Entry.bCollision = Actor->GetActorEnableCollision();
			Saved->Presentation.Add(Entry); // Capture once, before any collision callbacks.
		}
		Actor->SetActorHiddenInGame(true);
		Saved = Staged.Find(Id);
		if (!Saved || Saved->Identity != Identity || !IsOwnedPresentationActor(Actor, Character)) { return; }
		Actor->SetActorEnableCollision(false);
	}
}

void USovEncounterCoordinationComponent::ReleaseStagedParticipant(FName Id)
{
	FStaged Saved;
	if (!Staged.RemoveAndCopyValue(Id, Saved)) { return; }
	if (Saved.Character.IsValid())
	{ Saved.Character->CharacterVisualInitialized.RemoveDynamic(this, &ThisClass::HandleStagedVisualReady); }
	// Reentrant restaging inherits the original baseline instead of capturing our still-hidden visual as its authored state.
	const auto TransferToReplacement = [&]()
	{
		FStaged* Replacement = Staged.Find(Id);
		if (!Replacement || Replacement->Character != Saved.Character || Replacement->ASC != Saved.ASC) { return false; }
		Replacement->bHidden = Saved.bHidden; Replacement->bCollision = Saved.bCollision;
		Replacement->MovementMode = Saved.MovementMode; Replacement->CustomMovementMode = Saved.CustomMovementMode;
		for (auto Component : Saved.DisabledTicks) { Replacement->DisabledTicks.AddUnique(Component); }
		for (const FStagedPresentation& Prior : Saved.Presentation)
		{
			if (FStagedPresentation* Current = Replacement->Presentation.FindByPredicate(
				[&](const FStagedPresentation& Entry) { return Entry.Actor == Prior.Actor; }))
			{ Current->bHidden = Prior.bHidden; Current->bCollision = Prior.bCollision; }
		}
		return true;
	};
	for (const FStagedPresentation& Entry : Saved.Presentation)
	{
		if (TransferToReplacement()) { break; }
		AActor* Actor = Entry.Actor.Get();
		if (!IsOwnedPresentationActor(Actor, Saved.Character.Get())) { continue; }
		Actor->SetActorHiddenInGame(Entry.bHidden);
		if (TransferToReplacement()) { break; }
		if (IsOwnedPresentationActor(Actor, Saved.Character.Get())) { Actor->SetActorEnableCollision(Entry.bCollision); }
	}
	TransferToReplacement();
	if (Saved.Character.IsValid() && !TransferToReplacement())
	{
		Saved.Character->SetActorHiddenInGame(Saved.bHidden); Saved.Character->SetActorEnableCollision(Saved.bCollision);
		if (auto* Movement = Saved.Character->GetCharacterMovement(); Movement && !TransferToReplacement())
		{
			if (Movement->MovementMode == MOVE_None) { Movement->SetMovementMode(static_cast<EMovementMode>(Saved.MovementMode), Saved.CustomMovementMode); }
		}
	}
	for (auto Component : Saved.DisabledTicks)
	{ if (TransferToReplacement()) { break; } if (Component.IsValid()) { Component->SetComponentTickEnabled(true); } }
	if (Saved.ASC.IsValid())
	{
		if (Saved.bOwnsBusy) { Saved.ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll); }
		if (Saved.ASC.IsValid() && Saved.bOwnsInvulnerability) { Saved.ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll); }
	}
	// A callback may have restaged this same participant. Keep that new lease intact.
	const FStaged* Replacement = Staged.Find(Id);
	const bool bLeaseRenewed = Replacement && Replacement->ThreatController == Saved.ThreatController && Replacement->Character == Saved.Character;
	if (!bLeaseRenewed && Saved.ThreatController.IsValid() && Saved.Character.IsValid()
		&& Saved.ThreatController->GetPawn() == Saved.Character.Get())
	{ Saved.ThreatController->SetThreatMemorySuspended(this, false); }
}

void USovEncounterCoordinationComponent::RefreshComposition()
{
	if (bRefreshing || !Director.IsValid() || !bValidComposition || Director->GetEncounterState() != ESovEncounterState::Active) { return; }
	TGuardValue<bool> Refreshing(bRefreshing, true);
	const FGuid Attempt = BoundAttempt; const uint64 Generation = BoundGeneration; const int32 Wave = CurrentWave;
	const auto Participants = Director->Participants;
	for (const auto& Participant : Participants)
	{
		if (!IsWaveContextCurrent(Attempt, Generation, Wave)) { return; }
		if (Director->IsParticipantMassRepresented(Participant.ParticipantId)) { continue; }
		auto* Character = Participant.Character.Get();
		if (!IsValid(Character) || !Character->IsAlive()) { continue; }
		auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Character->GetAbilitySystemComponent());
		if (!ASC || ASC->GetAvatarActor() != Character || !ASC->SetBotAttackCoordinator(this)) { bValidComposition = false; return; }
		BoundASCs.AddUnique(ASC);
		const auto Entry = Member(Participant.ParticipantId);
		if (Entry.Wave > CurrentWave) { StageParticipant(Participant.ParticipantId, Character); continue; }
		ReleaseStagedParticipant(Participant.ParticipantId);
		if (!IsWaveContextCurrent(Attempt, Generation, Wave) || !IsValid(Character)) { return; }
		TInlineComponentArray<UActorComponent*> Components(Character);
		if (auto* AI = Cast<AAIController>(Character->GetController()))
		{ TInlineComponentArray<UActorComponent*> ControllerComponents(AI); Components.Append(ControllerComponents); }
		for (auto* Component : Components)
		{
			if (!IsDecisionComponent(Component)) { continue; }
			if (Entry.Tier == ESovEncounterDecisionTier::Supporting)
			{
				if (!PriorDecisionIntervals.Contains(Component)) { PriorDecisionIntervals.Add(Component, Component->GetComponentTickInterval()); }
				Component->SetComponentTickInterval(FMath::Max(SupportingDecisionInterval, PriorDecisionIntervals.FindRef(Component)));
			}
			else
			{
				float Prior;
				if (PriorDecisionIntervals.RemoveAndCopyValue(Component, Prior) && FMath::IsNearlyEqual(Component->GetComponentTickInterval(), SupportingDecisionInterval))
				{ Component->SetComponentTickInterval(Prior); }
			}
		}
	}
}

bool USovEncounterCoordinationComponent::CanChangeRepresentation(FName Id) const
{
	if (!Director.IsValid() || !Director->HasAuthority() || !bValidComposition || bRefreshing || bReserving
		|| Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()
		|| Staged.Contains(Id) || Member(Id).Wave != CurrentWave) { return false; }
	const auto* Character = Director->GetParticipant(Id);
	const auto* ASC = Character ? Character->GetNarrativeAbilitySystemComponent() : nullptr;
	if (!ASC || !Character->IsAlive()) { return false; }
	for (const auto& Pair : Reservations)
	{ if (Pair.Value.ASC.Get() == ASC || Pair.Value.Target.Get() == Character) { return false; } }
	for (const auto& Spec : ASC->GetActivatableAbilities()) { if (Spec.IsActive()) { return false; } }
	return true;
}

bool USovEncounterCoordinationComponent::CanPromoteRepresentation(FName Id) const
{
	if (!Director.IsValid() || !bValidComposition || Director->GetEncounterState() != ESovEncounterState::Active
		|| BoundAttempt != Director->GetAttemptId() || Member(Id).Wave != CurrentWave) { return false; }
	const auto Tier = Member(Id).Tier;
	int32 Count = 0;
	for (const auto& Participant : Director->Participants)
	{
		if (Participant.ParticipantId != Id && !Director->IsParticipantMassRepresented(Participant.ParticipantId)
			&& IsValid(Participant.Character) && Participant.Character->IsAlive()
			&& Member(Participant.ParticipantId).Wave <= CurrentWave && Member(Participant.ParticipantId).Tier == Tier) { ++Count; }
	}
	return Count < (Tier == ESovEncounterDecisionTier::Combatant ? MaximumCombatants : MaximumSupporting);
}

void USovEncounterCoordinationComponent::ReleaseRepresentationActor(FName Id, ASovNPCCharacterBase* Character)
{
	if (!IsValid(Character)) { return; }
	auto* ASC = Character->GetNarrativeAbilitySystemComponent();
	if (ASC && ASC->GetBotAttackCoordinator() == this) { ASC->SetBotAttackCoordinator(nullptr); }
	BoundASCs.Remove(ASC); Warnings.Remove(Id); NextAttackAt.Remove(Id);
	// Stage transitions are rejected before conversion. Never release another future-wave owner's contribution.
	for (auto It = PriorDecisionIntervals.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Key()->GetOwner() == Character || It.Key()->GetOwner() == Character->GetController()) { It.RemoveCurrent(); }
	}
}

void USovEncounterCoordinationComponent::RefreshRepresentationBindings()
{
	FString Error; bValidComposition = ValidateComposition(Error);
	if (bValidComposition) { RefreshComposition(); }
}

bool USovEncounterCoordinationComponent::SetDecisionTier(FName ParticipantId, ESovEncounterDecisionTier Tier)
{
	if (!Director.IsValid() || !Director->HasAuthority() || bRefreshing || bReserving || !bValidComposition
		|| Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()
		|| static_cast<uint8>(Tier) > static_cast<uint8>(ESovEncounterDecisionTier::Supporting)) { return false; }
	ASovNPCCharacterBase* Character = Director->GetParticipant(ParticipantId);
	if (!IsValid(Character) || !Character->IsAlive() || Member(ParticipantId).Wave > CurrentWave) { return false; }
	const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Character->GetAbilitySystemComponent());
	if (!ASC) { return false; }
	for (const auto& Reservation : Reservations) { if (Reservation.Value.ASC.Get() == ASC) { return false; } }
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{ if (Spec.IsActive() && Cast<UNarrativeCombatAbility>(Spec.Ability)) { return false; } }
	int32 Count = 0;
	for (const auto& Participant : Director->Participants)
	{
		if (Participant.ParticipantId != ParticipantId && IsValid(Participant.Character) && Participant.Character->IsAlive()
			&& Member(Participant.ParticipantId).Wave <= CurrentWave && Member(Participant.ParticipantId).Tier == Tier) { ++Count; }
	}
	if (!SovEncounterCoordinationPolicy::HasSlot(Count, Tier == ESovEncounterDecisionTier::Combatant ? MaximumCombatants : MaximumSupporting)) { return false; }
	RuntimeTiers.Add(ParticipantId, Tier);
	RefreshComposition();
	return bValidComposition;
}

bool USovEncounterCoordinationComponent::IsPressureReliefActive() const
{
	return Director.IsValid() && Director->HasAuthority() && GetWorld() && Director->GetEncounterState() == ESovEncounterState::Active
		&& BoundAttempt == Director->GetAttemptId() && GetWorld()->GetTimeSeconds() < ReliefUntil;
}

bool USovEncounterCoordinationComponent::IsBoundSource(UNarrativeAbilitySystemComponent* Source, FName& OutId) const
{
	OutId = NAME_None;
	if (!bValidComposition || !Director.IsValid() || !Director->HasAuthority() || !BoundAttempt.IsValid()
		|| Director->GetEncounterState() != ESovEncounterState::Active || Director->GetAttemptId() != BoundAttempt
		|| !IsValid(Source) || Source->GetBotAttackCoordinator() != this || !BoundASCs.Contains(Source)) { return false; }
	OutId = Director->FindParticipantId(Source->GetAvatarActor());
	if (Director->IsParticipantMassRepresented(OutId)) { return false; }
	const auto* Character = Director->GetParticipant(OutId);
	return IsValid(Character) && !Character->IsActorBeingDestroyed() && Character->IsAlive() && !Source->IsDead()
		&& Source->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f && Character->GetAbilitySystemComponent() == Source
		&& Member(OutId).Wave <= CurrentWave && !Staged.Contains(OutId);
}

bool USovEncounterCoordinationComponent::IsSourceOnscreen(const AActor* Source, const AActor* Target) const
{
	const auto* Player = Cast<APawn>(Target);
	const auto* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (!IsValid(Source) || !PC || !PC->IsLocalController()) { return false; }
	FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye, Rotation);
	if (FVector::DotProduct(Rotation.Vector(), Source->GetActorLocation() - Eye) <= 0.f) { return false; }
	FVector2D Screen; int32 Width = 0, Height = 0; PC->GetViewportSize(Width, Height);
	return Width > 0 && Height > 0 && PC->ProjectWorldLocationToScreen(Source->GetActorLocation(), Screen, true)
		&& Screen.X >= 0.f && Screen.X <= Width && Screen.Y >= 0.f && Screen.Y <= Height;
}

bool USovEncounterCoordinationComponent::CanAdmitAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
	const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) const
{
	FName Id;
	if (!IsBoundSource(Source, Id) || !IsValid(Ability) || !Handle.IsValid() || !GetWorld()
		|| !IsLivingTarget(Target, GetWorld()) || !IsHostileTo(Source->GetAvatarActor(), Target)) { return false; }
	const auto* Spec = Source->FindAbilitySpecFromHandle(Handle);
	if (!Spec || Spec->PendingRemove || Spec->IsActive() || (Spec->Ability != Ability && Spec->GetPrimaryInstance() != Ability)) { return false; }
	const auto Pressure = PressureOf(Ability);
	if (!IsBoundSource(Source, Id) || !IsValid(Target)) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAttackAt.FindRef(Id) || (IsPressureReliefActive() && Now < NextReliefAttackAt)) { return false; }
	int32 Melee = 0, Role = 0;
	const auto Entry = Member(Id);
	for (const auto& Pair : Reservations)
	{
		if (Pair.Value.ASC.Get() == Source) { return false; }
		if (Pair.Value.bMelee) { ++Melee; }
		if (Pair.Value.Role == Entry.Role) { ++Role; }
	}
	if (Pressure == ESovBotAttackPressure::Melee
		&& !SovEncounterCoordinationPolicy::HasSlot(Melee, SovEncounterCoordinationPolicy::Slots(MeleeAttackerSlots, IsPressureReliefActive()))) { return false; }
	if (const int32* Quota = SimultaneousRoleQuotas.Find(Entry.Role))
	{ if (!SovEncounterCoordinationPolicy::HasSlot(Role, *Quota)) { return false; } }
	if (Pressure == ESovBotAttackPressure::Ranged && Target == Director->GetEncounterPlayer())
	{
		const FWarning* Warning = Warnings.Find(Id);
		return SovEncounterCoordinationPolicy::WarningReady(IsSourceOnscreen(Source->GetAvatarActor(), Target), bRequireOffscreenRangedWarning,
			Warning && Warning->Source.Get() == Source->GetAvatarActor() && Warning->bAcknowledged
				&& Now <= Warning->CreatedAt + WarningLeadSeconds + 5., Now, Warning ? Warning->AcknowledgedAt : 0., WarningLeadSeconds);
	}
	return true;
}

FGuid USovEncounterCoordinationComponent::ReserveAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
	const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle)
{
	if (bReserving) { return {}; }
	TGuardValue<bool> Reserving(bReserving, true);
	if (!CanAdmitAttack(Source, Target, Ability, Handle)) { return {}; }
	const auto Pressure = PressureOf(Ability);
	FName Id;
	if (!IsBoundSource(Source, Id)) { return {}; }
	const FGameplayAbilitySpec* Spec = Source->FindAbilitySpecFromHandle(Handle);
	if (!Spec || Spec->PendingRemove || Spec->IsActive() || !IsLivingTarget(Target, GetWorld())
		|| !IsHostileTo(Source->GetAvatarActor(), Target)) { return {}; }
	FReservation Reservation; Reservation.ASC = Source; Reservation.Avatar = Source->GetAvatarActor(); Reservation.Target = Target;
	Reservation.Handle = Handle; Reservation.Role = Member(Id).Role; Reservation.bMelee = Pressure == ESovBotAttackPressure::Melee;
	const FGuid Ticket = FGuid::NewGuid(); Reservations.Add(Ticket, Reservation);
	const double Now = GetWorld()->GetTimeSeconds();
	if (Member(Id).Tier == ESovEncounterDecisionTier::Supporting) { NextAttackAt.Add(Id, Now + SupportingAttackInterval); }
	if (IsPressureReliefActive()) { NextReliefAttackAt = Now + ReliefAttackInterval; }
	if (Pressure == ESovBotAttackPressure::Ranged && Target == Director->GetEncounterPlayer()
		&& !IsSourceOnscreen(Source->GetAvatarActor(), Target)) { Warnings.Remove(Id); }
	return Ticket;
}

void USovEncounterCoordinationComponent::ReleaseAttack(FGuid ReservationId)
{
	Reservations.Remove(ReservationId);
}

bool USovEncounterCoordinationComponent::IsAttackReservationCurrent(FGuid ReservationId, const UNarrativeAbilitySystemComponent* Source,
	const AActor* Target, FGameplayAbilitySpecHandle Handle) const
{
	const auto* Reservation = Reservations.Find(ReservationId);
	FName Id;
	return Reservation && IsBoundSource(const_cast<UNarrativeAbilitySystemComponent*>(Source), Id)
		&& Reservation->ASC.Get() == Source && Reservation->Avatar.Get() == Source->GetAvatarActor()
		&& Reservation->Target.Get() == Target && IsLivingTarget(Target, GetWorld()) && Reservation->Handle == Handle;
}

bool USovEncounterCoordinationComponent::AcknowledgeOffscreenWarning(FGuid WarningId)
{
	if (!Director.IsValid() || !Director->HasAuthority() || Director->GetEncounterState() != ESovEncounterState::Active
		|| Director->GetAttemptId() != BoundAttempt || !GetWorld() || !WarningId.IsValid()) { return false; }
	for (auto& Pair : Warnings)
	{
		if (Pair.Value.Id != WarningId) { continue; }
		const double Now = GetWorld()->GetTimeSeconds();
		if (Pair.Value.bAcknowledged || Pair.Value.Source.Get() != Director->GetParticipant(Pair.Key)
			|| !Pair.Value.Source.IsValid() || !IsHostileTo(Pair.Value.Source.Get(), Director->GetEncounterPlayer())
			|| Now > Pair.Value.CreatedAt + 5.) { return false; }
		Pair.Value.bAcknowledged = true; Pair.Value.AcknowledgedAt = Now; return true;
	}
	return false;
}

void USovEncounterCoordinationComponent::UpdateWarnings()
{
	if (!Director.IsValid() || !GetWorld() || !bRequireOffscreenRangedWarning) { Warnings.Reset(); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = Warnings.CreateIterator(); It; ++It)
	{
		const auto* Character = Cast<ASovNPCCharacterBase>(It.Value().Source.Get());
		if (!IsValid(Character) || !Character->IsAlive() || Director->GetParticipant(It.Key()) != Character
			|| IsSourceOnscreen(Character, Director->GetEncounterPlayer()) || Now > It.Value().CreatedAt + WarningLeadSeconds + 5.) { It.RemoveCurrent(); }
	}
	const auto ASCs = BoundASCs;
	for (auto ASC : ASCs)
	{
		FName Id;
		if (!IsBoundSource(ASC.Get(), Id) || Warnings.Contains(Id) || IsSourceOnscreen(ASC->GetAvatarActor(), Director->GetEncounterPlayer())
			|| !IsHostileTo(ASC->GetAvatarActor(), Director->GetEncounterPlayer())) { continue; }
		bool bRanged = false;
		const auto Specs = ASC->GetActivatableAbilities();
		for (const auto& Spec : Specs)
		{
			const auto* Ability = Cast<UNarrativeCombatAbility>(Spec.GetPrimaryInstance() ? Spec.GetPrimaryInstance() : Spec.Ability.Get());
			if (!Spec.PendingRemove && Ability && Ability->bBotSelectionEnabled && PressureOf(Ability) == ESovBotAttackPressure::Ranged) { bRanged = true; break; }
		}
		if (!bRanged || !IsBoundSource(ASC.Get(), Id)) { continue; }
		FWarning Warning; Warning.Id = FGuid::NewGuid(); Warning.Source = ASC->GetAvatarActor(); Warning.CreatedAt = Now;
		Warnings.Add(Id, Warning);
		OnOffscreenAttackWarning.Broadcast(Warning.Id, Warning.Source.Get(), WarningLeadSeconds);
		if (!Director.IsValid() || Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()) { return; }
	}
}

void USovEncounterCoordinationComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta, TickType, TickFunction);
	if (!Director.IsValid() || !Director->HasAuthority() || !bValidComposition || bRefreshing
		|| Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()) { return; }
	// Visual actors and async weapon attachments have independent Actor flags.
	// Body publication is handled immediately; this existing 0.1s tick also observes later attachments.
	TArray<FName> StagedIds; Staged.GetKeys(StagedIds);
	for (FName Id : StagedIds) { RefreshStagedPresentation(Id); }
	if (!Director.IsValid() || Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()) { return; }
	for (auto It = Reservations.CreateIterator(); It; ++It)
	{
		const auto& Lease = It.Value();
		const auto* Spec = Lease.ASC.IsValid() ? Lease.ASC->FindAbilitySpecFromHandle(Lease.Handle) : nullptr;
		if (!Spec || !Spec->IsActive() || Lease.ASC->GetAvatarActor() != Lease.Avatar.Get() || !Lease.Target.IsValid()) { It.RemoveCurrent(); }
	}
	bool bRequiredAlive = false, bHasNext = false;
	int32 NextCombatants = 0, NextSupporting = 0;
	for (const auto& Participant : Director->Participants)
	{
		if (Director->IsParticipantMassRepresented(Participant.ParticipantId))
		{
			// Representation loss is never a confirmed defeat. Required C/D ownership holds the wave gate.
			if (Member(Participant.ParticipantId).Wave == CurrentWave && Participant.bRequiredForVictory) { bRequiredAlive = true; }
			continue;
		}
		if (!IsValid(Participant.Character) || !Participant.Character->IsAlive()) { continue; }
		const auto Entry = Member(Participant.ParticipantId);
		if (Entry.Wave == CurrentWave && Participant.bRequiredForVictory) { bRequiredAlive = true; }
		if (Entry.Wave == CurrentWave + 1) { bHasNext = true; }
		if (Entry.Wave <= CurrentWave + 1)
		{ if (Entry.Tier == ESovEncounterDecisionTier::Combatant) { ++NextCombatants; } else { ++NextSupporting; } }
	}
	const bool bEventGate = BoundWaveRules.Contains(CurrentWave + 1);
	const bool bRelease = bEventGate ? CanReleaseEventWave(CurrentWave + 1)
		: !bRequiredAlive && bHasNext && NextCombatants <= MaximumCombatants && NextSupporting <= MaximumSupporting;
	if (bRelease)
	{
		const FGuid Attempt = BoundAttempt; const uint64 Generation = BoundGeneration; const int32 ReleasedWave = ++CurrentWave;
		RefreshComposition();
		if (!IsWaveContextCurrent(Attempt, Generation, ReleasedWave)) { return; }
		OnWaveChanged.Broadcast(CurrentWave);
		if (!IsWaveContextCurrent(Attempt, Generation, ReleasedWave)) { return; }
	}
	auto* Player = Director->GetEncounterPlayer();
	const auto* ASC = Player ? Player->GetAbilitySystemComponent() : nullptr;
	const double Now = GetWorld()->GetTimeSeconds();
	if (bAllowLowResourceRelief && ASC && Player->IsAlive() && Now >= NextReliefAt
		&& SovEncounterCoordinationPolicy::LowResources(
			ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()),
			ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxShieldAttribute()),
			ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetStaminaAttribute()), ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxStaminaAttribute())))
	{ ReliefUntil = Now + ReliefDuration; NextReliefAt = Now + ReliefCooldown; }
	const bool bRelief = IsPressureReliefActive();
	if (bRelief != bReliefReported)
	{
		bReliefReported = bRelief;
		OnPressureChanged.Broadcast(bRelief, bRelief ? 0.25f : 1.f);
		if (!Director.IsValid() || Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()) { return; }
	}
	UpdateWarnings();
}

USovCommandLinkComponent* USovEncounterCoordinationComponent::ResolveRuleLink(const FSovEncounterWaveReleaseRule& Rule) const
{
	const auto* OwnerDirector = Cast<ASovEncounterDirector>(GetOwner());
	const auto* NPC = OwnerDirector ? OwnerDirector->GetParticipant(Rule.CommandLinkParticipantId) : nullptr;
	if (!IsValid(NPC) || Rule.CommandLinkComponentName.IsNone()) { return nullptr; }
	TArray<USovCommandLinkComponent*> Links; NPC->GetComponents(Links);
	for (auto* Link : Links)
	{
		if (IsValid(Link) && Link->GetFName() == Rule.CommandLinkComponentName && Link->GetOwner() == NPC && Link->IsRegistered()) { return Link; }
	}
	return nullptr;
}

bool USovEncounterCoordinationComponent::IsWaveContextCurrent(FGuid Attempt, uint64 Generation, int32 Wave) const
{
	return !IsBeingDestroyed() && Director.IsValid() && !Director->IsActorBeingDestroyed() && Director->HasAuthority()
		&& bValidComposition && BoundAttempt == Attempt && Attempt.IsValid() && Director->GetAttemptId() == Attempt
		&& BoundGeneration == Generation && Director->GetLifecycleGeneration() == Generation
		&& Director->GetEncounterState() == ESovEncounterState::Active && CurrentWave == Wave;
}

bool USovEncounterCoordinationComponent::BindWaveRules()
{
	WaveActors.Reset(); BoundWaveRules.Reset();
	if (WaveReleaseRules.IsEmpty()) { return true; }
	if (!Director.IsValid()) { return false; }
	for (const auto& Participant : Director->Participants)
	{
		auto* NPC = Participant.Character.Get();
		auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
		// An event-gated encounter requires exact authored actors, not an unknown Mass proxy.
		if (!IsValid(NPC) || !IsValid(ASC) || ASC->GetAvatarActor() != NPC || !NPC->IsAlive()
			|| Director->IsParticipantMassRepresented(Participant.ParticipantId)) { return false; }
		FWaveActor Captured; Captured.Character = NPC; Captured.ASC = ASC;
		Captured.bRequired = Participant.bRequiredForVictory; Captured.Wave = Member(Participant.ParticipantId).Wave;
		WaveActors.Add(Participant.ParticipantId, Captured);
	}
	for (const auto& Rule : WaveReleaseRules)
	{
		FWaveRule Binding; Binding.Rule = Rule;
		if (Rule.Condition == ESovEncounterWaveCondition::CommandSourceDefeated)
		{
			auto* Link = ResolveRuleLink(Rule);
			if (!Link || !Link->IsCommandLinkActive() || !Link->HasValidCommandLinkConfiguration() || !Link->GetLinkInstanceId().IsValid()) { return false; }
			Binding.Link = Link; Binding.SourceId = Director->FindParticipantId(Link->GetCommandSource()); Binding.LinkInstance = Link->GetLinkInstanceId();
			if (!WaveActors.Contains(Binding.SourceId)) { return false; }
		}
		BoundWaveRules.Add(Rule.Wave, Binding);
	}
	return true;
}

bool USovEncounterCoordinationComponent::CanReleaseEventWave(int32 Wave) const
{
	const auto* Binding = BoundWaveRules.Find(Wave);
	if (!Binding || Wave != CurrentWave + 1 || !IsWaveContextCurrent(BoundAttempt, BoundGeneration, CurrentWave)
		|| Director->Participants.Num() != WaveActors.Num()) { return false; }
	int32 LivingReleasedHostiles = 0, Combatants = 0, Supporting = 0;
	bool bHasNext = false;
	for (const auto& Participant : Director->Participants)
	{
		const auto* Captured = WaveActors.Find(Participant.ParticipantId);
		const auto Entry = Member(Participant.ParticipantId);
		if (!Captured || Captured->Character != TWeakObjectPtr<ASovNPCCharacterBase>(Participant.Character.Get())
			|| Captured->bRequired != Participant.bRequiredForVictory || Captured->Wave != Entry.Wave) { return false; }
		if (Entry.Wave > Wave) { continue; }
		if (Director->HasConfirmedParticipantDefeat(Participant.ParticipantId)) { continue; }
		const auto* NPC = Captured->Character.Get(); const auto* ASC = Captured->ASC.Get();
		if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || !IsValid(ASC) || NPC->GetNarrativeAbilitySystemComponent() != ASC
			|| ASC->GetAvatarActor() != NPC || !NPC->IsAlive() || ASC->IsDead()
			|| !(ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f)
			|| Director->IsParticipantMassRepresented(Participant.ParticipantId)) { return false; }
		if (Entry.Wave <= CurrentWave && Participant.bRequiredForVictory) { ++LivingReleasedHostiles; }
		if (Entry.Wave == Wave) { bHasNext = true; }
		if (Entry.Tier == ESovEncounterDecisionTier::Combatant) { ++Combatants; } else { ++Supporting; }
	}
	if (!bHasNext || LivingReleasedHostiles > Binding->Rule.MaximumLivingReleasedHostiles
		|| Combatants > MaximumCombatants || Supporting > MaximumSupporting) { return false; }
	switch (Binding->Rule.Condition)
	{
	case ESovEncounterWaveCondition::CommandSourceDefeated:
		// The source was registered and the exact formation active at this attempt's entry.
		// Only the director's authoritative ASC death receipt can retire it. Destroy/end-play cannot.
		return Binding->LinkInstance.IsValid() && Director->HasConfirmedParticipantDefeat(Binding->SourceId);
	case ESovEncounterWaveCondition::TransitDoorOpen:
		return IsValid(Binding->Rule.TransitDoor) && Binding->Rule.TransitDoor->GetWorld() == GetWorld()
			&& Binding->Rule.TransitDoor->IsOpenTraversableDoor();
	case ESovEncounterWaveCondition::AcceptedCrucibleLink:
		if (const auto* Phase = Cast<ASovAurelionLinkPhaseDirector>(Director.Get())) { return Phase->HasAcceptedCurrentLinkReceipt(); }
		return false;
	default: return false;
	}
}

bool USovEncounterCoordinationComponent::HasUnreleasedWaves() const
{
	return !Staged.IsEmpty() || Composition.ContainsByPredicate([this](const auto& Entry) { return Entry.Wave > CurrentWave; });
}

bool USovEncounterCoordinationComponent::ReleaseCompletedPhaseBindings()
{
	if (!Director.IsValid() || !Director->HasAuthority() || Director->IsActorBeingDestroyed() || bRefreshing || bReserving
		|| Director->GetEncounterState() != ESovEncounterState::Succeeded || Director->IsCampaignReceiptPending()
		|| HasUnreleasedWaves()) { return false; }
	// No staged actor can be released here. Reset only this owner's attack binding and decision interval;
	// the director's Busy, protection, brain pause and threat lease remain in place throughout transfer.
	ResetAttempt();
	return true;
}

bool USovEncounterCoordinationComponent::RestoreCompletedWaveState(int32 ReleasedWave)
{
	const auto* OwnerDirector = Cast<ASovEncounterDirector>(GetOwner());
	if (!OwnerDirector || !OwnerDirector->HasAuthority() || OwnerDirector->GetEncounterState() != ESovEncounterState::Succeeded
		|| OwnerDirector->IsActorBeingDestroyed() || !OwnerDirector->HasConfirmedVictory() || !Staged.IsEmpty() || bRefreshing || bReserving) { return false; }
	int32 LastWave = 0;
	for (const auto& Entry : Composition) { LastWave = FMath::Max(LastWave, Entry.Wave); }
	if (ReleasedWave != LastWave || ReleasedWave < 0 || ReleasedWave > 63) { return false; }
	CurrentWave = ReleasedWave;
	return true;
}
