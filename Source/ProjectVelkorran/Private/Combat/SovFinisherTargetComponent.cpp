// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Combat/SovFinisherTargetComponent.h"
#include "Combat/SovFinisherPolicy.h"
#include "Abilities/SovGameplayAbility_Finisher.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "ArsenalStatics.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
namespace
{
    bool Live(const UAbilitySystemComponent* ASC)
    {
        return IsValid(ASC) && IsValid(ASC->GetAvatarActor()) && !ASC->GetAvatarActor()->IsActorBeingDestroyed()
            && ASC->GetSet<UNarrativeAttributeSetBase>() && !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead)
            && !ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().State_Fatal)
            && ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f;
    }
}
USovFinisherTargetComponent::USovFinisherTargetComponent() { PrimaryComponentTick.bCanEverTick=false; }
bool USovFinisherTargetComponent::IsAvailableFor(AActor* Attacker) const
{
    if (static_cast<uint8>(TargetKind)>static_cast<uint8>(ESovFinisherTargetKind::Boss)
        || !FMath::IsFinite(PhaseDamage) || PhaseDamage<0.f
        || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsValid(Attacker) || ReservedBy.IsValid() || PendingOutcome.IsValid()
        || GetOwner()==Attacker || UArsenalStatics::GetAttitude(Attacker, GetOwner())!=ETeamAttitude::Hostile) { return false; }
    const auto* ASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    const auto& T=FSovGameplayTags::Get();
    if (!Live(ASC) || ASC->GetAvatarActor()!=GetOwner() || ASC->HasMatchingGameplayTag(T.Character_Player)
        || ASC->HasMatchingGameplayTag(T.State_Finisher_Target) || ASC->HasMatchingGameplayTag(T.State_InterruptProtected)
        || ASC->HasMatchingGameplayTag(T.State_Invulnerable) || ASC->HasMatchingGameplayTag(T.State_Damage_Immune)
        || ASC->HasMatchingGameplayTag(T.Damage_Immunity_All) || ASC->HasMatchingGameplayTag(T.Damage_Immunity_Edge)
        || ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)) { return false; }
    const bool bPhase=TargetKind!=ESovFinisherTargetKind::Normal || ASC->HasMatchingGameplayTag(T.Character_Enemy_Boss);
    if (bPhase && (!RequiredPhaseTag.IsValid() || !ASC->HasMatchingGameplayTag(RequiredPhaseTag)
        || ResolvedPhases.HasTagExact(RequiredPhaseTag))) { return false; }
    return SovFinisher::Vulnerable(true, ASC->HasMatchingGameplayTag(T.State_Poise_Broken),
        ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()),
        ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxHealthAttribute()), LowHealthThreshold);
}
bool USovFinisherTargetComponent::Reserve(USovGameplayAbility_Finisher* Ability, AActor* Attacker, FGuid& OutLease)
{
    if (!IsValid(Ability) || !IsAvailableFor(Attacker)) { return false; }
    ReservedBy=Ability; Reservation=FGuid::NewGuid(); ReservedPhase=RequiredPhaseTag; OutLease=Reservation; return true;
}
bool USovFinisherTargetComponent::OwnsLease(const USovGameplayAbility_Finisher* Ability, const FGuid& Value) const
{ return Value.IsValid() && Reservation==Value && ReservedBy.Get()==Ability; }
bool USovFinisherTargetComponent::IsReservedTargetValid(const USovGameplayAbility_Finisher* Ability, const FGuid& Value, AActor* Attacker) const
{
    const auto* ASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    const bool bPhase=TargetKind!=ESovFinisherTargetKind::Normal || (ASC && ASC->HasMatchingGameplayTag(FSovGameplayTags::Get().Character_Enemy_Boss));
    return OwnsLease(Ability, Value) && Live(ASC) && IsValid(Attacker)
        && UArsenalStatics::GetAttitude(Attacker, GetOwner())==ETeamAttitude::Hostile
        && (!bPhase || (ReservedPhase==RequiredPhaseTag && ReservedPhase.IsValid() && ASC->HasMatchingGameplayTag(ReservedPhase)
            && !ResolvedPhases.HasTagExact(ReservedPhase)));
}
void USovFinisherTargetComponent::Release(const USovGameplayAbility_Finisher* Ability, const FGuid& Value)
{ if (OwnsLease(Ability, Value)) { ReservedBy.Reset(); Reservation.Invalidate(); ReservedPhase=FGameplayTag(); } }
bool USovFinisherTargetComponent::BeginPhaseOutcome(const USovGameplayAbility_Finisher* Ability, const FGuid& Value, FGuid& OutOutcome)
{
    if (!OwnsLease(Ability, Value) || PendingOutcome.IsValid() || !ReservedPhase.IsValid()
        || ResolvedPhases.HasTagExact(ReservedPhase)) { return false; }
    PendingOutcome=FGuid::NewGuid(); PendingPhase=ReservedPhase; OutOutcome=PendingOutcome; return true;
}
bool USovFinisherTargetComponent::CommitPhaseOutcome(const FGuid& Outcome, FGameplayTag Phase,
    bool bAcceptedDamage, bool bSameTargetGeneration)
{
    if (!SovFinisher::CanCommitOutcome(bAcceptedDamage, bSameTargetGeneration,
        Outcome.IsValid() && PendingOutcome==Outcome && PendingPhase==Phase && Phase.IsValid(),
        ResolvedPhases.HasTagExact(Phase))) { return false; }
    ResolvedPhases.AddTag(Phase);
    // Clear before publishing. Reentrant saves now see both damage and outcome.
    FinishPhaseOutcome(Outcome);
    return true;
}
void USovFinisherTargetComponent::FinishPhaseOutcome(const FGuid& Outcome)
{
    if (Outcome.IsValid() && PendingOutcome==Outcome) { PendingOutcome.Invalidate(); PendingPhase=FGameplayTag(); }
}
void USovFinisherTargetComponent::Load_Implementation()
{
    // Active action ownership is transient. Loading never replays the phase event.
    ReservedBy.Reset(); Reservation.Invalidate(); ReservedPhase=FGameplayTag();
    PendingOutcome.Invalidate(); PendingPhase=FGameplayTag();
}
void USovFinisherTargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{ ReservedBy.Reset(); Reservation.Invalidate(); PendingOutcome.Invalidate(); PendingPhase=FGameplayTag(); Super::EndPlay(Reason); }
