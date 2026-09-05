// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Resonance/SovResonanceComponent.h"
#include "Resonance/SovResonanceAbility.h"
#include "Resonance/SovResonanceTargetComponent.h"
#include "Resonance/SovResonancePolicy.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Companions/SovCompanionComponent.h"
#include "Components/SovCommandLinkComponent.h"
#include "Components/SovGuardComponent.h"
#include "Components/SovWeakPointComponent.h"
#include "Combat/SovProtectionInterceptReceipt.h"
#include "Combat/SovSelenePayload.h"
#include "Effects/SovGameplayEffect_CinderWard.h"
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NarrativeGameplayTags.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

namespace
{
UNarrativeAbilitySystemComponent* ASC(AActor* Actor)
{ return IsValid(Actor) ? Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) : nullptr; }
bool Alive(AActor* Actor)
{
	auto* Abilities = ASC(Actor);
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Abilities && Abilities->GetAvatarActor() == Actor
		&& Abilities->GetSet<UNarrativeAttributeSetBase>() && Abilities->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f
		&& !Abilities->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
		&& !Abilities->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal);
}
USovCampaignStateComponent* Campaign(AActor* Owner)
{
	auto* Pawn = Cast<APawn>(Owner);
	return Pawn && Pawn->GetController() ? Pawn->GetController()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
}
bool Hostile(AActor* Source, AActor* Target)
{
	auto* Team = Cast<INarrativeTeamAgentInterface>(Source);
	return Team && IsValid(Target) && Team->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
}
bool Visible(AActor* Source, AActor* Target)
{
	if (!IsValid(Source) || !IsValid(Target)) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(ResonanceLOS), false);
	SovSelenePayload::IgnoreSource(Query, Source);
	FHitResult Hit;
	return !Source->GetWorld()->LineTraceSingleByChannel(Hit, Source->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query)
		|| SovSelenePayload::ResolveTarget(Hit.GetActor()) == Target;
}
}
USovResonanceComponent::USovResonanceComponent()
{
	SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}
void USovResonanceComponent::BeginPlay() { Super::BeginPlay(); BindASCs(); }
void USovResonanceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CancelInteraction();
	if (IsValid(PlayerASC)) { PlayerASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::ObserveDamage); }
	if (IsValid(PartnerASC)) { PartnerASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::ObserveDamage); }
	Super::EndPlay(Reason);
}
void USovResonanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(USovResonanceComponent, Interaction); }
void USovResonanceComponent::OnRep_Interaction() { OnInteractionChanged.Broadcast(Interaction); }
USovResonanceComponent* USovResonanceComponent::FindActive(UWorld* World)
{
	if (!World || World->GetNetMode() != NM_Standalone) { return nullptr; }
	for (auto It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn()) { return PC->GetPawn()->FindComponentByClass<USovResonanceComponent>(); }
	}
	return nullptr;
}
void USovResonanceComponent::BindASCs()
{
	auto* NewPlayer = ASC(GetOwner());
	auto* NewPartner = IsValid(Partner) ? ASC(Partner->GetOwner()) : nullptr;
	if (NewPlayer == PlayerASC && NewPartner == PartnerASC) { return; }
	if (Interaction.State == ESovResonanceState::Offered || Interaction.State == ESovResonanceState::Committed)
	{ Finish(false, TEXT("A protagonist's ability system changed.")); }
	if (IsValid(PlayerASC)) { PlayerASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::ObserveDamage); }
	if (IsValid(PartnerASC)) { PartnerASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::ObserveDamage); }
	PlayerASC = NewPlayer; PartnerASC = NewPartner;
	if (PlayerASC) { PlayerASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::ObserveDamage); }
	if (PartnerASC) { PartnerASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::ObserveDamage); }
}
AActor* USovResonanceComponent::GetTarrik() const
{
	const FGameplayTag Tag = FSovGameplayTags::Get().Character_Player_Tarrik;
	if (ASC(GetOwner()) && ASC(GetOwner())->HasMatchingGameplayTag(Tag)) { return GetOwner(); }
	return IsValid(Partner) && ASC(Partner->GetOwner()) && ASC(Partner->GetOwner())->HasMatchingGameplayTag(Tag) ? Partner->GetOwner() : nullptr;
}
AActor* USovResonanceComponent::GetSelene() const
{
	const FGameplayTag Tag = FSovGameplayTags::Get().Character_Player_Selene;
	if (ASC(GetOwner()) && ASC(GetOwner())->HasMatchingGameplayTag(Tag)) { return GetOwner(); }
	return IsValid(Partner) && ASC(Partner->GetOwner()) && ASC(Partner->GetOwner())->HasMatchingGameplayTag(Tag) ? Partner->GetOwner() : nullptr;
}
bool USovResonanceComponent::RegisterPartner(USovCompanionComponent* Companion, FString& Reason)
{
	Reason.Reset(); auto* State = Campaign(GetOwner()); auto* Mission = State ? State->GetActiveMission() : nullptr;
	if (bMutation || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Companion) || Companion->GetOwner() == GetOwner()
		|| !Mission || !Mission->bAllowJointResonance || !Mission->AllowedCompanionIds.Contains(Companion->CompanionId)
		|| Companion->GetWorld() != GetWorld() || Interaction.State == ESovResonanceState::Committed)
	{ Reason = TEXT("This mission does not permit that complementary protagonist companion."); return false; }
	CancelInteraction(); Partner = Companion; BindASCs();
	if (!ValidatePair(false, Reason)) { Partner = nullptr; BindASCs(); return false; }
	return true;
}
bool USovResonanceComponent::ValidatePair(bool bDuringCommit, FString& Reason) const
{
	Reason.Reset(); const auto& Tags = FSovGameplayTags::Get();
	auto* Pawn = Cast<ASovPlayerCharacterBase>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || !Pawn->IsPlayerControlled() || !Pawn->IsCharacterReady() || !Partner
		|| !Alive(Pawn) || !Alive(Partner->GetOwner()) || !PlayerASC || !PartnerASC || PlayerASC == PartnerASC
		|| ASC(Pawn) != PlayerASC || ASC(Partner->GetOwner()) != PartnerASC
		|| !SovResonancePolicy::Complementary(PlayerASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik),
			PlayerASC->HasMatchingGameplayTag(Tags.Character_Player_Selene), PartnerASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik),
			PartnerASC->HasMatchingGameplayTag(Tags.Character_Player_Selene)))
	{ Reason = TEXT("Resonance requires two living complementary protagonists with separate current ability systems."); return false; }
	if (FVector::DistSquared(Pawn->GetActorLocation(), Partner->GetOwner()->GetActorLocation()) > FMath::Square(2500.f))
	{ Reason = TEXT("The other protagonist is separated from the action."); return false; }
	for (auto* Abilities : { PlayerASC.Get(), PartnerASC.Get() })
	{
		const auto& N = FNarrativeGameplayTags::Get();
		for (FGameplayTag Blocker : { N.State_SequencerControlled, N.State_Movement_Ragdoll, Tags.State_Poise_Broken, Tags.State_Status_Frozen })
		{ if (Abilities->HasMatchingGameplayTag(Blocker)) { Reason = TEXT("A protagonist is interrupted or scripted."); return false; } }
		// Existing held guard/deflection is the valid setup action, and ends on our owned Busy commit.
		const bool bDefense = Abilities->HasMatchingGameplayTag(Tags.State_Guarding) || Abilities->HasMatchingGameplayTag(Tags.State_Deflecting);
		if (!bDuringCommit && Abilities->HasMatchingGameplayTag(N.State_Busy) && !bDefense)
		{ Reason = TEXT("A protagonist is committed to another action."); return false; }
	}
	return true;
}
bool USovResonanceComponent::IsPermitted(ESovResonanceType Type, const AActor* Target) const
{
	auto* State = Campaign(GetOwner()); auto* Mission = State ? State->GetActiveMission() : nullptr;
	if (!State || !State->IsStateValid() || !Mission || !Mission->bAllowJointResonance || !Mission->AllowedResonanceTypes.Contains(Type)
		|| (!Mission->MissionId.ToString().StartsWith(TEXT("M12_")) && !Mission->MissionId.ToString().StartsWith(TEXT("M13_")))
		|| !Partner || !Mission->AllowedCompanionIds.Contains(Partner->CompanionId)) { return false; }
	for (FName Beat : Mission->ResonancePrerequisiteBeats)
	{ if (!State->IsBeatComplete(Mission->MissionId, Beat)) { return false; } }
	if (const auto* Context = Target ? Target->FindComponentByClass<USovResonanceTargetComponent>() : nullptr)
	{ if (!Context->RequiredBeat.IsNone() && !State->IsBeatComplete(Mission->MissionId, Context->RequiredBeat)) { return false; } }
	return true;
}
bool USovResonanceComponent::MakeOffer(ESovResonanceType Type, USovResonanceTargetComponent* Target, float Pressure)
{
	FString Reason;
	if (bMutation || !IsValid(Target) || !Target->CanResolve(Type, this) || !ValidatePair(false, Reason)
		|| Interaction.State == ESovResonanceState::Committed || GetWorld()->GetTimeSeconds() < NextAllowedTime) { return false; }
	TGuardValue<bool> Mutation(bMutation, true);
	if (Interaction.State == ESovResonanceState::Offered) { Finish(false, TEXT("A newer complementary opening replaced the offer.")); }
	ContextTarget = Target; InteractionMission = Campaign(GetOwner())->GetActiveMission();
	Interaction = FSovResonanceInteraction(); Interaction.InteractionId = FGuid::NewGuid(); Interaction.Type = Type;
	Interaction.State = ESovResonanceState::Offered; Interaction.Target = Target->GetOwner(); Interaction.Partner = Partner->GetOwner();
	Interaction.Pressure = FMath::Max(0.f, Pressure);
	Interaction.ExpiresAt = GetWorld()->GetTimeSeconds() + (Type == ESovResonanceType::SupportSever ? .65f : 2.f);
	bOwnsAvailableTag = true;
	const FGuid OfferId = Interaction.InteractionId;
	PlayerASC->AddLooseGameplayTag(FSovGameplayTags::Get().State_Resonance_Available, 1, EGameplayTagReplicationState::TagAndCountToAll);
	if (Interaction.InteractionId != OfferId || Interaction.State != ESovResonanceState::Offered) { return false; }
	GetOwner()->ForceNetUpdate(); OnRep_Interaction(); return Interaction.State == ESovResonanceState::Offered;
}
void USovResonanceComponent::ObserveExposure(USovResonanceTargetComponent* Target, AActor* Instigator)
{
	if (Instigator == GetSelene()) { MakeOffer(ESovResonanceType::FormationBreach, Target); }
}
void USovResonanceComponent::ObserveDamage(const FSovDamageResult& Result)
{
	if (bMutation || !GetOwner()->HasAuthority() || !Result.TransactionId.IsValid() || Result.bPeriodicDamage
		|| SeenTransactions.Contains(Result.TransactionId)) { return; }
	if (SeenTransactions.Num() >= 256) { SeenTransactions.Reset(); }
	SeenTransactions.Add(Result.TransactionId);
	if (Result.TargetActor == GetSelene() && Result.bDeflected && Result.bPerfectDefense && Alive(Result.SourceActor))
	{ RoutedThreat = Result.SourceActor; RoutedUntil = GetWorld()->GetTimeSeconds() + 2.f; }
	if (Result.TargetActor != GetTarrik() || !Result.bGuarded || !Alive(Result.SourceActor)) { return; }
	auto* Target = Result.SourceActor->FindComponentByClass<USovResonanceTargetComponent>();
	if (!Target) { return; }
	const auto& Tags = FSovGameplayTags::Get();
	if (Result.bPerfectDefense && (Result.AttackClassifications.HasTag(Tags.Damage_Heavy)
		|| Result.AttackClassifications.HasTag(Tags.Damage_GuardClass_Heavy))
		&& ASC(Result.SourceActor)->HasMatchingGameplayTag(Tags.State_Target_Marked))
	{ MakeOffer(ESovResonanceType::SupportSever, Target); }
	else if (RoutedThreat == Result.SourceActor && GetWorld()->GetTimeSeconds() < RoutedUntil
		&& ASC(GetTarrik())->GetNumericAttribute(UNarrativeAttributeSetBase::GetShieldAttribute()) > 0.f)
	{ MakeOffer(ESovResonanceType::AdvanceCorridor, Target); RoutedThreat.Reset(); }
}
void USovResonanceComponent::NotifyProtectionIntercept(USovProtectionInterceptReceipt* Receipt, const FSovDamageResult& Result)
{
	AActor* Protected = nullptr;
	if (!Receipt || !Receipt->MatchesCommittedForProtector(GetTarrik(), Result, Protected) || Protected != GetSelene()
		|| Result.DefenseKind != ESovDefenseKind::Guard || !Result.bGuarded || !IsPermitted(ESovResonanceType::TerminalRelease)) { return; }
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{ if (auto* Target = It->FindComponentByClass<USovResonanceTargetComponent>()) { Target->RecordProtection(Protected, Result.BaseDamage); } }
}
bool USovResonanceComponent::IsContextValid() const
{
	FString Reason;
	return IsValid(ContextTarget) && IsValid(InteractionMission) && Campaign(GetOwner())
		&& Campaign(GetOwner())->GetActiveMission() == InteractionMission && ContextTarget->CanResolve(Interaction.Type, this)
		&& ValidatePair(Interaction.State == ESovResonanceState::Committed, Reason)
		&& (Interaction.Type == ESovResonanceType::TerminalRelease || Hostile(GetOwner(), ContextTarget->GetOwner()))
		&& FVector::DistSquared(GetOwner()->GetActorLocation(), ContextTarget->GetOwner()->GetActorLocation()) <= FMath::Square(2500.f)
		&& Visible(GetOwner(), ContextTarget->GetOwner()) && Visible(Partner->GetOwner(), ContextTarget->GetOwner());
}
bool USovResonanceComponent::ConfirmOffer(FGuid Id, FString& Reason)
{
	Reason.Reset();
	if (bMutation || Interaction.State != ESovResonanceState::Offered || Id != Interaction.InteractionId
		|| !SovResonancePolicy::OfferLive(GetWorld()->GetTimeSeconds(), Interaction.ExpiresAt,
			Campaign(GetOwner()) && Campaign(GetOwner())->GetActiveMission() == InteractionMission,
			Alive(GetOwner()) && Partner && Alive(Partner->GetOwner()), IsContextValid(), PlayerASC != PartnerASC))
	{ Reason = TEXT("The exact complementary offer is no longer available."); return false; }
	TGuardValue<bool> Mutation(bMutation, true);
	Interaction.State = ESovResonanceState::Committed; CommittedAt = GetWorld()->GetTimeSeconds(); bParticipantEnded = false; bPayoffStarted = false;
	if (bOwnsAvailableTag) { bOwnsAvailableTag = false; PlayerASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Resonance_Available, 1, EGameplayTagReplicationState::TagAndCountToAll); }
	for (auto* Abilities : { PlayerASC.Get(), PartnerASC.Get() })
	{
		auto* Ticket = NewObject<USovResonanceTicket>(this); Ticket->Coordinator = this; Ticket->ExpectedASC = Abilities; Ticket->InteractionId = Id;
		Tickets.Add(Ticket);
		const auto Handle = Abilities->GiveAbility(FGameplayAbilitySpec(USovResonanceAbility::StaticClass(), 1, INDEX_NONE, Ticket));
		if (Abilities == PlayerASC) { PlayerHandle = Handle; } else { PartnerHandle = Handle; }
		if (!IsTicketCurrent(Ticket) || !Abilities->TryActivateAbility(Handle) || bParticipantEnded || !IsTicketCurrent(Ticket))
		{ Reason = TEXT("One protagonist could not commit the paired action."); Finish(false, Reason); return false; }
	}
	GetOwner()->ForceNetUpdate(); OnRep_Interaction(); return Interaction.State == ESovResonanceState::Committed;
}
bool USovResonanceComponent::IsTicketCurrent(const USovResonanceTicket* Ticket) const
{
	return IsValid(Ticket) && Interaction.State == ESovResonanceState::Committed && Ticket->InteractionId == Interaction.InteractionId
		&& Tickets.Contains(Ticket) && (Ticket->ExpectedASC == PlayerASC || Ticket->ExpectedASC == PartnerASC)
		&& IsValid(Ticket->ExpectedASC.Get()) && Ticket->ExpectedASC->GetAvatarActor()
		&& ASC(Ticket->ExpectedASC->GetAvatarActor()) == Ticket->ExpectedASC.Get();
}
void USovResonanceComponent::NotifyParticipationEnded(const USovResonanceTicket* Ticket)
{ if (IsTicketCurrent(Ticket)) { bParticipantEnded = true; } }
void USovResonanceComponent::ReleaseOwnedParticipation()
{
	Tickets.Reset(); // Invalidates both capabilities before synchronous ability-end callbacks.
	const auto OldPlayer = PlayerHandle; const auto OldPartner = PartnerHandle; PlayerHandle = {}; PartnerHandle = {};
	if (IsValid(PlayerASC) && OldPlayer.IsValid()) { PlayerASC->CancelAbilityHandle(OldPlayer); PlayerASC->ClearAbility(OldPlayer); }
	if (IsValid(PartnerASC) && OldPartner.IsValid()) { PartnerASC->CancelAbilityHandle(OldPartner); PartnerASC->ClearAbility(OldPartner); }
	if (BreachCapsule.IsValid())
	{ for (auto Actor : OwnedMovementIgnores) { if (Actor.IsValid()) { BreachCapsule->IgnoreActorWhenMoving(Actor.Get(), false); } } }
	OwnedMovementIgnores.Reset(); BreachCapsule.Reset();
}
void USovResonanceComponent::Finish(bool bSucceeded, const FString& Reason)
{
	TGuardValue<bool> Mutation(bMutation, true);
	Interaction.State = bSucceeded ? ESovResonanceState::Succeeded : ESovResonanceState::Canceled;
	Interaction.Reason = Reason;
	if (bOwnsAvailableTag && IsValid(PlayerASC))
	{ bOwnsAvailableTag = false; PlayerASC->RemoveLooseGameplayTag(FSovGameplayTags::Get().State_Resonance_Available, 1, EGameplayTagReplicationState::TagAndCountToAll); }
	ReleaseOwnedParticipation(); ContextTarget = nullptr; InteractionMission = nullptr;
	if (bSucceeded) { NextAllowedTime = GetWorld()->GetTimeSeconds() + 8.f; }
	if (GetOwner()) { GetOwner()->ForceNetUpdate(); } OnRep_Interaction();
}
void USovResonanceComponent::CancelInteraction()
{
	if (GetOwner() && GetOwner()->HasAuthority() && (Interaction.State == ESovResonanceState::Offered || Interaction.State == ESovResonanceState::Committed))
	{ Finish(false, TEXT("The action was canceled.")); }
}
void USovResonanceComponent::ApplyReleaseDamage(AActor* Source, const FVector& Origin, float Damage, float Radius)
{
	auto* SourceASC = ASC(Source);
	if (!SourceASC || !Alive(Source)) { return; }
	const FGuid Id = Interaction.InteractionId;
	TArray<TWeakObjectPtr<AActor>> Targets;
	for (TActorIterator<ANarrativeCharacter> It(GetWorld()); It; ++It)
	{ if (Alive(*It) && Hostile(Source, *It) && FVector::DistSquared(Origin, It->GetActorLocation()) <= FMath::Square(Radius) && Visible(Source, *It)) { Targets.Add(*It); } }
	for (auto Target : Targets)
	{
		if (Interaction.InteractionId != Id || Interaction.State != ESovResonanceState::Committed || !Alive(Source) || !Target.IsValid()) { return; }
		USovResonanceTicket* SourceTicket = nullptr;
		for (const auto& Candidate : Tickets) { if (Candidate && Candidate->ExpectedASC.Get() == SourceASC && IsTicketCurrent(Candidate)) { SourceTicket = Candidate; break; } }
		if (!SourceTicket) { return; }
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext(); Context.AddSourceObject(SourceTicket); Context.AddOrigin(Origin);
		auto Spec = SourceASC->MakeOutgoingSpec(USovGameplayEffect_SeleneDamage::StaticClass(), 1.f, Context);
		if (!Spec.IsValid()) { continue; }
		const auto& Tags = FSovGameplayTags::Get();
		Spec.Data->AddDynamicAssetTag(Tags.Ability_Echo); Spec.Data->AddDynamicAssetTag(Tags.Damage_Channel_Echo);
		Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, Damage);
		Spec.Data->SetSetByCallerMagnitude(Tags.SetByCaller_Damage_PoiseDamage, Damage * .5f);
		SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), ASC(Target.Get()));
	}
}
void USovResonanceComponent::ApplyCorridor()
{
	// The absorption is bounded protection during a narrow advance interval, not invulnerability.
	for (auto* Abilities : { PlayerASC.Get(), PartnerASC.Get() })
	{
		if (!Abilities || !ContextTarget) { return; }
		auto Spec = PlayerASC->MakeOutgoingSpec(USovGameplayEffect_ResonanceCorridor::StaticClass(), 1.f, PlayerASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			Spec.Data->DynamicGrantedTags.AddTag(FSovGameplayTags::Get().State_Resonance_ProtectedTarget);
			const float Duration = FMath::Clamp(ContextTarget->CorridorSeconds, .1f, 10.f);
			Spec.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Duration, Duration);
			Spec.Data->SetDuration(Duration, true); PlayerASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), Abilities);
		}
	}
}
bool USovResonanceComponent::ApplyPayoff(float DeltaTime)
{
	if (!ContextTarget) { return false; }
	const FGuid Id = Interaction.InteractionId;
	switch (Interaction.Type)
	{
	case ESovResonanceType::SupportSever:
	{
		auto* Guard = GetTarrik()->FindComponentByClass<USovGuardComponent>();
		auto* Link = ContextTarget->GetSupportLink();
		if (!Guard || !Guard->IsCounterWindowOpen() || !Link) { Finish(false, TEXT("The counter opening closed before the sever.")); return false; }
		FSovCommandLinkSeverResult Result;
		if (Link->TrySeverCommandLink(GetSelene(), Result) != ESovCommandLinkSeverResolution::NewlySevered)
		{ Finish(false, TEXT("The support link rejected the sever.")); return false; }
		if (Interaction.InteractionId != Id || Interaction.State != ESovResonanceState::Committed || !ContextTarget) { return false; }
		Guard->ExtendCounterWindow(FMath::Clamp(ContextTarget->CounterExtension, .1f, 3.f));
		if (!ContextTarget->SupportWeakPointId.IsNone())
		{ if (auto* Weak = ContextTarget->GetOwner()->FindComponentByClass<USovWeakPointComponent>()) { Weak->BreakWeakPointWithoutReward(ContextTarget->SupportWeakPointId); } }
		return true;
	}
	case ESovResonanceType::FormationBreach:
	{
		auto* Tarrik = Cast<ACharacter>(GetTarrik());
		if (!Tarrik || !Tarrik->GetCapsuleComponent()) { Finish(false, TEXT("Tarrik cannot enter the breach.")); return false; }
		if (!bPayoffStarted)
		{
			UNavigationPath* Path = UNavigationSystemV1::FindPathToActorSynchronously(GetWorld(), Tarrik->GetNavAgentLocation(), ContextTarget->GetOwner(), 100.f, Tarrik);
			if (!Tarrik->GetCharacterMovement()->IsMovingOnGround() || !Path || !Path->IsValid() || Path->IsPartial())
			{ Finish(false, TEXT("The breach has no complete grounded route to the exposed node.")); return false; }
			if (FVector::DistSquared(Tarrik->GetActorLocation(), ContextTarget->GetOwner()->GetActorLocation()) > FMath::Square(FMath::Clamp(ContextTarget->BreachMaximumDistance, 50.f, 1500.f)))
			{ Finish(false, TEXT("The exposed node is outside breach reach.")); return false; }
			bPayoffStarted = true; BreachCapsule = Tarrik->GetCapsuleComponent();
			const TArray<AActor*> Existing = BreachCapsule->CopyArrayOfMoveIgnoreActors();
			auto* Team = Cast<INarrativeTeamAgentInterface>(Tarrik);
			for (TActorIterator<ANarrativeCharacter> It(GetWorld()); It; ++It)
			{
				if (*It != Tarrik && Team && Team->GetTeamAttitudeTowards(**It) == ETeamAttitude::Friendly && !Existing.Contains(*It))
				{ BreachCapsule->IgnoreActorWhenMoving(*It, true); OwnedMovementIgnores.Add(*It); }
			}
			Tarrik->GetCharacterMovement()->StopMovementImmediately();
		}
		const FVector Target = ContextTarget->GetOwner()->GetActorLocation();
		const FVector Delta = Target - Tarrik->GetActorLocation();
		if (Delta.Size() <= 150.f) { return true; }
		const FVector Step = Delta.GetSafeNormal() * FMath::Min(Delta.Size() - 150.f, FMath::Clamp(ContextTarget->BreachSpeed, 100.f, 3000.f) * DeltaTime);
		auto* Navigation = UNavigationSystemV1::GetCurrent(GetWorld()); FNavLocation Projected;
		const FVector DesiredFeet = Tarrik->GetActorLocation() + Step - FVector(0,0,Tarrik->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (!Navigation || !Navigation->ProjectPointToNavigation(DesiredFeet, Projected, FVector(75.f,75.f,50.f))
			|| FVector::DistSquared(DesiredFeet, Projected.Location) > FMath::Square(80.f))
		{ Finish(false, TEXT("Navigation changed during the breach.")); return false; }
		FHitResult Hit; Tarrik->SetActorLocation(Tarrik->GetActorLocation() + Step, true, &Hit);
		if (Hit.bBlockingHit) { Finish(false, TEXT("World geometry or an enemy blocked the breach.")); }
		return false;
	}
	case ESovResonanceType::TerminalRelease:
	{
		const FVector Origin = ContextTarget->GetOwner()->GetActorLocation();
		const float Damage = FMath::Clamp(Interaction.Pressure, 0.f, 500.f) * FMath::Clamp(ContextTarget->ReleaseDamagePerPressure, 0.f, 2.f);
		const float Radius = FMath::Clamp(ContextTarget->ReleaseRadius, 50.f, 1500.f);
		// Distinct half-payloads retain each protagonist's actual instigator and ASC.
		ApplyReleaseDamage(GetTarrik(), Origin, Damage * .5f, Radius);
		if (Interaction.InteractionId == Id && Interaction.State == ESovResonanceState::Committed) { ApplyReleaseDamage(GetSelene(), Origin, Damage * .5f, Radius); }
		return Interaction.InteractionId == Id && Interaction.State == ESovResonanceState::Committed;
	}
	case ESovResonanceType::AdvanceCorridor: ApplyCorridor(); return true;
	default: return false;
	}
}
void USovResonanceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
	Super::TickComponent(DeltaTime, TickType, Function);
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutation) { return; }
	BindASCs();
	if (Interaction.State != ESovResonanceState::Offered && Interaction.State != ESovResonanceState::Committed) { return; }
	TGuardValue<bool> Mutation(bMutation, true);
	if (!IsContextValid() || bParticipantEnded) { Finish(false, TEXT("The complementary context or paired activation was interrupted.")); return; }
	if (Interaction.State == ESovResonanceState::Offered)
	{ if (GetWorld()->GetTimeSeconds() >= Interaction.ExpiresAt) { Finish(false, TEXT("The complementary opening expired.")); } return; }
	if (GetWorld()->GetTimeSeconds() - CommittedAt > 2.f) { Finish(false, TEXT("The paired action exceeded its recovery deadline.")); return; }
	const FGuid Id = Interaction.InteractionId;
	if (ApplyPayoff(DeltaTime) && Interaction.InteractionId == Id && Interaction.State == ESovResonanceState::Committed)
	{ Finish(true, FString()); }
}
