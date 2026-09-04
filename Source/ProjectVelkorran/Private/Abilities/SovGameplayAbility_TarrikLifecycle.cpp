// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Abilities/SovGameplayAbility_TarrikEcho.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"

bool USovGameplayAbility_TarrikEchoBase::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	const auto& Tags = FSovGameplayTags::Get();
	if (!ASC || !ASC->HasMatchingGameplayTag(Tags.Character_Player_Tarrik)
		|| ASC->HasMatchingGameplayTag(Tags.Character_Player_Selene)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping))
	{
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsBlocked); }
		return false;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void USovGameplayAbility_TarrikEchoBase::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	++TarrikActivationSerial;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USovGameplayAbility_TarrikEchoBase::EndAbility(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	++TarrikActivationSerial; // invalidate callbacks before any Blueprint Ended hook
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TarrikReleaseTimer);
		World->GetTimerManager().ClearTimer(TarrikRecoveryTimer);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool USovGameplayAbility_TarrikEchoBase::IsTarrikReleaseContextValid() const
{
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()
		|| !IsValid(CurrentActorInfo->AvatarActor.Get())) { return false; }
	const UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!IsValid(ASC)) { return false; }
	const FSovGameplayTags& Sov = FSovGameplayTags::Get();
	const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
	return ASC->HasMatchingGameplayTag(Sov.Character_Player_Tarrik)
		&& !ASC->HasMatchingGameplayTag(Sov.Character_Player_Selene)
		&& !ASC->HasMatchingGameplayTag(Narrative.State_IsDead)
		&& !ASC->HasMatchingGameplayTag(Sov.State_Fatal)
		&& !ASC->HasMatchingGameplayTag(Narrative.State_Weapon_Equipping)
		&& !ASC->HasMatchingGameplayTag(Narrative.State_SequencerControlled)
		&& !ASC->HasMatchingGameplayTag(Narrative.State_Interacting)
		&& !ASC->HasMatchingGameplayTag(Narrative.State_Movement_Ragdoll)
		&& !ASC->HasMatchingGameplayTag(Sov.State_Guard_Broken)
		&& !ASC->HasMatchingGameplayTag(Sov.State_Poise_Broken)
		&& MeetsWeaponRequirement(CurrentSpecHandle, CurrentActorInfo);
}

void USovGameplayAbility_TarrikEchoBase::ArmTarrikPayload(float Delay, uint64 ExpectedActivation)
{
	if (!IsActive() || TarrikActivationSerial != ExpectedActivation
		|| !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority()) { return; }
	UWorld* World = GetWorld();
	if (!World || !FMath::IsFinite(Delay)) { FinishEchoAbility(true); return; }
	FTimerDelegate Callback = FTimerDelegate::CreateWeakLambda(this, [this, ExpectedActivation]()
	{
		if (!IsActive() || TarrikActivationSerial != ExpectedActivation) { return; }
		if (!IsTarrikReleaseContextValid()) { FinishEchoAbility(true); return; }
		ExecuteAutomaticTarrikPayload();
	});
	if (Delay <= KINDA_SMALL_NUMBER) { Callback.Execute(); }
	else { World->GetTimerManager().SetTimer(TarrikReleaseTimer, Callback, Delay, false); }
}

void USovGameplayAbility_TarrikEchoBase::BeginTarrikRecovery(float Delay, uint64 ExpectedActivation)
{
	if (!IsActive() || TarrikActivationSerial != ExpectedActivation) { return; }
	UWorld* World = GetWorld();
	if (!World) { FinishEchoAbility(false); return; }
	World->GetTimerManager().ClearTimer(TarrikReleaseTimer);
	FTimerDelegate Callback = FTimerDelegate::CreateWeakLambda(this, [this, ExpectedActivation]()
	{
		if (IsActive() && TarrikActivationSerial == ExpectedActivation) { FinishEchoAbility(false); }
	});
	if (Delay <= KINDA_SMALL_NUMBER) { Callback.Execute(); }
	else { World->GetTimerManager().SetTimer(TarrikRecoveryTimer, Callback, Delay, false); }
}
