// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovCompanionApproachPolicy.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovCompanionCommandActivity.h"
#include "Companions/SovCoActionAnchor.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Abilities/SovGameplayAbility_TarrikGuard.h"
#include "Abilities/SovGameplayAbility_SeleneDeflection.h"
#include "Resonance/SovResonanceTargetComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovEchoComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Resonance/SovResonanceComponent.h"
#include "Resonance/SovResonanceAbility.h"
#include "Resonance/SovResonancePolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbility.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include "Components/EquipmentComponent.h"
#include "Items/WeaponItem.h"
#include "GAS/NarrativeGameplayAbility.h"

namespace
{
UNarrativeAbilitySystemComponent* CompanionASC(AActor* Actor)
{ return IsValid(Actor) ? Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)) : nullptr; }
bool HostileCompanionTarget(AActor* NPC, AActor* Target)
{
	auto* Team = Cast<INarrativeTeamAgentInterface>(NPC); auto* ASC = CompanionASC(Target);
	return Team && ASC && ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f
		&& Team->GetTeamAttitudeTowards(*Target) == ETeamAttitude::Hostile;
}
bool CompanionTargetDamageBlocked(const UNarrativeAbilitySystemComponent* ASC, const USovResonanceTargetComponent* Context)
{
	const auto& Tags = FSovGameplayTags::Get();
	return ASC->HasMatchingGameplayTag(Tags.State_Resonance_ProtectedTarget)
		|| ASC->HasMatchingGameplayTag(Tags.Character_Enemy_Boss)
		|| ASC->HasMatchingGameplayTag(Tags.State_Invulnerable)
		|| ASC->HasMatchingGameplayTag(Tags.State_Damage_Immune)
		|| ASC->HasMatchingGameplayTag(Tags.Damage_Immunity_All)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)
		|| (Context && Context->bRequiresPlayerFinish);
}
}
bool USovCompanionComponent::HasMissionPermission(ASovPlayerCharacterBase* Player) const
{
	auto* State = Player && Player->GetController() ? Player->GetController()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
	auto* Mission = State ? State->GetActiveMission() : nullptr;
	return State && State->IsStateValid() && Mission && Mission->AllowedCompanionIds.Contains(CompanionId);
}
bool USovCompanionComponent::SetLeader(ASovPlayerCharacterBase* Player, FString& Reason)
{
	Reason.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Player) || !Player->IsPlayerControlled() || !HasMissionPermission(Player)
		|| !CompanionASC(GetOwner()) || !CompanionASC(Player))
	{ Reason = TEXT("This mission does not permit that companion leader."); return false; }
	CancelContextCommand();
	ReleaseLeaderOwnership();
	Leader = Player; LeaderASC = CompanionASC(Player); BoundASC = CompanionASC(GetOwner());
	if (auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		CollisionLeader = Player; LeaderMovementPrimitive = Player->GetCapsuleComponent(); CompanionMovementPrimitive = NPC->GetCapsuleComponent();
		bOwnsLeaderIgnore = LeaderMovementPrimitive.IsValid() && !LeaderMovementPrimitive->CopyArrayOfMoveIgnoreActors().Contains(NPC);
		bOwnsCompanionIgnore = CompanionMovementPrimitive.IsValid() && !CompanionMovementPrimitive->CopyArrayOfMoveIgnoreActors().Contains(Player);
		if (bOwnsLeaderIgnore) { LeaderMovementPrimitive->IgnoreActorWhenMoving(NPC, true); }
		if (bOwnsCompanionIgnore) { CompanionMovementPrimitive->IgnoreActorWhenMoving(Player, true); }
	}
	LeaderASC->OnDamageResolvedAsSource.AddUniqueDynamic(this, &ThisClass::ObserveContribution);
	BoundASC->OnDamageResolvedAsSource.AddUniqueDynamic(this, &ThisClass::ObserveContribution);
	if (Leader->GetEchoComponent()) { Leader->GetEchoComponent()->OnEncounterScopeChanged.AddUniqueDynamic(this, &ThisClass::ResetContribution); }
	ResetContribution(true);
	return RequestCommand(Player, ESovCompanionCommand::Regroup, Player, Reason);
}
void USovCompanionComponent::ReleaseLeaderOwnership()
{
	if (IsValid(LeaderASC)) { LeaderASC->OnDamageResolvedAsSource.RemoveDynamic(this, &ThisClass::ObserveContribution); }
	if (IsValid(Leader) && Leader->GetEchoComponent()) { Leader->GetEchoComponent()->OnEncounterScopeChanged.RemoveDynamic(this, &ThisClass::ResetContribution); }
	if (bOwnsLeaderIgnore && LeaderMovementPrimitive.IsValid()) { LeaderMovementPrimitive->IgnoreActorWhenMoving(GetOwner(), false); }
	if (bOwnsCompanionIgnore && CompanionMovementPrimitive.IsValid() && CollisionLeader.IsValid())
	{ CompanionMovementPrimitive->IgnoreActorWhenMoving(CollisionLeader.Get(), false); }
	bOwnsLeaderIgnore = bOwnsCompanionIgnore = false; LeaderMovementPrimitive.Reset(); CompanionMovementPrimitive.Reset(); CollisionLeader.Reset();
	Leader = nullptr; LeaderASC = nullptr;
}
bool USovCompanionComponent::CanRequestCommand(ASovPlayerCharacterBase* Player, ESovCompanionCommand Command, AActor* Target, FString& Reason) const
{
	Reason.Reset();
	auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()); auto* Controller = NPC ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
	auto* Abilities = CompanionASC(GetOwner());
	if (bMutation || !NPC || !NPC->HasAuthority() || !NPC->IsAlive() || !Controller || !Controller->GetActivityComponent()
		|| !IsValid(Player) || !Player->IsPlayerControlled() || !Player->IsAlive() || !HasMissionPermission(Player)
		|| !Abilities || Abilities->GetAvatarActor() != NPC || Abilities->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Resonance_Committed)
		|| Abilities->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled))
	{ Reason = TEXT("The companion is not available for a contextual command."); return false; }
	if (Command == ESovCompanionCommand::MoveToAnchor || Command == ESovCompanionCommand::Interact)
	{ return CanRequestCoAction(Player, Cast<ASovCoActionAnchor>(Target), Reason); }
	if (Command == ESovCompanionCommand::ExecuteCoAction)
	{
		auto* Resonance = Player->FindComponentByClass<USovResonanceComponent>();
		return Resonance && Resonance->GetInteraction().State == ESovResonanceState::Offered && Resonance->GetInteraction().Partner == NPC;
	}
	if (Leader != Player) { Reason = TEXT("Set the mission-permitted leader before issuing tactical commands."); return false; }
	if (Command == ESovCompanionCommand::HoldPosition && Target && Target != Player
		&& (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->GetWorld() != GetWorld()
			|| !Target->GetRootComponent() || Target->GetActorLocation().ContainsNaN()
			|| FVector::DistSquared(Player->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(3000.f)))
	{ Reason = TEXT("Hold position requires a valid nearby same-world mark."); return false; }
	if (CommandState == ESovCompanionCommandState::MovingToAnchor)
	{ Reason = TEXT("Complete or cancel the required co-action before issuing another command."); return false; }
	if (UNPCActivity* Current = Controller->GetActivityComponent()->GetCurrentActivity(); Current && !Current->IsInterruptable())
	{ Reason = TEXT("A scripted activity owns the companion."); return false; }
	if (Command == ESovCompanionCommand::FocusTarget && (!IsValid(Target) || !HostileCompanionTarget(NPC, Target)
		|| FVector::DistSquared(Player->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(2500.f)))
	{ Reason = TEXT("Select a living nearby hostile target."); return false; }
	const auto* Team = Cast<INarrativeTeamAgentInterface>(NPC);
	if (Command == ESovCompanionCommand::DefendPerson && (!IsValid(Target) || !CompanionASC(Target)
		|| CompanionASC(Target)->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f || !Team
		|| Team->GetTeamAttitudeTowards(*Target) != ETeamAttitude::Friendly))
	{ Reason = TEXT("Defend requires a living friendly person."); return false; }
	return true;
}
bool USovCompanionComponent::RequestCommand(ASovPlayerCharacterBase* Player, ESovCompanionCommand Command, AActor* Target, FString& Reason)
{
	if (!CanRequestCommand(Player, Command, Target, Reason)) { return false; }
	if (Command == ESovCompanionCommand::MoveToAnchor || Command == ESovCompanionCommand::Interact)
	{ return RequestCoAction(Player, Cast<ASovCoActionAnchor>(Target), Reason); }
	if (Command == ESovCompanionCommand::ExecuteCoAction)
	{ auto* Resonance = Player->FindComponentByClass<USovResonanceComponent>(); return Resonance->ConfirmOffer(Resonance->GetInteraction().InteractionId, Reason); }
	CancelContextCommand();
	TGuardValue<bool> Mutation(bMutation, true);
	auto* NPC = CastChecked<ANarrativeNPCCharacter>(GetOwner()); auto* Controller = CastChecked<ANarrativeNPCController>(NPC->GetController());
	Activities = Controller->GetActivityComponent();
	const TSubclassOf<UNPCActivity> ActivityClass = NPC->IsA<ASovProtagonistCompanionCharacter>()
		? USovProtagonistCompanionActivity::StaticClass() : USovCompanionCommandActivity::StaticClass();
	if (!Activities->GetActivity(ActivityClass)) { Activities->AddActivity(ActivityClass, false); }
	CommandGoal = NewObject<USovCompanionCommandGoal>(Activities); CommandGoal->Companion = this; CommandGoal->RequestId = FGuid::NewGuid();
	CommandGoal->Command = Command; CommandGoal->Target = IsValid(Target) ? Target : Player;
	CommandGoal->bExplicitHoldTarget = Command == ESovCompanionCommand::HoldPosition && IsValid(Target) && Target != Player;
	CommandGoal->HoldLocation = CommandGoal->bExplicitHoldTarget ? Target->GetActorLocation() : Player->GetActorLocation();
	CommandGoal->GoalKey = this; Leader = Player; bCommandInterrupted = false; SetComponentTickEnabled(true);
	if (!Activities->AddGoal(CommandGoal, true)) { Reason = TEXT("Narrative rejected the contextual command."); CancelContextCommand(); return false; }
	return true;
}
bool USovCompanionComponent::IsCommandCurrent(const USovCompanionCommandGoal* Goal) const
{
	return IsValid(Goal) && Goal == CommandGoal && Goal->RequestId.IsValid() && !bCommandInterrupted && IsValid(Leader)
		&& (!Goal->bExplicitHoldTarget || (IsValid(Goal->Target) && !Goal->Target->IsActorBeingDestroyed()
			&& Goal->Target->GetWorld() == GetWorld() && Leader->IsPlayerControlled() && Leader->IsCharacterReady()
			&& IsValid(LeaderASC) && LeaderASC->GetAvatarActor() == Leader));
}
void USovCompanionComponent::NotifyCommandInterrupted(USovCompanionCommandGoal* Goal)
{ if (IsCommandCurrent(Goal)) { bCommandInterrupted = true; SetComponentTickEnabled(true); } }
bool USovCompanionComponent::HasAcceptedHoldPosition(const AActor* Target) const
{
	const auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner());
	if (bMutation || !IsValid(Target) || !IsCommandCurrent(CommandGoal) || !IsValid(Activities)
		|| !NPC || Activities->GetOwner() != NPC->GetController()
		|| CommandGoal->Command != ESovCompanionCommand::HoldPosition
		|| !CommandGoal->bExplicitHoldTarget || CommandGoal->Target != Target) { return false; }
	bool bFound = false;
	const auto* AcceptedGoal = Activities->GetGoalByKey(USovCompanionCommandGoal::StaticClass(), this, bFound);
	return bFound && AcceptedGoal == CommandGoal;
}
void USovCompanionComponent::CancelContextCommand()
{
	USovCompanionCommandGoal* Previous = CommandGoal; CommandGoal = nullptr; bCommandInterrupted = false;
	auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()); auto* Controller = NPC ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
	if (Controller)
	{
		if (Controller->GetPathFollowingComponent() && Controller->GetPathFollowingComponent()->GetCurrentRequestId() == CommandMoveId) { Controller->StopMovement(); }
		if (OwnedFocus.IsValid() && Controller->GetFocusActor() == OwnedFocus.Get())
		{ if (PreviousFocus.IsValid()) { Controller->SetFocus(PreviousFocus.Get()); } else { Controller->ClearFocus(EAIFocusPriority::Gameplay); } }
	}
	CommandMoveId = FAIRequestID::InvalidRequest; OwnedFocus.Reset(); PreviousFocus.Reset();
	if (BoundASC && OwnedCommandAttack.IsValid()) { BoundASC->CancelAbilityHandle(OwnedCommandAttack); }
	OwnedCommandAttack = {};
	if (IsValid(Activities) && IsValid(Previous)) { Activities->RemoveGoal(Previous); }
	if (CommandState != ESovCompanionCommandState::MovingToAnchor) { SetComponentTickEnabled(false); }
}
void USovCompanionComponent::ObserveContribution(const FSovDamageResult& Result)
{
	const float Damage = FMath::Max(0.f, Result.AppliedHealthDamage) + FMath::Max(0.f, Result.AppliedShieldDamage);
	if (Result.SourceActor == GetOwner()) { CompanionContribution += Damage; }
	else if (Result.SourceActor == Leader) { PlayerContribution += Damage; }
}
void USovCompanionComponent::ResetContribution(bool bStarted)
{
	PlayerContribution = 0.f; CompanionContribution = 0.f;
	// The opening allowance belongs to the scope, so it restarts with the budget it relaxes.
	ContributionScopeOpenedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
	OpeningCombatAt = -1.;
}

bool USovCompanionComponent::IsInOpeningContribution() const
{
	const UWorld* const World = GetWorld();
	return World && SovResonancePolicy::WithinOpeningContribution(World->GetTimeSeconds(),
		OpeningCombatAt >= 0. ? OpeningCombatAt : ContributionScopeOpenedAt,
		FMath::IsFinite(OpeningContributionSeconds) ? FMath::Clamp(OpeningContributionSeconds, 0.f, 30.f) : 0.f);
}
ANarrativeCharacter* USovCompanionComponent::ResolveCommandAttackTarget(ANarrativeCharacter* Character)
{
	if (!IsValid(Character)) { return nullptr; }
	auto* Component = Character->FindComponentByClass<USovCompanionComponent>();
	auto* Controller = Cast<ANarrativeNPCController>(Character->GetController());
	auto* Abilities = CompanionASC(Character);
	if (!Component || !Controller || !Abilities || !Component->IsCommandCurrent(Component->CommandGoal)
		|| !IsValid(Component->Activities) || Component->Activities->GetCurrentActivityGoal() != Component->CommandGoal
		|| !IsValid(Component->Leader) || !Component->Leader->IsAlive()
		|| !Component->HasMissionPermission(Component->Leader)) { return nullptr; }
	auto* Target = Cast<ANarrativeCharacter>(Component->OwnedFocus.Get());
	return IsValid(Target) && Controller->GetFocusActor() == Target && HostileCompanionTarget(Character, Target)
		&& Abilities->IsBotAttackExecutionValid(Target, Component->OwnedCommandAttack) ? Target : nullptr;
}
void USovCompanionComponent::TickContextCommand(USovCompanionCommandGoal* Goal)
{
	if (IsValid(Goal) && Goal == CommandGoal && Goal->bExplicitHoldTarget && !IsCommandCurrent(Goal))
	{ CancelContextCommand(); return; }
	if (!IsCommandCurrent(Goal) || !GetOwner()->HasAuthority()) { return; }
	auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()); auto* Controller = NPC ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
	auto* Abilities = CompanionASC(GetOwner());
	if (!NPC || !Controller || !Abilities || !NPC->IsAlive() || !Leader->IsAlive() || !HasMissionPermission(Leader)
		|| !Activities || Activities->GetCurrentActivityGoal() != Goal) { CancelContextCommand(); return; }
	const auto& Tags = FSovGameplayTags::Get(); const auto& N = FNarrativeGameplayTags::Get();
	if (Abilities->HasMatchingGameplayTag(Tags.State_Resonance_Committed) || Abilities->HasMatchingGameplayTag(N.State_SequencerControlled)) { return; }
	if (OwnedCommandAttack.IsValid())
	{
		const auto* Spec = Abilities->FindAbilitySpecFromHandle(OwnedCommandAttack);
		const bool bDefense = Spec && Spec->Ability && (Spec->Ability->IsA<USovGameplayAbility_TarrikGuard>()
			|| Spec->Ability->IsA<USovGameplayAbility_SeleneDeflection>());
		if (Spec && Spec->IsActive() && !bDefense) { return; } // Let native attack/montage completion own its lifetime.
		if (Spec && Spec->IsActive() && GetWorld()->GetTimeSeconds() - CommandAttackStarted < 1.5f) { return; }
		if (Spec && Spec->IsActive()) { Abilities->CancelAbilityHandle(OwnedCommandAttack); }
		OwnedCommandAttack = {};
	}
	if (Abilities->HasMatchingGameplayTag(N.State_Busy)) { return; }
	// An authored hold mark is explicit movement intent, not a lost companion.
	// Never use hidden recovery to teleport it away from that mark.
	if (!Goal->bExplicitHoldTarget && TryRecoverSeparation()) { return; }
	AActor* Focus = Goal->Command == ESovCompanionCommand::FocusTarget ? Goal->Target.Get() : nullptr;
	FVector Destination = Goal->Command == ESovCompanionCommand::HoldPosition ? Goal->HoldLocation
		: Goal->Command == ESovCompanionCommand::DefendPerson && IsValid(Goal->Target) ? Goal->Target->GetActorLocation() : Leader->GetActorLocation();
	if (!Focus && Goal->Command != ESovCompanionCommand::HoldPosition)
	{
		float Best = FMath::Square(1000.f);
		float DeferredBest = Best;
		AActor* DeferredFocus = nullptr;
		for (TActorIterator<ANarrativeNPCCharacter> It(GetWorld()); It; ++It)
		{
			const float Dist = FVector::DistSquared(Destination, It->GetActorLocation());
			if (Dist >= FMath::Square(1000.f) || !HostileCompanionTarget(NPC, *It) || !Controller->LineOfSightTo(*It)) { continue; }
			const auto* TargetASC = CompanionASC(*It);
			const auto* TargetContext = It->FindComponentByClass<USovResonanceTargetComponent>();
			const bool bDeferred = TargetASC->HasMatchingGameplayTag(Tags.State_Target_Unfinishable)
				|| CompanionTargetDamageBlocked(TargetASC, TargetContext);
			if (bDeferred)
			{
				if (Dist < DeferredBest) { DeferredBest = Dist; DeferredFocus = *It; }
			}
			else if (Dist < Best) { Best = Dist; Focus = *It; }
		}
		// An encounter-held Elite must not monopolize autonomous focus while an ordinary
		// hostile is still in the leader's defense area. Retain it as a fallback.
		if (!Focus) { Focus = DeferredFocus; }
	}
	bool bCombatApproach = false;
	if (IsValid(Focus) && HostileCompanionTarget(NPC, Focus))
	{
		if (!OwnedFocus.IsValid()) { PreviousFocus = Controller->GetFocusActor(); }
		Controller->SetFocus(Focus); OwnedFocus = Focus;
		// Use the normal replicated wield contract: it owns weapon attachment,
		// animation layers and item ability grants. Never manufacture an attack grant.
		if (NPC->IsA<ASovProtagonistCompanionCharacter>() && !NPC->GetWeapon()
			&& NPC->GetWeaponWieldState().WieldSlots.IsEmpty() && NPC->GetEquipmentComponent()
			&& !Abilities->HasMatchingGameplayTag(N.State_Weapon_Equipping))
		{
			TArray<UEquippableItem*> Equipped;
			NPC->GetEquipmentComponent()->GetEquippedItemsOfClass(UWeaponItem::StaticClass(), Equipped);
			for (auto* Item : Equipped)
			{
				auto* Weapon = Cast<UWeaponItem>(Item);
				if (!IsValid(Weapon) || !Weapon->GetEquippedSlot().IsValid()) { continue; }
				bool bPermitted = false;
				for (const auto& Class : Weapon->GetWeaponAbilities())
				{ if (CuratedAbilities.Contains(Class.Get())) { bPermitted = true; break; } }
				if (!bPermitted) { continue; }
				FWeaponWieldState Wield;
				Wield.EquipSlots.AddTag(Weapon->GetEquippedSlot()); Wield.WieldSlots.AddTag(N.Weapon_WieldSlot_Mainhand);
				NPC->SetWieldState(Wield);
				return; // Grant/visual callbacks may replace command ownership; revalidate next tick.
			}
		}
		auto* TargetASC = CompanionASC(Focus);
		// Curated AI may neither finish a protected interaction target nor spend an unlisted ability.
		const auto* Context = Focus->FindComponentByClass<USovResonanceTargetComponent>();
		const bool bProtected = CompanionTargetDamageBlocked(TargetASC, Context);
		if (!bProtected && OpeningCombatAt < 0.)
		{ OpeningCombatAt = GetWorld()->GetTimeSeconds(); }
		// Only the protagonist proxy gets its copied defense kit. Ordinary allies do not become guard/deflect clones.
		if (NPC->IsA<ASovProtagonistCompanionCharacter>() && GetWorld()->GetTimeSeconds() >= NextCommandDefense
			&& TargetASC->HasMatchingGameplayTag(N.State_NPC_Activity_Attacking) && Controller->LineOfSightTo(Focus))
		{
			for (const auto& Class : CuratedAbilities)
			{
				if (!Class || (!Class->IsChildOf(USovGameplayAbility_TarrikGuard::StaticClass()) && !Class->IsChildOf(USovGameplayAbility_SeleneDeflection::StaticClass()))) { continue; }
				const auto* Spec = Abilities->FindAbilitySpecFromClass(Class);
				if (!Spec) { continue; }
				OwnedCommandAttack = Spec->Handle; CommandAttackStarted = GetWorld()->GetTimeSeconds() - 1.f;
				if (Abilities->TryActivateAbility(OwnedCommandAttack))
				{
					// Defense has its own cadence. An enemy's sustained attacking tag
					// must not repeatedly postpone our next ordinary weapon attack.
					NextCommandDefense = GetWorld()->GetTimeSeconds() + 4.f;
					return;
				}
				OwnedCommandAttack = {}; // An unavailable defense must not starve ordinary attacks.
			}
		}
		if (!bProtected && GetWorld()->GetTimeSeconds() >= NextCommandAttack
			&& SovResonancePolicy::MayCommitAttack(IsInOpeningContribution(), CompanionContribution, PlayerContribution,
				FMath::Clamp(ContributionFraction, .15f, .25f),
				FMath::IsFinite(MinimumMeaningfulContribution) ? FMath::Max(MinimumMeaningfulContribution, 0.f) : 0.f))
		{
			for (const auto& Candidate : Abilities->GetBotAttackCandidates(Focus, FGameplayTag()))
			{
				const auto* Spec = Abilities->FindAbilitySpecFromHandle(Candidate.Handle);
				if (!Spec || !Spec->Ability || !CuratedAbilities.Contains(Spec->Ability->GetClass())) { continue; }
				if (!Candidate.bAvailable)
				{
					// Respect explicit holds and the existing ten-metre defense area.
					// Native attack ranges determine approach; range checks remain authoritative.
					// Respect explicit holds. Native attack ranges determine approach; the leash keeps the
					// companion within the distance the order itself admits. See SovCompanionApproachPolicy.
					FVector ApproachPoint;
					if (!bCombatApproach && Goal->Command != ESovCompanionCommand::HoldPosition && !Candidate.bInRange
						&& SovCompanionApproachPolicy::SelectApproachPoint(NPC->GetActorLocation(), Focus->GetActorLocation(),
							Leader->GetActorLocation(), Candidate.MinimumRange, Candidate.PreferredRange, Candidate.MaximumRange,
							Goal->Command == ESovCompanionCommand::FocusTarget ? SovCompanionApproachPolicy::OrderedLeash
								: SovCompanionApproachPolicy::UnorderedLeash, ApproachPoint))
					{ Destination = ApproachPoint; bCombatApproach = true; }
					continue;
				}
				OwnedCommandAttack = Candidate.Handle; CommandAttackStarted = GetWorld()->GetTimeSeconds(); NextCommandAttack = CommandAttackStarted + 2.f;
				if (Controller->GetPathFollowingComponent() && Controller->GetPathFollowingComponent()->GetCurrentRequestId() == CommandMoveId)
				{ Controller->StopMovement(); }
				if (!Abilities->TryActivateBotAttack(Focus, Candidate.Handle)) { OwnedCommandAttack = {}; }
				return;
			}
		}
	}
	else if (OwnedFocus.IsValid())
	{
		if (Controller->GetFocusActor() == OwnedFocus.Get())
		{ Controller->ClearFocus(EAIFocusPriority::Gameplay); if (PreviousFocus.IsValid()) { Controller->SetFocus(PreviousFocus.Get()); } }
		OwnedFocus.Reset(); PreviousFocus.Reset();
	}
	const float MovementThreshold = bCombatApproach ? 25.f : Goal->bExplicitHoldTarget ? 75.f : 250.f;
	if (FVector::DistSquared(NPC->GetActorLocation(), Destination) > FMath::Square(MovementThreshold)
		&& Controller->GetMoveStatus() != EPathFollowingStatus::Moving && GetWorld()->GetTimeSeconds() >= NextMoveAttempt)
	{
		NextMoveAttempt = GetWorld()->GetTimeSeconds() + 1.f;
		FAIMoveRequest Request(Destination); Request.SetAcceptanceRadius(bCombatApproach ? 10.f : Goal->bExplicitHoldTarget ? 25.f : 150.f);
		if (bCombatApproach) { Request.SetReachTestIncludesAgentRadius(false); Request.SetReachTestIncludesGoalRadius(false); }
		Request.SetAllowPartialPath(false); Request.SetUsePathfinding(true);
		const auto Move = Controller->MoveTo(Request); CommandMoveId = Move.MoveId;
		// Retain authored intent and retry at a bounded cadence. Hidden recovery has its own stricter gate.
		if (Move.Code == EPathFollowingRequestResult::Failed) { CommandMoveId = FAIRequestID::InvalidRequest; }
	}
}
bool USovCompanionComponent::TryRecoverSeparation()
{
	auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner());
	if (!NPC || !Leader || !IsValid(RecoveryAnchor)
		|| !SovResonancePolicy::MayRecoverSeparation(FVector::DistSquared(NPC->GetActorLocation(), Leader->GetActorLocation()),
			FVector::DistSquared(RecoveryAnchor->GetActorLocation(), Leader->GetActorLocation()), bInAuthoredSplitPhase,
			RecoveryAnchor->GetWorld() == GetWorld())) { return false; }
	auto* Navigation = UNavigationSystemV1::GetCurrent(GetWorld()); const auto* Capsule = NPC->GetCapsuleComponent();
	FNavLocation Projected;
	if (!Navigation || !Capsule || !Navigation->ProjectPointToNavigation(RecoveryAnchor->GetActorLocation(), Projected, FVector(75.f,75.f,150.f))) { return false; }
	const FVector Destination = Projected.Location + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight() + 2.f);
	FCollisionQueryParams Query = NPC->GetIgnoreCharacterParams();
	if (GetWorld()->OverlapBlockingTestByChannel(Destination, NPC->GetActorQuat(), Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query)
		|| !IsFallbackHiddenFromAllPlayers(Destination)) { return false; }
	UNavigationPath* Path = UNavigationSystemV1::FindPathToActorSynchronously(GetWorld(), Projected.Location, Leader, 50.f, NPC);
	if (!Path || !Path->IsValid() || Path->IsPartial()) { return false; }
	if (auto* Controller = Cast<ANarrativeNPCController>(NPC->GetController())) { Controller->StopMovement(); }
	NPC->GetCharacterMovement()->StopMovementImmediately();
	return NPC->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
}
bool USovCompanionComponent::CanProvideRescue(ASovPlayerCharacterBase* Player, FString& Reason) const
{
	Reason.Reset(); auto* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()); auto* Abilities = CompanionASC(GetOwner());
	const auto* Settings = UNarrativeGameUserSettings::GetSovSessionSettings(GetOwner());
	if (!bMayRescue || !Settings || !Settings->IsCompanionRescueAllowed() || !NPC || !NPC->HasAuthority() || !NPC->IsAlive()
		|| !IsValid(Player) || !HasMissionPermission(Player) || !Abilities || Abilities->GetAvatarActor() != NPC
		|| Abilities->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled)
		|| Abilities->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Resonance_Committed)
		|| Abilities->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Poise_Broken)
		|| Abilities->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy)
		|| CommandState == ESovCompanionCommandState::MovingToAnchor || !FMath::IsFinite(RescueRange)
		|| FVector::DistSquared(NPC->GetActorLocation(), Player->GetActorLocation()) > FMath::Square(FMath::Clamp(RescueRange, 100.f, 2500.f)))
	{ Reason = TEXT("No permitted, living, unreserved companion is in rescue range."); return false; }
	UNavigationPath* Path = UNavigationSystemV1::FindPathToActorSynchronously(GetWorld(), NPC->GetNavAgentLocation(), Player, 50.f, NPC);
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->GetPathLength() > RescueRange)
	{ Reason = TEXT("The companion has no complete reachable rescue path."); return false; }
	return true;
}

bool USovCompanionComponent::LimitSovDamage(AActor* Target, const FGameplayEffectContextHandle& Context,
	float& ShieldDamage, float& HealthDamage, float& PoiseDamage) const
{
	const auto* Ticket = Cast<USovResonanceTicket>(Context.GetSourceObject());
	if (Ticket && Ticket->Coordinator.IsValid() && Ticket->Coordinator->IsTicketCurrent(Ticket)
		&& Ticket->ExpectedASC.Get() == CompanionASC(GetOwner()) && Context.GetOriginalInstigatorAbilitySystemComponent() == Ticket->ExpectedASC.Get()) { return true; }
	const auto* TargetContext = IsValid(Target) ? Target->FindComponentByClass<USovResonanceTargetComponent>() : nullptr;
	if (!IsValid(Leader) || !HasMissionPermission(Leader) || (TargetContext && TargetContext->bRequiresPlayerFinish))
	{ ShieldDamage = HealthDamage = PoiseDamage = 0.f; return false; }
	// Inside the opening allowance the companion is not budgeted at all. Its damage still accrues, so
	// the ordinary cap resumes the moment the window closes.
	if (IsInOpeningContribution()) { return true; }
	const float Fraction = FMath::Clamp(ContributionFraction, .15f, .25f);
	// Still a backstop for an attack already in flight when the budget ran out. Starting one that cannot
	// land meaningfully is refused at selection instead, which is where the decision belongs.
	const float Remaining = SovResonancePolicy::RemainingContribution(CompanionContribution, PlayerContribution, Fraction);
	const float Requested = FMath::Max(0.f, ShieldDamage) + FMath::Max(0.f, HealthDamage);
	ShieldDamage = FMath::Min(ShieldDamage, Remaining);
	HealthDamage = FMath::Min(HealthDamage, FMath::Max(0.f, Remaining - ShieldDamage));
	if (auto* TargetASC = CompanionASC(Target); TargetASC && TargetASC->HasMatchingGameplayTag(FSovGameplayTags::Get().Character_Enemy_Boss))
	{ HealthDamage = FMath::Min(HealthDamage, FMath::Max(0.f, TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) - 1.f)); }
	PoiseDamage *= Requested > KINDA_SMALL_NUMBER ? FMath::Clamp((ShieldDamage + HealthDamage) / Requested, 0.f, 1.f) : (Remaining > 0.f ? 1.f : 0.f);
	return Remaining > 0.f && (Requested <= KINDA_SMALL_NUMBER || ShieldDamage + HealthDamage > 0.f);
}
