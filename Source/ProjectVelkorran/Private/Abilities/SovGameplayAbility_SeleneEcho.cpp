// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_SeleneEcho.h"

#include "GameplayEffect.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "Weapons/NarrativeProjectile.h"

USovGameplayAbility_SeleneEchoBase::USovGameplayAbility_SeleneEchoBase()
{
	RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Selene;
}

USovGameplayAbility_SeleneStillpointGrenade::USovGameplayAbility_SeleneStillpointGrenade()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 35.0f;
	EchoCost = 35.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_StillpointGrenade;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_StillpointGrenade;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Universal;
	WeaponGatePolicy = ESovEchoWeaponGatePolicy::AnyAllowedWielded;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "StillpointGrenadeName", "Stillpoint Grenade");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"StillpointGrenadeDescription",
		"Open a cryothermal Stasis field that locks down standard enemies and damages them while Frozen; resistant targets are Chilled instead.");
}

bool USovGameplayAbility_SeleneStillpointGrenade::HasRequiredPayloadConfiguration() const
{
	return GrenadeClass.Get()
		&& ChillEffectClass.Get()
		&& FreezeEffectClass.Get()
		&& FrozenDamageOverTimeEffectClass.Get()
		&& ResistantTargetDamageOverTimeEffectClass.Get()
		&& StasisRadius > KINDA_SMALL_NUMBER
		&& StasisDuration > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_SeleneDispatch::USovGameplayAbility_SeleneDispatch()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_Dispatch;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_Dispatch;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Verity;
	// Dispatch is Selene's signature and summons Verity even while a firearm is active.
	bRequiresAllowedWeapon = false;
	MaximumActiveDuration = 7.0f;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "DispatchName", "Dispatch");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"DispatchDescription",
		"Cast Verity through a steerable arc, then recall it on command or at the outbound limit; enemies can be struck once on each leg of the path.");
}

bool USovGameplayAbility_SeleneDispatch::HasRequiredPayloadConfiguration() const
{
	return ReturningVerityClass.Get()
		&& OutboundDamageEffectClass.Get()
		&& ReturnDamageEffectClass.Get()
		&& MaximumOutboundDuration > KINDA_SMALL_NUMBER
		&& MaximumOutboundDistance > KINDA_SMALL_NUMBER
		&& OutboundSpeed > KINDA_SMALL_NUMBER
		&& ReturnSpeed > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_SeleneStaccatoZero::USovGameplayAbility_SeleneStaccatoZero()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_StaccatoZero;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_StaccatoZero;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Staccato;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "StaccatoZeroName", "Staccato Zero");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"StaccatoZeroDescription",
		"Fire one overdriven precision shot with extreme direct damage and a deterministic Freeze against a valid target.");
}

bool USovGameplayAbility_SeleneStaccatoZero::HasRequiredPayloadConfiguration() const
{
	return EmpoweredShotDamageEffectClass.Get()
		&& FreezeEffectClass.Get()
		&& ResistantTargetChillEffectClass.Get()
		&& MaximumRange > KINDA_SMALL_NUMBER
		&& DamageMultiplier > KINDA_SMALL_NUMBER;
}

USovGameplayAbility_SeleneAxiomNullPulse::USovGameplayAbility_SeleneAxiomNullPulse()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_AxiomNullPulse;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_AxiomNullPulse;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Axiom;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "AxiomNullPulseName", "Axiom Null Pulse");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"AxiomNullPulseDescription",
		"Charge Axiom and release a directed EMP pulse that collapses Shields, suppresses recharge, and disables eligible combat systems.");
}

bool USovGameplayAbility_SeleneAxiomNullPulse::HasRequiredPayloadConfiguration() const
{
	return ShieldDisruptionDamageEffectClass.Get()
		&& ShieldRechargeBlockEffectClass.Get()
		&& FullChargeDuration > KINDA_SMALL_NUMBER
		&& MinimumPulseRange > KINDA_SMALL_NUMBER
		&& MaximumPulseRange >= MinimumPulseRange
		&& MaximumPulseHalfAngleDegrees >= MinimumPulseHalfAngleDegrees
		&& MaximumShieldSuppressionDuration >= MinimumShieldSuppressionDuration;
}

USovGameplayAbility_SeleneVeritysWake::USovGameplayAbility_SeleneVeritysWake()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 30.0f;
	EchoCost = 30.0f;
	EchoSpendTag = Tags.Ability_Echo_Selene_VeritysWake;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Selene_VeritysWake;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovSeleneEchoWeaponFamily::Verity;
	AbilityDisplayName = NSLOCTEXT("SovSeleneEcho", "VeritysWakeName", "Verity's Wake");
	AbilityDescription = NSLOCTEXT(
		"SovSeleneEcho",
		"VeritysWakeDescription",
		"Drive a fast frost wave down a combat lane, dealing Echo damage and Chill; centerline or already-Chilled targets Freeze and suffer Frost damage over time.");
}

bool USovGameplayAbility_SeleneVeritysWake::HasRequiredPayloadConfiguration() const
{
	return WaveClass.Get()
		&& WaveDamageEffectClass.Get()
		&& ChillEffectClass.Get()
		&& FreezeEffectClass.Get()
		&& FrostDamageOverTimeEffectClass.Get()
		&& WaveRange > KINDA_SMALL_NUMBER
		&& WaveWidth > KINDA_SMALL_NUMBER;
}
