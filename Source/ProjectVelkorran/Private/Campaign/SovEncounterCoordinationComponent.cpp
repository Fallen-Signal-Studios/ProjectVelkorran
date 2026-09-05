// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Campaign/SovEncounterCoordinationPolicy.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovNPCCharacterBase.h"
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
		if (Entry.ParticipantId.IsNone() || Ids.Contains(Entry.ParticipantId) || !IsValid(Entry.Character) || Actors.Contains(Entry.Character)
			|| Entry.Character->GetWorld() != GetWorld())
		{ Error = TEXT("Composition requires unique registered participant identities and actors in this world."); return false; }
		const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Entry.Character->GetAbilitySystemComponent());
		if (ASC && ASC->GetBotAttackCoordinator() && ASC->GetBotAttackCoordinator() != this)
		{ Error = TEXT("An NPC cannot belong to two encounter coordinators."); return false; }
		Ids.Add(Entry.ParticipantId); Actors.Add(Entry.Character);
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
			if (Entry.Tier == ESovEncounterDecisionTier::Combatant) { ++Combatants; } else { ++Supporting; }
			if (Participant.bRequiredForVictory) { ++Required; }
		}
		if (Combatants > MaximumCombatants || Supporting > MaximumSupporting || (Waves.Num() > 1 && Required == 0))
		{ Error = TEXT("Each wave must fit the A/B budget; multiwave encounters require a defeat gate in every wave."); return false; }
	}
	return true;
}

void USovEncounterCoordinationComponent::ResetAttempt()
{
	BoundAttempt.Invalidate(); Reservations.Reset(); Warnings.Reset(); NextAttackAt.Reset(); RuntimeTiers.Reset();
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
	BoundAttempt = Director->GetAttemptId();
	RefreshComposition();
	if (Director.IsValid() && Director->GetEncounterState() == ESovEncounterState::Active && BoundAttempt == Director->GetAttemptId())
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
	Saved.bOwnsBusy = true;
	Staged.Add(Id, Saved); // Commit ownership before tag callbacks.
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
	FStaged* Live = Staged.Find(Id);
	if (!Live || Live->Character.Get() != Character || !IsValid(Character) || !IsValid(ASC)) { return; }
	Live->bOwnsInvulnerability = true;
	ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll);
	if (Staged.Contains(Id) && IsValid(Character)) { Character->SetActorHiddenInGame(true); Character->SetActorEnableCollision(false); }
}

void USovEncounterCoordinationComponent::ReleaseStagedParticipant(FName Id)
{
	FStaged Saved;
	if (!Staged.RemoveAndCopyValue(Id, Saved)) { return; }
	if (Saved.Character.IsValid())
	{
		Saved.Character->SetActorHiddenInGame(Saved.bHidden); Saved.Character->SetActorEnableCollision(Saved.bCollision);
		if (auto* Movement = Saved.Character->GetCharacterMovement())
		{
			if (Movement->MovementMode == MOVE_None) { Movement->SetMovementMode(static_cast<EMovementMode>(Saved.MovementMode), Saved.CustomMovementMode); }
		}
	}
	for (auto Component : Saved.DisabledTicks) { if (Component.IsValid()) { Component->SetComponentTickEnabled(true); } }
	if (Saved.ASC.IsValid())
	{
		if (Saved.bOwnsBusy) { Saved.ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll); }
		if (Saved.ASC.IsValid() && Saved.bOwnsInvulnerability) { Saved.ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll); }
	}
}

void USovEncounterCoordinationComponent::RefreshComposition()
{
	if (bRefreshing || !Director.IsValid() || !bValidComposition || Director->GetEncounterState() != ESovEncounterState::Active) { return; }
	TGuardValue<bool> Refreshing(bRefreshing, true);
	const auto Participants = Director->Participants;
	for (const auto& Participant : Participants)
	{
		auto* Character = Participant.Character.Get();
		if (!IsValid(Character) || !Character->IsAlive()) { continue; }
		auto* ASC = Cast<UNarrativeAbilitySystemComponent>(Character->GetAbilitySystemComponent());
		if (!ASC || ASC->GetAvatarActor() != Character || !ASC->SetBotAttackCoordinator(this)) { bValidComposition = false; return; }
		BoundASCs.AddUnique(ASC);
		const auto Entry = Member(Participant.ParticipantId);
		if (Entry.Wave > CurrentWave) { StageParticipant(Participant.ParticipantId, Character); continue; }
		ReleaseStagedParticipant(Participant.ParticipantId);
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
		if (!IsValid(Participant.Character) || !Participant.Character->IsAlive()) { continue; }
		const auto Entry = Member(Participant.ParticipantId);
		if (Entry.Wave == CurrentWave && Participant.bRequiredForVictory) { bRequiredAlive = true; }
		if (Entry.Wave == CurrentWave + 1) { bHasNext = true; }
		if (Entry.Wave <= CurrentWave + 1)
		{ if (Entry.Tier == ESovEncounterDecisionTier::Combatant) { ++NextCombatants; } else { ++NextSupporting; } }
	}
	if (!bRequiredAlive && bHasNext && NextCombatants <= MaximumCombatants && NextSupporting <= MaximumSupporting)
	{
		++CurrentWave; RefreshComposition(); OnWaveChanged.Broadcast(CurrentWave);
		if (!Director.IsValid() || Director->GetEncounterState() != ESovEncounterState::Active || BoundAttempt != Director->GetAttemptId()) { return; }
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
