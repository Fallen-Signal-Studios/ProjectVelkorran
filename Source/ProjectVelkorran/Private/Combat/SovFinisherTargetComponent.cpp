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
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealFramework/NarrativeCharacter.h"
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
        || !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsValid(Attacker) || ReservedBy.IsValid()
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
    OutLease.Invalidate();
    if (!IsValid(Ability) || !IsValid(Attacker) || !IsValid(GetOwner())) { return false; }
    TStrongObjectPtr<USovFinisherTargetComponent> TargetLifetime(this);
    TStrongObjectPtr<USovGameplayAbility_Finisher> AbilityLifetime(Ability);
    TStrongObjectPtr<AActor> AttackerLifetime(Attacker), OwnerLifetime(GetOwner());
    const uint64 Epoch = Ability->GetFinisherActivationEpoch();
    const FGuid PreviousReservation = Reservation;
    const FGameplayTag Phase = RequiredPhaseTag;
    const ESovFinisherTargetKind Kind = TargetKind;
    const float Damage = PhaseDamage, Threshold = LowHealthThreshold;
    if (!Ability->IsFinisherActivationCurrent(Epoch, Attacker) || !IsAvailableFor(Attacker)) { return false; }
    // Team policy may execute authored callbacks. Never overwrite a replacement
    // reservation or continue an activation/configuration that changed inside it.
    if (!Ability->IsFinisherActivationCurrent(Epoch, Attacker) || ReservedBy.IsValid()
        || Reservation != PreviousReservation || !IsValid(GetOwner()) || GetOwner() != OwnerLifetime.Get()
        || GetOwner()->IsActorBeingDestroyed() || !GetOwner()->HasAuthority()
        || !IsValid(Attacker) || Attacker->IsActorBeingDestroyed() || !Attacker->HasAuthority()
        || RequiredPhaseTag != Phase || TargetKind != Kind || PhaseDamage != Damage || LowHealthThreshold != Threshold)
    { return false; }
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
        && OwnsLease(Ability, Value) && Live(ASC) && ASC->GetAvatarActor()==GetOwner()
        && (!bPhase || (ReservedPhase==RequiredPhaseTag && ReservedPhase.IsValid() && ASC->HasMatchingGameplayTag(ReservedPhase)
            && !ResolvedPhases.HasTagExact(ReservedPhase)));
}
void USovFinisherTargetComponent::Release(const USovGameplayAbility_Finisher* Ability, const FGuid& Value)
{ if (OwnsLease(Ability, Value)) { ReservedBy.Reset(); Reservation.Invalidate(); ReservedPhase=FGameplayTag(); } }
bool USovFinisherTargetComponent::CommitPhase(const USovGameplayAbility_Finisher* Ability, const FGuid& Value)
{
    if (!OwnsLease(Ability, Value) || !ReservedPhase.IsValid() || ResolvedPhases.HasTagExact(ReservedPhase)) { return false; }
    ResolvedPhases.AddTag(ReservedPhase);
    PendingPhaseOutcomes.AddTag(ReservedPhase);
    return true;
}
void USovFinisherTargetComponent::PublishCommittedPhase(FGameplayTag Phase, AActor* Instigator,
    const FGameplayEffectContextHandle& Context)
{
    AActor* Target = GetOwner();
    if (!ResolvedPhases.HasTagExact(Phase)) { PendingPhaseOutcomes.RemoveTag(Phase); return; }
    if (!PendingPhaseOutcomes.HasTagExact(Phase) || !IsValid(Target) || Target->IsActorBeingDestroyed()
        || !Target->HasAuthority()) { return; }
    const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!IsValid(ASC) || ASC->GetAvatarActor() != Target) { return; }
    // Delivery belongs to the durable target outcome, never the animation lease.
    // Consume before broadcasting so reentrant publication/checkpointing cannot duplicate it.
    PendingPhaseOutcomes.RemoveTag(Phase);
    FGameplayEventData Payload;
    Payload.EventTag = FSovGameplayTags::Get().Event_Finisher_PhaseResolved;
    Payload.Instigator = IsValid(Instigator) ? Instigator : nullptr;
    Payload.Target = Target; Payload.TargetTags.AddTag(Phase); Payload.ContextHandle = Context;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Target, Payload.EventTag, Payload);
}
void USovFinisherTargetComponent::PublishPendingPhases()
{
    const FGameplayTagContainer Pending = PendingPhaseOutcomes;
    for (const FGameplayTag& Phase : Pending)
    {
        if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) { return; }
        // After restore, phase identity is stable; an expired attacker is intentionally absent.
        PublishCommittedPhase(Phase, nullptr, FGameplayEffectContextHandle());
    }
}
void USovFinisherTargetComponent::Load_Implementation()
{
    // Active action ownership is transient. Completed outcomes do not replay, but
    // a save taken inside strike damage must finish its pending delivery after restore.
    ReservedBy.Reset(); Reservation.Invalidate(); ReservedPhase=FGameplayTag();
    if (auto* Character = Cast<ANarrativeCharacter>(GetOwner()))
    { Character->OnASCInitialized.AddUniqueDynamic(this, &ThisClass::HandleOutcomeASCInitialized); }
    HandleOutcomeASCInitialized();
}
void USovFinisherTargetComponent::HandleOutcomeASCInitialized()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PendingPhaseTimer);
        if (!PendingPhaseOutcomes.IsEmpty())
        { PendingPhaseTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::PublishPendingPhases); }
    }
}
void USovFinisherTargetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Character = Cast<ANarrativeCharacter>(GetOwner()))
    { Character->OnASCInitialized.RemoveDynamic(this, &ThisClass::HandleOutcomeASCInitialized); }
    if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(PendingPhaseTimer); }
    ReservedBy.Reset(); Reservation.Invalidate(); Super::EndPlay(Reason);
}
