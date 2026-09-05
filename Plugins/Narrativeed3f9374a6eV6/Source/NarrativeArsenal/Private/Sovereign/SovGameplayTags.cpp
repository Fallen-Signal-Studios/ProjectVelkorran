// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Sovereign/SovGameplayTags.h"

#include "GameplayTagsManager.h"

FSovGameplayTags FSovGameplayTags::GameplayTags;

void FSovGameplayTags::InitializeNativeTags()
{
	if (GameplayTags.Character_Player_Tarrik.IsValid()
		&& GameplayTags.Ability.IsValid()
		&& GameplayTags.Ability_Defense_Selene_Deflection.IsValid()
		&& GameplayTags.Ability_Weapon_Cinderline_PrimaryFire.IsValid()
		&& GameplayTags.Ability_NPC_ReformationDrone_Gunfire.IsValid()
		&& GameplayTags.Ability_NPC_ReformationDrone_RocketLauncher.IsValid()
		&& GameplayTags.Ability_NPC_ReformationDrone_SelfDestruct.IsValid()
		&& GameplayTags.Ability_NPC_DominionHound_Bite.IsValid()
		&& GameplayTags.Ability_NPC_DominionHound_HornCharge.IsValid()
		&& GameplayTags.Ability_NPC_DominionHound_Pounce.IsValid()
		&& GameplayTags.Ability_NPC_DominionHandler_CommandHound.IsValid()
		&& GameplayTags.Ability_Echo_Selene_StillpointGrenade.IsValid()
		&& GameplayTags.Status_Apply_Corruption.IsValid()
		&& GameplayTags.Status_Immunity_All.IsValid()
		&& GameplayTags.Status_Immunity_Burn.IsValid()
		&& GameplayTags.Status_Immunity_DeviceDisable.IsValid()
		&& GameplayTags.Status_Cleanse_All.IsValid()
		&& GameplayTags.State_Deflecting.IsValid()
		&& GameplayTags.State_Status_Exposed.IsValid()
		&& GameplayTags.State_Status_Corrupted.IsValid()
		&& GameplayTags.State_Corruption_OverwriteRisk.IsValid()
		&& GameplayTags.State_CommandLink_Active.IsValid()
		&& GameplayTags.State_CommandLink_Severed.IsValid()
		&& GameplayTags.State_CommandLink_HoundChargeAuthorized.IsValid()
		&& GameplayTags.Echo_Source_PerfectDeflection.IsValid()
		&& GameplayTags.Echo_Source_WeakPointBreak.IsValid()
		&& GameplayTags.Echo_Source_CommandLinkSever.IsValid()
		&& GameplayTags.Echo_Source_CombatSustainPickup.IsValid()
		&& GameplayTags.Echo_Source_ExposureKill.IsValid()
		&& GameplayTags.Event_Status_Applied.IsValid()
		&& GameplayTags.Event_Corruption_BandChanged.IsValid()
		&& GameplayTags.SetByCaller_Status_Duration.IsValid())
	{
		return;
	}

	GameplayTags.AddAllTags(UGameplayTagsManager::Get());
}

void FSovGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(Input_Evade, "Narrative.Input.Evade", "Semantic evade input, independent of physical device bindings.");
	AddTag(Input_FieldRecovery, "Narrative.Input.FieldRecovery", "Semantic field recovery input.");
	AddTag(Ability_FieldRecovery, "Sov.Ability.FieldRecovery", "Interruptible field recovery charge use.");
	AddTag(State_FieldRecovery, "Sov.State.FieldRecovery", "Native field recovery in progress.");
	AddTag(Input_ThreatFocus, "Narrative.Input.ThreatFocus", "Toggle native threat focus.");
	AddTag(Input_CycleTargetLeft, "Narrative.Input.CycleTargetLeft", "Cycle to the visible threat on the left.");
	AddTag(Input_CycleTargetRight, "Narrative.Input.CycleTargetRight", "Cycle to the visible threat on the right.");
	AddTag(Input_AbilityModifier, "Narrative.Input.AbilityModifier", "Semantic alternate-ability modifier.");
	AddTag(Ability_Evade, "Sov.Ability.Evade", "Native campaign runtime contract.");
	AddTag(State_Evading, "Sov.State.Evading", "Native campaign runtime contract.");
	AddTag(State_Exertion_Exhausted, "Sov.State.Exertion.Exhausted", "Native campaign runtime contract.");
	AddTag(Ability_Finisher, "Sov.Ability.Finisher", "Native campaign runtime contract.");
	AddTag(Input_Finisher, "Sov.Input.Finisher", "Native campaign runtime contract.");
	AddTag(State_Finisher_Active, "Sov.State.Finisher.Active", "Native campaign runtime contract.");
	AddTag(State_Finisher_Target, "Sov.State.Finisher.Target", "Native campaign runtime contract.");
	AddTag(State_InterruptProtected, "Sov.State.InterruptProtected", "Native campaign runtime contract.");
	AddTag(Event_Finisher_PhaseResolved, "Sov.Event.Finisher.PhaseResolved", "Native campaign runtime contract.");
	AddTag(Event_Finisher_Strike, "Sov.Event.Finisher.Strike", "Native campaign runtime contract.");
	AddTag(State_Resonance_Available, "Sov.State.Resonance.Available", "Native campaign runtime contract.");
	AddTag(State_Resonance_Committed, "Sov.State.Resonance.Committed", "Native campaign runtime contract.");
	AddTag(Event_Resonance_Setup_PerfectGuard, "Sov.Event.Resonance.Setup.PerfectGuard", "Native campaign runtime contract.");
	AddTag(Event_Resonance_Setup_CommandExposed, "Sov.Event.Resonance.Setup.CommandExposed", "Native campaign runtime contract.");
	AddTag(Event_Resonance_Setup_TerminalProtection, "Sov.Event.Resonance.Setup.TerminalProtection", "Native campaign runtime contract.");
	AddTag(Event_Resonance_Setup_RoutedFire, "Sov.Event.Resonance.Setup.RoutedFire", "Native campaign runtime contract.");
	AddTag(State_Resonance_CounterWindow, "Sov.State.Resonance.CounterWindow", "Native campaign runtime contract.");
	AddTag(State_Resonance_ProtectedTarget, "Sov.State.Resonance.ProtectedTarget", "Native campaign runtime contract.");
	AddTag(State_Invulnerable_Respawn, "Sov.State.Invulnerable.Respawn", "Native campaign runtime contract.");
	AddTag(State_Recovery_Rescue, "Sov.State.Recovery.Rescue", "Native campaign runtime contract.");
	AddTag(State_Traversal, "Sov.State.Traversal", "Native campaign runtime contract.");
	AddTag(State_Choice_Unresolved, "Sov.State.Choice.Unresolved", "Native campaign runtime contract.");
	AddTag(Character_Player, "Sov.Character.Player", "Parent identity tag for playable Sovereign characters.");
	AddTag(Character_Player_Tarrik, "Sov.Character.Player.Tarrik", "Tarrik Walcur player character.");
	AddTag(Character_Player_Selene, "Sov.Character.Player.Selene", "Selene Veyne player character.");
	AddTag(Character_Enemy_Boss, "Sov.Character.Enemy.Boss", "Boss identity tag used by reduced-reward combat loops.");

	AddTag(Ability, "Sov.Ability", "Parent classification for project-owned Sovereign abilities.");
	AddTag(Ability_ActivateFail_Echo, "Sov.Ability.ActivateFail.Echo", "Ability activation failed its Echo threshold or spend check.");
	AddTag(Ability_Echo, "Sov.Ability.Echo", "Parent classification for every Echo-spending ability.");
	AddTag(Ability_Defense_Selene_Deflection, "Sov.Ability.Defense.Selene.Deflection", "Selene's short precision-deflection action.");
	AddTag(Ability_Weapon_Cinderline_PrimaryFire, "Sov.Ability.Weapon.Cinderline.PrimaryFire", "Ordinary Cinderline primary-fire classification used by Tarrik's Echo cadence loop.");
	AddTag(Ability_NPC_ReformationDrone_Gunfire, "Sov.Ability.NPC.ReformationDrone.Gunfire", "Standard gunfire attack used by Reformation drones.");
	AddTag(Ability_NPC_ReformationDrone_RocketLauncher, "Sov.Ability.NPC.ReformationDrone.RocketLauncher", "Rocket launcher attack used by Reformation drones.");
	AddTag(Ability_NPC_ReformationDrone_SelfDestruct, "Sov.Ability.NPC.ReformationDrone.SelfDestruct", "Pursuit and radial self-destruction attack used by explosive Reformation drones.");
	AddTag(Ability_NPC_DominionHound_Bite, "Sov.Ability.NPC.DominionHound.Bite", "Standard close-range bite attack used by Dominion hounds.");
	AddTag(Ability_NPC_DominionHound_HornCharge, "Sov.Ability.NPC.DominionHound.HornCharge", "Committed heavy horn-charge attack used by Dominion hounds.");
	AddTag(Ability_NPC_DominionHound_Pounce, "Sov.Ability.NPC.DominionHound.Pounce", "Snapshot-target heavy pounce attack used by Dominion hounds.");
	AddTag(Ability_NPC_DominionHandler_CommandHound, "Sov.Ability.NPC.DominionHandler.CommandHound", "Dominion Handler order that authorizes a linked hound's horn charge.");
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
	AddTag(State_CommandLink_Active, "Sov.State.CommandLink.Active", "This combatant is receiving coordination from an active authored command link.");
	AddTag(State_CommandLink_Severed, "Sov.State.CommandLink.Severed", "This combatant's authored command link was severed for the current encounter state.");
	AddTag(State_CommandLink_HoundChargeAuthorized, "Sov.State.CommandLink.HoundChargeAuthorized", "Transient server-only GAS gate used with the Handler's native dispatch scope for one exact hound Horn Charge order.");
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
	AddTag(State_Status, "Sov.State.Status", "Parent tag for active project-owned status state.");
	AddTag(State_Status_Burning, "Sov.State.Status.Burning", "The target is taking periodic damage from Burn.");
	AddTag(State_Status_Chilled, "Sov.State.Status.Chilled", "Movement and control resistance are reduced by cryothermal pressure.");
	AddTag(State_Status_Frozen, "Sov.State.Status.Frozen", "The target is held by an authored hard-Freeze effect.");
	AddTag(State_Status_DeviceDisabled, "Sov.State.Status.DeviceDisabled", "Eligible combat systems are disabled by Disruption.");
	AddTag(State_Status_Exposed, "Sov.State.Status.Exposed", "The target is temporarily vulnerable to an authored exposure payoff.");
	AddTag(State_Status_Corrupted, "Sov.State.Status.Corrupted", "The target has nonzero persistent Eclipse corruption exposure.");
	AddTag(State_Corruption, "Sov.State.Corruption", "Parent tag for the target's exact corruption-pressure band.");
	AddTag(State_Weapon_VerityAbsent, "Sov.State.Weapon.VerityAbsent", "Verity is committed to its authoritative Dispatch flight and cannot be drawn as a duplicate.");
	AddTag(Campaign_Value_Withheld, "Sov.Campaign.Value.Withheld", "Authored immutable campaign fact or discrete value.");
	AddTag(Campaign_Fact_MarketShot, "Sov.Campaign.Fact.MarketShot", "Authored immutable campaign fact or discrete value.");
	AddTag(Campaign_Value_WoundedAlive, "Sov.Campaign.Value.WoundedAlive", "Authored immutable campaign fact or discrete value.");
	AddTag(Campaign_Fact_Caelus, "Sov.Campaign.Fact.Caelus", "Authored immutable campaign fact or discrete value.");
	AddTag(Campaign_Value_Tarrik, "Sov.Campaign.Value.Tarrik", "Authored immutable campaign fact or discrete value.");
	AddTag(Campaign_Fact_Heir, "Sov.Campaign.Fact.Heir", "Authored immutable campaign fact or discrete value.");
	AddTag(State_Corruption_OverwriteRisk, "Sov.State.Corruption.OverwriteRisk", "Mission-permitted corruption exposure band; presentation never changes player input.");
	AddTag(State_Corruption_Contest, "Sov.State.Corruption.Contest", "Mission-permitted corruption exposure band; presentation never changes player input.");
	AddTag(State_Corruption_Intrusion, "Sov.State.Corruption.Intrusion", "Mission-permitted corruption exposure band; presentation never changes player input.");
	AddTag(State_Corruption_Trace, "Sov.State.Corruption.Trace", "Mission-permitted corruption exposure band; presentation never changes player input.");

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
	AddTag(Status_Application_NativeOwned, "Sov.Status.Application.NativeOwned", "The native payload consumes status acceptance and owns its effects; generic status listeners must not duplicate them.");
	AddTag(Status_Apply_Burn, "Sov.Status.Apply.Burn", "A resolved hit requests the project-owned Burn status.");
	AddTag(Status_Apply_Chill, "Sov.Status.Apply.Chill", "A resolved hit requests the project-owned Chill status.");
	AddTag(Status_Apply_Freeze, "Sov.Status.Apply.Freeze", "A resolved hit requests the project-owned hard-Freeze status.");
	AddTag(Status_Apply_DeviceDisabled, "Sov.Status.Apply.DeviceDisabled", "A resolved hit requests a device-disable status on eligible targets.");
	AddTag(Status_Apply_Exposed, "Sov.Status.Apply.Exposed", "A resolved hit requests the project-owned Exposed status.");
	AddTag(Status_Apply_Corruption, "Sov.Status.Apply.Corruption", "A resolved hit requests persistent corruption exposure.");
	AddTag(Status_Immunity, "Sov.Status.Immunity", "Parent tag for project-owned status immunities.");
	AddTag(Status_Immunity_All, "Sov.Status.Immunity.All", "The target rejects every project-owned status application.");
	AddTag(Status_Immunity_Burn, "Sov.Status.Immunity.Burn", "The target rejects project-owned Burn effects.");
	AddTag(Status_Immunity_Chill, "Sov.Status.Immunity.Chill", "The target rejects project-owned Chill effects.");
	AddTag(Status_Immunity_Freeze, "Sov.Status.Immunity.Freeze", "The target rejects hard Freeze and should receive its authored fallback.");
	AddTag(Status_Immunity_DeviceDisable, "Sov.Status.Immunity.DeviceDisable", "The target rejects device-disable effects.");
	AddTag(Status_Immunity_Exposed, "Sov.Status.Immunity.Exposed", "The target rejects project-owned Exposed effects.");
	AddTag(Status_Immunity_Corruption, "Sov.Status.Immunity.Corruption", "The target rejects corruption exposure requests.");
	AddTag(Status_Cleanse, "Sov.Status.Cleanse", "Parent tag for project-owned status-removal requests.");
	AddTag(Status_Cleanse_All, "Sov.Status.Cleanse.All", "Remove every cleansable project-owned status.");
	AddTag(Status_Cleanse_Burn, "Sov.Status.Cleanse.Burn", "Remove active Burn.");
	AddTag(Status_Cleanse_Chill, "Sov.Status.Cleanse.Chill", "Remove active Chill.");
	AddTag(Status_Cleanse_Freeze, "Sov.Status.Cleanse.Freeze", "Remove active Freeze.");
	AddTag(Status_Cleanse_DeviceDisabled, "Sov.Status.Cleanse.DeviceDisabled", "Remove active device disable.");
	AddTag(Status_Cleanse_Exposed, "Sov.Status.Cleanse.Exposed", "Remove active Exposed.");
	AddTag(Status_Cleanse_Corruption, "Sov.Status.Cleanse.Corruption", "Request the authored corruption remedy instead of erasing exposure directly.");

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
	AddTag(SetByCaller_Status_Duration, "Sov.SetByCaller.Status.Duration", "Status duration override in seconds; zero selects the definition default.");

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
	AddTag(Event_CommandLink_Severed, "Sov.Event.CommandLink.Severed", "An active authored command link completed one authoritative Active-to-Severed transition.");
	AddTag(Event_Echo_Gained_PerfectGuard, "Sov.Event.Echo.Gained.PerfectGuard", "Echo was granted for a perfect guard.");
	AddTag(Event_Status_ApplicationRequested, "Sov.Event.Status.ApplicationRequested", "A resolved hit requested project-owned status application.");
	AddTag(Event_Status_Applied, "Sov.Event.Status.Applied", "A project-owned status was applied for the first time.");
	AddTag(Event_Status_Refreshed, "Sov.Event.Status.Refreshed", "An active project-owned status accepted a refresh.");
	AddTag(Event_Status_StackChanged, "Sov.Event.Status.StackChanged", "An active project-owned status changed its authoritative stack count.");
	AddTag(Event_Status_Removed, "Sov.Event.Status.Removed", "A project-owned status ended or was removed.");
	AddTag(Event_Status_Cleansed, "Sov.Event.Status.Cleansed", "A project-owned status was removed by an accepted cleanse request.");
	AddTag(Event_Status_Rejected, "Sov.Event.Status.Rejected", "A project-owned status application was rejected by authoritative policy.");
	AddTag(Event_Corruption_ExposureChanged, "Sov.Event.Corruption.ExposureChanged", "Persistent corruption exposure changed.");
	AddTag(Event_Corruption_BandChanged, "Sov.Event.Corruption.BandChanged", "Corruption crossed into a different exact pressure band.");
	AddTag(Event_Corruption_RemedyChanged, "Sov.Event.Corruption.RemedyChanged", "An authored corruption remedy changed persistent exposure.");
	AddTag(Event_Corruption_OverwriteRiskReached, "Sov.Event.Corruption.OverwriteRiskReached", "Corruption entered the Overwrite Risk band; this is not an automatic fail state.");

	AddTag(Echo_Source_PerfectGuard, "Sov.Echo.Source.PerfectGuard", "Echo source for Tarrik perfect guard.");
	AddTag(Echo_Source_PerfectDeflection, "Sov.Echo.Source.PerfectDeflection", "Echo source for Selene's correctly timed Deflection.");
	AddTag(Echo_Source_WeakPointBreak, "Sov.Echo.Source.WeakPointBreak", "Echo source for breaking an authored weak point.");
	AddTag(Echo_Source_WeakPointHit, "Sov.Echo.Source.WeakPointHit", "Echo source for an accepted hit on an unbroken authored weak point.");
	AddTag(Echo_Source_CommandLinkSever, "Sov.Echo.Source.CommandLinkSever", "Echo source for Selene severing one active hostile command link.");
	AddTag(Echo_Source_GuardPressure, "Sov.Echo.Source.GuardPressure", "Combat activity caused by intentional guard pressure.");
	AddTag(Echo_Source_GuardCounter, "Sov.Echo.Source.GuardCounter", "Echo source for a landed Tarrik guard counter.");
	AddTag(Echo_Source_CinderlineCadence, "Sov.Echo.Source.CinderlineCadence", "Echo source for completing Tarrik's Cinderline firing cadence.");
	AddTag(Echo_Source_CinderlinePrecisionKill, "Sov.Echo.Source.CinderlinePrecisionKill", "Echo source for a Cinderline precision kill.");
	AddTag(Echo_Source_CombatSustainPickup, "Sov.Echo.Source.CombatSustainPickup", "Echo source for collecting a transient combat-sustain mote.");
	AddTag(Echo_Source_ExposureKill, "Sov.Echo.Source.ExposureKill", "Echo source for defeating an enemy while its Exposed status is active.");
	AddTag(State_CommandTarget_Window, "Sov.State.CommandTarget.Window", "Target-owned authored command-target reward window.");
	AddTag(State_Target_Exposed, "Sov.State.Target.Exposed", "Target-owned authored exposure for precision reward eligibility.");
	AddTag(State_Target_Marked, "Sov.State.Target.Marked", "Target-owned authored mark for precision reward eligibility.");
	AddTag(Echo_Source_UndetectedBypass, "Sov.Echo.Source.UndetectedBypass", "Echo awarded for crossing a registered encounter without being perceived.");
	AddTag(Echo_Source_PrecisionChain, "Sov.Echo.Source.PrecisionChain", "Echo awarded for a verified precision kill chain.");
	AddTag(Echo_Source_MarkedKill, "Sov.Echo.Source.MarkedKill", "Echo awarded for killing a marked or exposed hostile.");
	AddTag(Echo_Source_CommandTargetKill, "Sov.Echo.Source.CommandTargetKill", "Echo awarded for killing a target within its authored command window.");
	AddTag(Echo_Source_PoiseBreak, "Sov.Echo.Source.PoiseBreak", "Echo awarded for an accepted hostile Poise break.");
	AddTag(Echo_Source_HeavyMultiHit, "Sov.Echo.Source.HeavyMultiHit", "Echo awarded once for one verified heavy attack hitting three distinct hostiles.");
	AddTag(Echo_Source_ProtectionIntercept, "Sov.Echo.Source.ProtectionIntercept", "Echo awarded from an authoritative ally-protection damage receipt.");
}

void FSovGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName(TagName),
		FString(TEXT("(Native) ")) + FString(TagComment));
}
