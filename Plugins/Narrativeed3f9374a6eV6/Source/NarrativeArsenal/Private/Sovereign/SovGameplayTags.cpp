// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Sovereign/SovGameplayTags.h"

#include "GameplayTagsManager.h"

FSovGameplayTags FSovGameplayTags::GameplayTags;

void FSovGameplayTags::InitializeNativeTags()
{
	if (GameplayTags.Character_Player_Tarrik.IsValid()
		&& GameplayTags.Ability_Defense_Selene_Deflection.IsValid()
		&& GameplayTags.Ability_Weapon_Cinderline_PrimaryFire.IsValid()
		&& GameplayTags.Ability_NPC_ReformationDrone_Gunfire.IsValid()
		&& GameplayTags.Ability_NPC_ReformationDrone_RocketLauncher.IsValid()
		&& GameplayTags.Ability_NPC_ReformationDrone_SelfDestruct.IsValid()
		&& GameplayTags.Ability_Echo_Selene_StillpointGrenade.IsValid()
		&& GameplayTags.Status_Immunity_Burn.IsValid()
		&& GameplayTags.Status_Immunity_DeviceDisable.IsValid()
		&& GameplayTags.State_Deflecting.IsValid()
		&& GameplayTags.Echo_Source_PerfectDeflection.IsValid()
		&& GameplayTags.Echo_Source_WeakPointBreak.IsValid()
		&& GameplayTags.Echo_Source_CombatSustainPickup.IsValid())
	{
		return;
	}

	GameplayTags.AddAllTags(UGameplayTagsManager::Get());
}

void FSovGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(Character_Player, "Sov.Character.Player", "Parent identity tag for playable Sovereign characters.");
	AddTag(Character_Player_Tarrik, "Sov.Character.Player.Tarrik", "Tarrik Walcur player character.");
	AddTag(Character_Player_Selene, "Sov.Character.Player.Selene", "Selene Veyne player character.");
	AddTag(Character_Enemy_Boss, "Sov.Character.Enemy.Boss", "Boss identity tag used by reduced-reward combat loops.");

	AddTag(Ability_ActivateFail_Echo, "Sov.Ability.ActivateFail.Echo", "Ability activation failed its Echo threshold or spend check.");
	AddTag(Ability_Echo, "Sov.Ability.Echo", "Parent classification for every Echo-spending ability.");
	AddTag(Ability_Defense_Selene_Deflection, "Sov.Ability.Defense.Selene.Deflection", "Selene's short precision-deflection action.");
	AddTag(Ability_Weapon_Cinderline_PrimaryFire, "Sov.Ability.Weapon.Cinderline.PrimaryFire", "Ordinary Cinderline primary-fire classification used by Tarrik's Echo cadence loop.");
	AddTag(Ability_NPC_ReformationDrone_Gunfire, "Sov.Ability.NPC.ReformationDrone.Gunfire", "Standard gunfire attack used by Reformation drones.");
	AddTag(Ability_NPC_ReformationDrone_RocketLauncher, "Sov.Ability.NPC.ReformationDrone.RocketLauncher", "Rocket launcher attack used by Reformation drones.");
	AddTag(Ability_NPC_ReformationDrone_SelfDestruct, "Sov.Ability.NPC.ReformationDrone.SelfDestruct", "Pursuit and radial self-destruction attack used by explosive Reformation drones.");
	AddTag(Ability_Echo_Tarrik_CinderSlam, "Sov.Ability.Echo.Tarrik.CinderSlam", "Tarrik's sword-exclusive Cinder Slam Echo ability.");
	AddTag(Ability_Echo_Tarrik_VelkorransHunger, "Sov.Ability.Echo.Tarrik.VelkorransHunger", "Tarrik's sword-exclusive Velkorran's Hunger Echo ability.");
	AddTag(Ability_Echo_Tarrik_CinderStickyGrenade, "Sov.Ability.Echo.Tarrik.CinderStickyGrenade", "Tarrik's shared Cinder Sticky Grenade Echo ability.");
	AddTag(Ability_Echo_Tarrik_CinderJudgement, "Sov.Ability.Echo.Tarrik.CinderJudgement", "Tarrik's Cinderline-exclusive Cinder Judgement Echo ability.");
	AddTag(Ability_Echo_Tarrik_CinderlineRequiem, "Sov.Ability.Echo.Tarrik.CinderlineRequiem", "Tarrik's Cinderline-exclusive signature Echo ability.");
	AddTag(Ability_Echo_Selene_StillpointGrenade, "Sov.Ability.Echo.Selene.StillpointGrenade", "Selene's universal Stillpoint Grenade Echo ability.");
	AddTag(Ability_Echo_Selene_Dispatch, "Sov.Ability.Echo.Selene.Dispatch", "Selene's Verity throw-and-recall signature Echo ability.");
	AddTag(Ability_Echo_Selene_StaccatoZero, "Sov.Ability.Echo.Selene.StaccatoZero", "Selene's Staccato precision Echo ability.");
	AddTag(Ability_Echo_Selene_AxiomNullPulse, "Sov.Ability.Echo.Selene.AxiomNullPulse", "Selene's Axiom shield-disruption Echo ability.");
	AddTag(Ability_Echo_Selene_VeritysWake, "Sov.Ability.Echo.Selene.VeritysWake", "Selene's Verity frost-wave Echo ability.");
	AddTag(AnimSet_Ability_Tarrik_CinderSlam, "Narrative.Anim.AnimSets.Ability.Tarrik.CinderSlam", "Weapon-layer animation set for Cinder Slam.");
	AddTag(AnimSet_Ability_Tarrik_VelkorransHunger, "Narrative.Anim.AnimSets.Ability.Tarrik.VelkorransHunger", "Weapon-layer animation set for Velkorran's Hunger.");
	AddTag(AnimSet_Ability_Tarrik_CinderStickyGrenade, "Narrative.Anim.AnimSets.Ability.Tarrik.CinderStickyGrenade", "Weapon-layer animation set for Cinder Sticky Grenade.");
	AddTag(AnimSet_Ability_Tarrik_CinderJudgement, "Narrative.Anim.AnimSets.Ability.Tarrik.CinderJudgement", "Weapon-layer animation set for Cinder Judgement.");
	AddTag(AnimSet_Ability_Tarrik_CinderlineRequiem, "Narrative.Anim.AnimSets.Ability.Tarrik.CinderlineRequiem", "Weapon-layer animation set for Cinderline Requiem.");
	AddTag(AnimSet_Ability_Selene_StillpointGrenade, "Narrative.Anim.AnimSets.Ability.Selene.StillpointGrenade", "Weapon-layer animation set for Stillpoint Grenade.");
	AddTag(AnimSet_Ability_Selene_Dispatch, "Narrative.Anim.AnimSets.Ability.Selene.Dispatch", "Weapon-layer animation set for Dispatch.");
	AddTag(AnimSet_Ability_Selene_StaccatoZero, "Narrative.Anim.AnimSets.Ability.Selene.StaccatoZero", "Weapon-layer animation set for Staccato Zero.");
	AddTag(AnimSet_Ability_Selene_AxiomNullPulse, "Narrative.Anim.AnimSets.Ability.Selene.AxiomNullPulse", "Weapon-layer animation set for Axiom Null Pulse.");
	AddTag(AnimSet_Ability_Selene_VeritysWake, "Narrative.Anim.AnimSets.Ability.Selene.VeritysWake", "Weapon-layer animation set for Verity's Wake.");

	AddTag(State_Invulnerable, "Sov.State.Invulnerable", "Damage immunity shared by explicit invulnerability states.");
	AddTag(State_Damage_Immune, "Sov.State.Damage.Immune", "Target is immune to ordinary damage execution.");
	AddTag(State_Fatal, "Sov.State.Fatal", "Health reached zero and normal combat input is disabled.");
	AddTag(State_EchoAbility_Active, "Sov.State.EchoAbility.Active", "A committed character Echo ability currently owns the Echo-action lane.");
	AddTag(State_Guarding, "Sov.State.Guarding", "A frontal guard plane is active.");
	AddTag(State_PerfectGuard, "Sov.State.PerfectGuard", "The perfect-defense timing window is active.");
	AddTag(State_Deflecting, "Sov.State.Deflecting", "Selene's brief precision-deflection window is active.");
	AddTag(State_Guard_CounterWindow, "Sov.State.Guard.CounterWindow", "A perfect guard opened a counter opportunity.");
	AddTag(State_Guard_Broken, "Sov.State.Guard.Broken", "Guard stamina was exhausted by an impact.");
	AddTag(State_Shield_Broken, "Sov.State.Shield.Broken", "Shield is currently depleted.");
	AddTag(State_Shield_RechargeBlocked, "Sov.State.Shield.RechargeBlocked", "Shield recharge is paused.");
	AddTag(State_Poise_Pressured, "Sov.State.Poise.Pressured", "Poise is at or below its warning threshold.");
	AddTag(State_Poise_Broken, "Sov.State.Poise.Broken", "Poise is broken.");
	AddTag(State_Poise_Recovering, "Sov.State.Poise.Recovering", "Post-break re-break protection is active.");
	AddTag(State_Poise_RegenBlocked, "Sov.State.Poise.RegenBlocked", "Poise regeneration is paused.");
	AddTag(State_Poise_SuperArmor, "Sov.State.Poise.SuperArmor", "Poise cannot break during an authored super-armor window.");
	AddTag(State_Status_Chilled, "Sov.State.Status.Chilled", "Movement and control resistance are reduced by cryothermal pressure.");
	AddTag(State_Status_Frozen, "Sov.State.Status.Frozen", "The target is held by an authored hard-Freeze effect.");
	AddTag(State_Status_DeviceDisabled, "Sov.State.Status.DeviceDisabled", "Eligible combat systems are disabled by Disruption.");

	AddTag(Damage_BypassShield, "Sov.Damage.BypassShield", "All resolved health damage bypasses Shield.");
	AddTag(Damage_BypassShield_Partial, "Sov.Damage.BypassShield.Partial", "Uses the authored partial Shield bypass ratio.");
	AddTag(Damage_BypassGuard, "Sov.Damage.BypassGuard", "The hit ignores an active guard plane.");
	AddTag(Damage_BypassDeflection, "Sov.Damage.BypassDeflection", "The hit cannot be intercepted by Selene's Deflection.");
	AddTag(Damage_AlreadyResolved, "Sov.Damage.Policy.AlreadyResolved", "The authored magnitude bypasses source stats and mitigation.");
	AddTag(Damage_Fatal, "Sov.Damage.Policy.Fatal", "Fatal policy bypasses invulnerability, Guard, Deflection, and Shield.");
	AddTag(Damage_IgnoreArmor, "Sov.Damage.IgnoreArmor", "The hit bypasses Armor mitigation.");
	AddTag(Damage_IgnoreResistance, "Sov.Damage.IgnoreResistance", "The hit bypasses channel resistance.");
	AddTag(Damage_AllowFriendlyFire, "Sov.Damage.AllowFriendlyFire", "The hit may damage a friendly target.");
	AddTag(Damage_RestartShieldRecharge, "Sov.Damage.RestartShieldRecharge", "The hit restarts Shield recharge even if no Shield remains.");
	AddTag(Damage_Heavy, "Sov.Damage.Heavy", "Legacy heavy attack classification.");
	AddTag(Damage_Unblockable, "Sov.Damage.Unblockable", "Legacy unblockable attack classification.");
	AddTag(Damage_GuardClass_Standard, "Sov.Damage.GuardClass.Standard", "Ordinary guardable attack classification.");
	AddTag(Damage_GuardClass_Heavy, "Sov.Damage.GuardClass.Heavy", "Heavy attacks require perfect defense, evade, or interruption.");
	AddTag(Damage_GuardClass_Unblockable, "Sov.Damage.GuardClass.Unblockable", "The hit cannot be guarded.");
	AddTag(Damage_Source_GuardCounter, "Sov.Damage.Source.GuardCounter", "Damage dealt by Tarrik's active guard-counter branch.");
	AddTag(Damage_Result_Guarded, "Sov.Damage.Result.Guarded", "Callback-local result tag for a hit successfully intercepted by Guard.");
	AddTag(Damage_Result_Deflected, "Sov.Damage.Result.Deflected", "Callback-local result tag for a hit successfully intercepted by Deflection.");
	AddTag(Damage_Poise, "Sov.Damage.Poise", "The hit carries explicit Poise pressure.");
	AddTag(Damage_Channel_Kinetic, "Sov.Damage.Channel.Kinetic", "Projectile, impact, or blunt-force damage.");
	AddTag(Damage_Channel_Edge, "Sov.Damage.Channel.Edge", "Blade or cutting-field damage.");
	AddTag(Damage_Channel_Thermal, "Sov.Damage.Channel.Thermal", "Heat and thermal damage.");
	AddTag(Damage_Channel_Echo, "Sov.Damage.Channel.Echo", "Echo or King-era system damage.");
	AddTag(Damage_Channel_Disruption, "Sov.Damage.Channel.Disruption", "Equipment and network disruption.");
	AddTag(Damage_Channel_Corruption, "Sov.Damage.Channel.Corruption", "Eclipse corruption pressure.");
	AddTag(Damage_Channel_Environmental, "Sov.Damage.Channel.Environmental", "Authored environmental damage.");
	AddTag(Damage_Immunity_All, "Sov.Damage.Immunity.All", "Target ignores all Sovereign damage channels.");
	AddTag(Damage_Immunity_Kinetic, "Sov.Damage.Immunity.Kinetic", "Target ignores Kinetic damage.");
	AddTag(Damage_Immunity_Edge, "Sov.Damage.Immunity.Edge", "Target ignores Edge damage.");
	AddTag(Damage_Immunity_Thermal, "Sov.Damage.Immunity.Thermal", "Target ignores Thermal damage.");
	AddTag(Damage_Immunity_Echo, "Sov.Damage.Immunity.Echo", "Target ignores Echo damage.");
	AddTag(Damage_Immunity_Disruption, "Sov.Damage.Immunity.Disruption", "Target ignores Disruption damage.");
	AddTag(Damage_Immunity_Corruption, "Sov.Damage.Immunity.Corruption", "Target ignores Corruption damage.");
	AddTag(Damage_Immunity_Environmental, "Sov.Damage.Immunity.Environmental", "Target ignores Environmental damage.");
	AddTag(Status_Apply, "Sov.Status.Apply", "Parent tag for requested status applications carried by damage specs.");
	AddTag(Status_Apply_Burn, "Sov.Status.Apply.Burn", "A resolved hit requests the project-owned Burn status.");
	AddTag(Status_Apply_Chill, "Sov.Status.Apply.Chill", "A resolved hit requests the project-owned Chill status.");
	AddTag(Status_Apply_Freeze, "Sov.Status.Apply.Freeze", "A resolved hit requests the project-owned hard-Freeze status.");
	AddTag(Status_Apply_DeviceDisabled, "Sov.Status.Apply.DeviceDisabled", "A resolved hit requests a device-disable status on eligible targets.");
	AddTag(Status_Immunity, "Sov.Status.Immunity", "Parent tag for project-owned status immunities.");
	AddTag(Status_Immunity_Burn, "Sov.Status.Immunity.Burn", "The target rejects project-owned Burn effects.");
	AddTag(Status_Immunity_Freeze, "Sov.Status.Immunity.Freeze", "The target rejects hard Freeze and should receive its authored fallback.");
	AddTag(Status_Immunity_DeviceDisable, "Sov.Status.Immunity.DeviceDisable", "The target rejects device-disable effects.");

	AddTag(SetByCaller_Damage_AbilityScalar, "Sov.SetByCaller.Damage.AbilityScalar", "Ability-specific damage scalar.");
	AddTag(SetByCaller_Damage_SourceModifier, "Sov.SetByCaller.Damage.SourceModifier", "Authored source damage multiplier.");
	AddTag(SetByCaller_Damage_HitZoneModifier, "Sov.SetByCaller.Damage.HitZoneModifier", "Authored deterministic hit-zone multiplier.");
	AddTag(SetByCaller_Damage_DifficultyScalar, "Sov.SetByCaller.Damage.DifficultyScalar", "Difficulty damage multiplier.");
	AddTag(SetByCaller_Damage_MitigationMultiplier, "Sov.SetByCaller.Damage.MitigationMultiplier", "Additional bounded mitigation multiplier.");
	AddTag(SetByCaller_Damage_ShieldBypassRatio, "Sov.SetByCaller.Damage.ShieldBypassRatio", "Fraction of damage routed directly to Health.");
	AddTag(SetByCaller_Damage_ShieldCoefficient, "Sov.SetByCaller.Damage.ShieldCoefficient", "Shield damage coefficient.");
	AddTag(SetByCaller_Damage_HealthCoefficient, "Sov.SetByCaller.Damage.HealthCoefficient", "Health damage coefficient.");
	AddTag(SetByCaller_Damage_PoiseDamage, "Sov.SetByCaller.Damage.PoiseDamage", "Explicit post-formula Poise damage.");
	AddTag(SetByCaller_Damage_PoiseCoefficient, "Sov.SetByCaller.Damage.PoiseCoefficient", "Poise pressure derived from resolved damage.");
	AddTag(SetByCaller_Damage_GuardStaminaDamage, "Sov.SetByCaller.Damage.GuardStaminaDamage", "Explicit guard impact Stamina cost.");
	AddTag(SetByCaller_Status_Magnitude, "Sov.SetByCaller.Status.Magnitude", "Status-system payload magnitude.");

	AddTag(Event_Character_Ready, "Sov.Event.Character.Ready", "Character initialization completed idempotently.");
	AddTag(Event_Damage_Resolved, "Sov.Event.Combat.DamageResolved", "Damage finished authoritative routing.");
	AddTag(Event_Shield_Broken, "Sov.Event.Shield.Broken", "A resolved hit depleted Shield.");
	AddTag(Event_Poise_Broken, "Sov.Event.Poise.Broken", "A resolved hit depleted Poise.");
	AddTag(Event_Guard_Blocked, "Sov.Event.Guard.Blocked", "An active guard reduced an incoming hit.");
	AddTag(Event_Guard_Perfect, "Sov.Event.Guard.Perfect", "An incoming hit landed during the perfect-defense window.");
	AddTag(Event_Guard_Broken, "Sov.Event.Guard.Broken", "Guard Stamina was exhausted.");
	AddTag(Event_Guard_CounterWindowOpened, "Sov.Event.Guard.CounterWindowOpened", "Perfect defense opened a counter window.");
	AddTag(Event_Guard_CounterConsumed, "Sov.Event.Guard.CounterConsumed", "A landed guard counter consumed the counter window.");
	AddTag(Event_Deflection_Perfect, "Sov.Event.Deflection.Perfect", "Selene intercepted one eligible hit during her Deflection window.");
	AddTag(Event_Echo_Gained_PerfectGuard, "Sov.Event.Echo.Gained.PerfectGuard", "Echo was granted for a perfect guard.");
	AddTag(Event_Status_ApplicationRequested, "Sov.Event.Status.ApplicationRequested", "A resolved hit requested project-owned status application.");

	AddTag(Echo_Source_PerfectGuard, "Sov.Echo.Source.PerfectGuard", "Echo source for Tarrik perfect guard.");
	AddTag(Echo_Source_PerfectDeflection, "Sov.Echo.Source.PerfectDeflection", "Echo source for Selene's correctly timed Deflection.");
	AddTag(Echo_Source_WeakPointBreak, "Sov.Echo.Source.WeakPointBreak", "Echo source for breaking an authored weak point.");
	AddTag(Echo_Source_GuardPressure, "Sov.Echo.Source.GuardPressure", "Combat activity caused by intentional guard pressure.");
	AddTag(Echo_Source_GuardCounter, "Sov.Echo.Source.GuardCounter", "Echo source for a landed Tarrik guard counter.");
	AddTag(Echo_Source_CinderlineCadence, "Sov.Echo.Source.CinderlineCadence", "Echo source for completing Tarrik's Cinderline firing cadence.");
	AddTag(Echo_Source_CinderlinePrecisionKill, "Sov.Echo.Source.CinderlinePrecisionKill", "Echo source for a Cinderline precision kill.");
	AddTag(Echo_Source_CombatSustainPickup, "Sov.Echo.Source.CombatSustainPickup", "Echo source for collecting a transient combat-sustain mote.");
}

void FSovGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TagName),
		FString(TEXT("(Native) ")) + FString(TagComment));
}
