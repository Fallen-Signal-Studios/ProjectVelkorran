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
#include "Effects/SovGameplayEffect_SeleneControl.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovSeleneConfiguration, Log, All);

bool USovGameplayAbility_SeleneEchoBase::ValidateNativeEffectOverrides(FString& OutError) const
{
	OutError.Reset();
	const TPair<FName, UClass*> RetiredFields[] = {
		{TEXT("DetonationDamageEffectClass"), USovGameplayEffect_SeleneDamage::StaticClass()},
		{TEXT("OutboundDamageEffectClass"), USovGameplayEffect_SeleneDamage::StaticClass()},
		{TEXT("ReturnDamageEffectClass"), USovGameplayEffect_SeleneDamage::StaticClass()},
		{TEXT("FrozenShatterEffectClass"), USovGameplayEffect_SeleneDamage::StaticClass()},
		{TEXT("EmpoweredShotDamageEffectClass"), USovGameplayEffect_SeleneDamage::StaticClass()},
		{TEXT("WaveDamageEffectClass"), USovGameplayEffect_SeleneDamage::StaticClass()},
		{TEXT("ChillEffectClass"), USovGameplayEffect_SeleneControl::StaticClass()},
		{TEXT("FreezeEffectClass"), USovGameplayEffect_SeleneControl::StaticClass()},
		{TEXT("ResistantTargetChillEffectClass"), USovGameplayEffect_SeleneControl::StaticClass()},
		{TEXT("FrozenDamageOverTimeEffectClass"), USovGameplayEffect_SeleneFrozenDOT::StaticClass()},
		{TEXT("ResistantTargetDamageOverTimeEffectClass"), USovGameplayEffect_SeleneFrostDOT::StaticClass()},
		{TEXT("FrostDamageOverTimeEffectClass"), USovGameplayEffect_SeleneFrostDOT::StaticClass()}
	};
	for (const auto& Entry : RetiredFields)
	{
		const FClassProperty* Property = FindFProperty<FClassProperty>(GetClass(), Entry.Key);
		const UObject* Value = Property ? Property->GetObjectPropertyValue_InContainer(this) : nullptr;
		if (Value && Value != Entry.Value)
		{
			OutError += FString::Printf(TEXT("%s uses retired effect override %s. Clear it or restore %s; tune the native damage/duration fields and presentation events. "),
				*Entry.Key.ToString(), *Value->GetPathName(), *Entry.Value->GetPathName());
		}
	}
	return OutError.IsEmpty();
}

bool USovGameplayAbility_SeleneEchoBase::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	FString Error;
	if (!ValidateNativeEffectOverrides(Error))
	{
		UE_LOG(LogSovSeleneConfiguration, Error, TEXT("%s: %s"), *GetPathName(), *Error);
		if (OptionalRelevantTags) { OptionalRelevantTags->AddTag(FNarrativeGameplayTags::Get().Ability_ActivateFail_TagsBlocked); }
		return false;
	}
	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

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
	if (!IsEndAbilityValid(Handle, ActorInfo)) { return; }
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
	if (Epoch != NativePayloadEpoch || !IsCurrentEchoExecutionValid() || !CurrentActorInfo || !GetWorld()
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
