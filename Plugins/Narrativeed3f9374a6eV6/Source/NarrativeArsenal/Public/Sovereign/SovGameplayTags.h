// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UGameplayTagsManager;

/**
 * Native gameplay tags that form Sovereign Call's C++ contracts.
 *
 * These live in the frozen NarrativeArsenal fork so Narrative damage,
 * attributes, and the ProjectVelkorran game module can share one registrar
 * without introducing a circular module dependency.
 */
struct NARRATIVEARSENAL_API FSovGameplayTags
{
public:
	static const FSovGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeTags();

	FGameplayTag Character_Player;
	FGameplayTag Character_Player_Tarrik;
	FGameplayTag Character_Player_Selene;
	FGameplayTag Character_Enemy_Boss;

	FGameplayTag Ability_ActivateFail_Echo;
	FGameplayTag Ability_Echo;
	FGameplayTag Ability_Defense_Selene_Deflection;
	FGameplayTag Ability_Weapon_Cinderline_PrimaryFire;
	FGameplayTag Ability_NPC_ReformationDrone_Gunfire;
	FGameplayTag Ability_NPC_ReformationDrone_RocketLauncher;
	FGameplayTag Ability_NPC_ReformationDrone_SelfDestruct;
	FGameplayTag Ability_Echo_Tarrik_CinderSlam;
	FGameplayTag Ability_Echo_Tarrik_VelkorransHunger;
	FGameplayTag Ability_Echo_Tarrik_CinderStickyGrenade;
	FGameplayTag Ability_Echo_Tarrik_CinderJudgement;
	FGameplayTag Ability_Echo_Tarrik_CinderlineRequiem;
	FGameplayTag Ability_Echo_Selene_StillpointGrenade;
	FGameplayTag Ability_Echo_Selene_Dispatch;
	FGameplayTag Ability_Echo_Selene_StaccatoZero;
	FGameplayTag Ability_Echo_Selene_AxiomNullPulse;
	FGameplayTag Ability_Echo_Selene_VeritysWake;
	FGameplayTag AnimSet_Ability_Tarrik_CinderSlam;
	FGameplayTag AnimSet_Ability_Tarrik_VelkorransHunger;
	FGameplayTag AnimSet_Ability_Tarrik_CinderStickyGrenade;
	FGameplayTag AnimSet_Ability_Tarrik_CinderJudgement;
	FGameplayTag AnimSet_Ability_Tarrik_CinderlineRequiem;
	FGameplayTag AnimSet_Ability_Selene_StillpointGrenade;
	FGameplayTag AnimSet_Ability_Selene_Dispatch;
	FGameplayTag AnimSet_Ability_Selene_StaccatoZero;
	FGameplayTag AnimSet_Ability_Selene_AxiomNullPulse;
	FGameplayTag AnimSet_Ability_Selene_VeritysWake;

	FGameplayTag State_Invulnerable;
	FGameplayTag State_Damage_Immune;
	FGameplayTag State_Fatal;
	FGameplayTag State_EchoAbility_Active;
	FGameplayTag State_Guarding;
	FGameplayTag State_PerfectGuard;
	FGameplayTag State_Deflecting;
	FGameplayTag State_Guard_CounterWindow;
	FGameplayTag State_Guard_Broken;
	FGameplayTag State_Shield_Broken;
	FGameplayTag State_Shield_RechargeBlocked;
	FGameplayTag State_Poise_Pressured;
	FGameplayTag State_Poise_Broken;
	FGameplayTag State_Poise_Recovering;
	FGameplayTag State_Poise_RegenBlocked;
	FGameplayTag State_Poise_SuperArmor;
	FGameplayTag State_Status_Chilled;
	FGameplayTag State_Status_Frozen;
	FGameplayTag State_Status_DeviceDisabled;

	FGameplayTag Damage_BypassShield;
	FGameplayTag Damage_BypassShield_Partial;
	FGameplayTag Damage_BypassGuard;
	FGameplayTag Damage_BypassDeflection;
	FGameplayTag Damage_AlreadyResolved;
	FGameplayTag Damage_Fatal;
	FGameplayTag Damage_IgnoreArmor;
	FGameplayTag Damage_IgnoreResistance;
	FGameplayTag Damage_AllowFriendlyFire;
	FGameplayTag Damage_RestartShieldRecharge;
	FGameplayTag Damage_Heavy;
	FGameplayTag Damage_Unblockable;
	FGameplayTag Damage_GuardClass_Standard;
	FGameplayTag Damage_GuardClass_Heavy;
	FGameplayTag Damage_GuardClass_Unblockable;
	FGameplayTag Damage_Source_GuardCounter;
	FGameplayTag Damage_Result_Guarded;
	FGameplayTag Damage_Result_Deflected;
	FGameplayTag Damage_Poise;
	FGameplayTag Damage_Channel_Kinetic;
	FGameplayTag Damage_Channel_Edge;
	FGameplayTag Damage_Channel_Thermal;
	FGameplayTag Damage_Channel_Echo;
	FGameplayTag Damage_Channel_Disruption;
	FGameplayTag Damage_Channel_Corruption;
	FGameplayTag Damage_Channel_Environmental;
	FGameplayTag Damage_Immunity_All;
	FGameplayTag Damage_Immunity_Kinetic;
	FGameplayTag Damage_Immunity_Edge;
	FGameplayTag Damage_Immunity_Thermal;
	FGameplayTag Damage_Immunity_Echo;
	FGameplayTag Damage_Immunity_Disruption;
	FGameplayTag Damage_Immunity_Corruption;
	FGameplayTag Damage_Immunity_Environmental;
	FGameplayTag Status_Apply;
	FGameplayTag Status_Apply_Burn;
	FGameplayTag Status_Apply_Chill;
	FGameplayTag Status_Apply_Freeze;
	FGameplayTag Status_Apply_DeviceDisabled;
	FGameplayTag Status_Immunity;
	FGameplayTag Status_Immunity_Burn;
	FGameplayTag Status_Immunity_Freeze;
	FGameplayTag Status_Immunity_DeviceDisable;

	FGameplayTag SetByCaller_Damage_AbilityScalar;
	FGameplayTag SetByCaller_Damage_SourceModifier;
	FGameplayTag SetByCaller_Damage_HitZoneModifier;
	FGameplayTag SetByCaller_Damage_DifficultyScalar;
	FGameplayTag SetByCaller_Damage_MitigationMultiplier;
	FGameplayTag SetByCaller_Damage_ShieldBypassRatio;
	FGameplayTag SetByCaller_Damage_ShieldCoefficient;
	FGameplayTag SetByCaller_Damage_HealthCoefficient;
	FGameplayTag SetByCaller_Damage_PoiseDamage;
	FGameplayTag SetByCaller_Damage_PoiseCoefficient;
	FGameplayTag SetByCaller_Damage_GuardStaminaDamage;
	FGameplayTag SetByCaller_Status_Magnitude;

	FGameplayTag Event_Character_Ready;
	FGameplayTag Event_Damage_Resolved;
	FGameplayTag Event_Shield_Broken;
	FGameplayTag Event_Poise_Broken;
	FGameplayTag Event_Guard_Blocked;
	FGameplayTag Event_Guard_Perfect;
	FGameplayTag Event_Guard_Broken;
	FGameplayTag Event_Guard_CounterWindowOpened;
	FGameplayTag Event_Guard_CounterConsumed;
	FGameplayTag Event_Deflection_Perfect;
	FGameplayTag Event_Echo_Gained_PerfectGuard;
	FGameplayTag Event_Status_ApplicationRequested;

	FGameplayTag Echo_Source_PerfectGuard;
	FGameplayTag Echo_Source_PerfectDeflection;
	FGameplayTag Echo_Source_WeakPointBreak;
	FGameplayTag Echo_Source_GuardPressure;
	FGameplayTag Echo_Source_GuardCounter;
	FGameplayTag Echo_Source_CinderlineCadence;
	FGameplayTag Echo_Source_CinderlinePrecisionKill;
	FGameplayTag Echo_Source_CombatSustainPickup;

private:
	void AddAllTags(UGameplayTagsManager& Manager);
	static void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

	static FSovGameplayTags GameplayTags;
};
