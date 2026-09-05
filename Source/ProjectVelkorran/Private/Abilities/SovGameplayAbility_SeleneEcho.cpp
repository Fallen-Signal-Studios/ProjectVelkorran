// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_SeleneEcho.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Items/WeaponItem.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GameplayEffect.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "Weapons/NarrativeProjectile.h"

USovGameplayAbility_SeleneEchoBase::USovGameplayAbility_SeleneEchoBase()
{
	RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Selene;
	ActivationRequiredTags.AddTag(RequiredCharacterTag);
	ActivationBlockedTags.AddTag(FSovGameplayTags::Get().Character_Player_Tarrik);
	ActivationBlockedTags.AddTag(FNarrativeGameplayTags::Get().State_Weapon_Equipping);
}

bool USovGameplayAbility_SeleneEchoBase::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	const auto* Character = ActorInfo ? Cast<ANarrativeCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const auto* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!IsValid(Character) || !IsValid(ASC) || ASC->GetAvatarActor() != Character
		|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ANarrativeCharacter*>(Character)) != ASC) { return false; }
	if (bRequiresAllowedWeapon)
	{
		const auto AllowedAndWielded = [this](const UWeaponItem* Weapon)
		{
			return IsValid(Weapon) && Weapon->IsWielded() && AllowedWeaponClasses.ContainsByPredicate(
				[Weapon](const TSubclassOf<UWeaponItem>& Class) { return Class && Weapon->IsA(Class); });
		};
		if (WeaponGatePolicy == ESovEchoWeaponGatePolicy::AnyAllowedWielded)
		{
			if (!AllowedAndWielded(Character->GetWeapon(true)) && !AllowedAndWielded(Character->GetWeapon(false))) { return false; }
		}
		else
		{
			const auto* Weapon = Cast<UWeaponItem>(GetSourceObject(Handle, ActorInfo));
			if (!AllowedAndWielded(Weapon) || (Character->GetWeapon(true) != Weapon && Character->GetWeapon(false) != Weapon)) { return false; }
		}
	}
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void USovGameplayAbility_SeleneEchoBase::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	++NativePayloadEpoch; // Fence BEFORE cost/Blueprint/ASC delegates may end or reactivate this instance.
	NativeSourceWeapon = ActorInfo ? Cast<UWeaponItem>(GetSourceObject(Handle, ActorInfo)) : nullptr;
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}
void USovGameplayAbility_SeleneEchoBase::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	++NativePayloadEpoch;
	NativeSourceWeapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
FSovSelenePayloadContext USovGameplayAbility_SeleneEchoBase::MakeNativePayloadContext() const
{
	FSovSelenePayloadContext Context;
	Context.SourceASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
	Context.SourceAvatar = GetAvatarActorFromActorInfo();
	Context.SourceObject = NativeSourceWeapon.Get();
	Context.AbilityTag = EchoSpendTag;
	Context.Level = GetAbilityLevel();
	return Context;
}
bool USovGameplayAbility_SeleneEchoBase::CanExecuteNativePayload(uint32 Epoch) const
{
	if (Epoch != NativePayloadEpoch || !IsActive() || !CurrentActorInfo || !GetWorld()
		|| !HasRequiredPayloadConfiguration() || !SovSelenePayload::ValidSource(MakeNativePayloadContext())
		|| !MeetsWeaponRequirement(CurrentSpecHandle, CurrentActorInfo)) { return false; }
	const UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	const auto* Character = Cast<ANarrativeCharacter>(GetAvatarActorFromActorInfo());
	if (!Character || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ANarrativeCharacter*>(Character)) != ASC) { return false; }
	if (bRequiresAllowedWeapon && WeaponGatePolicy == ESovEchoWeaponGatePolicy::GrantingSourceMustBeWielded)
	{
		if (!NativeSourceWeapon.IsValid() || !NativeSourceWeapon->IsWielded()
			|| GetSourceObject(CurrentSpecHandle, CurrentActorInfo) != NativeSourceWeapon.Get()
			|| (Character->GetWeapon(true) != NativeSourceWeapon.Get() && Character->GetWeapon(false) != NativeSourceWeapon.Get())) { return false; }
	}
	else if (bRequiresAllowedWeapon)
	{
		const auto AllowedAndWielded = [this](const UWeaponItem* Weapon)
		{
			return IsValid(Weapon) && Weapon->IsWielded() && AllowedWeaponClasses.ContainsByPredicate(
				[Weapon](const TSubclassOf<UWeaponItem>& Class) { return Class && Weapon->IsA(Class); });
		};
		if (!AllowedAndWielded(Character->GetWeapon(true)) && !AllowedAndWielded(Character->GetWeapon(false))) { return false; }
	}
	const auto& Narrative = FNarrativeGameplayTags::Get();
	const auto& Tags = FSovGameplayTags::Get();
	FGameplayTagContainer Blocking;
	Blocking.AddTag(Narrative.State_Weapon_Equipping);
	Blocking.AddTag(Narrative.State_Interacting);
	Blocking.AddTag(Narrative.State_SequencerControlled);
	Blocking.AddTag(Narrative.State_Movement_Ragdoll);
	Blocking.AddTag(Tags.State_Poise_Broken);
	Blocking.AddTag(Tags.State_Guard_Broken);
	Blocking.AddTag(Tags.State_Status_Frozen);
	return !ASC->HasAnyMatchingGameplayTags(Blocking) && ASC->GetGameplayTagCount(Narrative.State_Busy) <= 1;
}
bool USovGameplayAbility_SeleneEchoBase::ContinueNativePayload(uint32 Epoch)
{
	if (CanExecuteNativePayload(Epoch)) { return true; }
	if (NativePayloadEpoch == Epoch && IsActive()) { FinishEchoAbility(true); }
	return false;
}
