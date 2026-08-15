// Copyright Narrative Tools 2024. 


#include "NarrativeGameplayTags.h"

#include "Containers/Array.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagsManager.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"

FNarrativeGameplayTags FNarrativeGameplayTags::GameplayTags;

void FNarrativeGameplayTags::InitializeNativeTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	GameplayTags.AddAllTags(Manager);

	// Notify manager that we are done adding native tags.
	Manager.DoneAddingNativeTags();
}

void FNarrativeGameplayTags::AddAllTags(UGameplayTagsManager& Manager)
{
	AddTag(Ability_ActivateFail_NoAmmo, "Ability.ActivateFail.NoAmmo", "Ability failed to activate because our owner didn't have ammo.");
	AddTag(Ability_ActivateFail_IsDead, "Ability.ActivateFail.IsDead", "Ability failed to activate because its owner is dead.");
	AddTag(Ability_ActivateFail_Cooldown, "Ability.ActivateFail.Cooldown", "Ability failed to activate because it is on cool down.");
	AddTag(Ability_ActivateFail_Cost, "Ability.ActivateFail.Cost", "Ability failed to activate because it did not pass the cost checks.");
	AddTag(Ability_ActivateFail_TagsBlocked, "Ability.ActivateFail.TagsBlocked", "Ability failed to activate because tags are blocking it.");
	AddTag(Ability_ActivateFail_TagsMissing, "Ability.ActivateFail.TagsMissing", "Ability failed to activate because tags are missing.");
	AddTag(Ability_ActivateFail_Networking, "Ability.ActivateFail.Networking", "Ability failed to activate because it did not pass the network checks.");
	AddTag(Ability_ActivateFail_ActivationGroup, "Ability.ActivateFail.ActivationGroup", "Ability failed to activate because of its activation group.");

	//These can be used by exec calcs and all manner of other things to conditionally change effects based on damage. 
	AddTag(Ability_DamageType_Heavy, "Abilities.Damage.Types.Heavy", "Heavy damage type. Can be combined with other types, ie Heavy Poison damage, Heavy Melee Damage, etc.");
	AddTag(Ability_DamageType_Melee, "Abilities.Damage.Types.Melee", "Melee damage type.");
	AddTag(Ability_DamageType_Ranged, "Abilities.Damage.Types.Ranged", "Ranged damage type.");
	AddTag(Ability_DamageType_Poison, "Abilities.Damage.Types.Poison", "Poison damage type.");

	AddTag(Ability_WeaponFire, "Abilities.Attacks.WeaponFire", "Weapon firing ability tag.");
	AddTag(Ability_MeleeAttack, "Abilities.Attacks.MeleeAttack", "Melee attack ability tag.");
	AddTag(Ability_MeleeAttack, "Abilities.Attacks.MagicAttack", "Magic attack ability tag.");
	AddTag(Ability_Death, "Abilities.Death", "Death ability tag.");
	AddTag(Ability_Aim, "Abilities.Aim", "Aim ability tag.");
	AddTag(Ability_Reload, "Abilities.Reload", "Reload ability tag.");
	AddTag(Ability_WeaponBash, "Abilities.WeaponBash", "WeaponBash ability tag.");
	AddTag(Ability_Jump, "Abilities.Jump", "Jump ability tag.");
	AddTag(Ability_Crouch, "Abilities.Crouch", "Crouch ability tag.");
	AddTag(Ability_Sprint, "Abilities.Sprint", "Sprint ability tag.");
	AddTag(Ability_Dodge, "Abilities.Dodge", "Dodge ability tag.");
	AddTag(Ability_Block, "Abilities.Block", "Block ability tag.");
	AddTag(Ability_WieldWeapon, "Abilities.WieldWeapon", "WieldWeapon ability tag.");
	AddTag(Ability_Interact_Sit, "Abilities.Interact.Sit", "Tag for the sit interaction ability.");

	AddTag(Camera_FirstPerson_Follow3PHeadLocation, "Camera.FirstPerson.Follow3PHeadLocation", "When applied to character we force first person camera to follow head bone loc");
	AddTag(Camera_FirstPerson_Follow3PHeadRotation, "Camera.FirstPerson.Follow3PHeadRotation", "When applied to character we force first person camera to follow head bone rot");
	AddTag(Camera_FirstPerson_FollowControlRotation, "Camera.FirstPerson.FollowControlRotation", "When applied to character we force first person camera to follow control rot (clamped specified by gameplay code)");

	AddTag(Camera_FirstPerson_AlwaysTickFirstPersonMesh, "Camera.FirstPerson.AlwaysTickFirstPersonMesh", "If tag is present we'll keep ticking FP mesh even if not in first person.");
	AddTag(Camera_FirstPerson_DisableFirstPersonRendering, "Camera.FirstPerson.DisableFirstPersonRendering", "When added we switch off first person rendering, sometimes useful for example when driving. ");
	AddTag(Camera_FirstPerson_CameraInsideHead, "Camera.FirstPerson.CameraInsideHead", "Applied to the character when the camera is inside the head - this is different from being in first person mode, as transition may not be complete.");
	AddTag(Camera_Perspective_FirstPerson, "Camera.Perspective.FirstPerson", "Applied to the player character when we're in first person mode, even if current transitioning");
	AddTag(Camera_Perspective_ThirdPerson, "Camera.Perspective.ThirdPerson", "Applied to the player character when we're in Third person mode, even if current transitioning");

	//Unfortunately users need to define these manually as GameplayCues cant use native tags 
	//AddTag(GameplayCue_TakeDamage, "GameplayCue.TakeDamage", "TakeDamage gameplaycue tag. ");
	//AddTag(GameplayCue_Weapon_Fire, "GameplayCue.Weapon.Fire", "Weapon Fire gameplaycue tag. ");
	//AddTag(GameplayCue_Weapon_Impact, "GameplayCue.Weapon.Impact", "Weapon Impact gameplaycue tag. ");
	
	AddTag(CharacterCreator_Scalars, "Narrative.CharacterCreator.Scalars", "Scalar TagIDs are added as subtags of this tag. ");
	AddTag(CharacterCreator_Vectors, "Narrative.CharacterCreator.Vectors", "Vector TagIDs are added as subtags of this tag. ");
	AddTag(CharacterCreator_Form_Male, "Narrative.CharacterCreator.Forms.Male", "The default male form that comes with the character creator. ");
	AddTag(CharacterCreator_Form_Female, "Narrative.CharacterCreator.Forms.Female", "The default female form that comes with the character creator. ");

	AddTag(Attachment_Slot_Scope, "Narrative.Equipment.Weapon.AttachSlot.Scope", "Tag for the weapon scope attachment slot");
	AddTag(Attachment_Slot_Muzzle, "Narrative.Equipment.Weapon.AttachSlot.Muzzle", "Tag for the weapon Muzzle attachment slot");

	AddTag(Equipment_Slot_Offhand, "Narrative.Equipment.Slot.Mesh.Offhand", "Tag for the offhand equipment slot, used for things like shields");
	AddTag(Equipment_Slot_Body, "Narrative.Equipment.Slot.Mesh.Body", "Tag for the body equipment slot. This is different from the BaseBody which defines the skeleton. ");
	AddTag(Equipment_Slot_Ammo, "Narrative.Equipment.Slot.Mesh.Ammo", "Tag for the Ammo equipment slot. If your game requires equippable arrows, bullets, etc this is the slot to use.");
	AddTag(Equipment_Slot_Helmet, "Narrative.Equipment.Slot.Mesh.Helmet", "Tag for the helmet equipment slot.");
	AddTag(Equipment_Slot_Torso, "Narrative.Equipment.Slot.Mesh.Torso", "Tag for the Torso equipment slot.");
	AddTag(Equipment_Slot_Torso_1P, "Narrative.Equipment.Slot.Mesh.Torso.1P", "Tag for the 1P Torso equipment slot.");
	AddTag(Equipment_Slot_Legs, "Narrative.Equipment.Slot.Mesh.Legs", "Tag for the Legs equipment slot.");
	AddTag(Equipment_Slot_Feet, "Narrative.Equipment.Slot.Mesh.Feet", "Tag for the Feet equipment slot.");
	AddTag(Equipment_Slot_Hands, "Narrative.Equipment.Slot.Mesh.Hands", "Tag for the Hands equipment slot.");
	AddTag(Equipment_Slot_Hands_1P, "Narrative.Equipment.Slot.Mesh.Hands.1P", "Tag for the 1P hands equipment slot.");
	AddTag(Equipment_Slot_Weapon_Back, "Narrative.Equipment.Slot.Weapon.Back", "Tag for the weapon slot where the weapon equips to the players back. ");
	AddTag(Equipment_Slot_Weapon_Hip, "Narrative.Equipment.Slot.Weapon.Hip", "Tag for the weapon slot where the weapon equips to the players hip. ");
	AddTag(Equipment_Slot_Backpack, "Narrative.Equipment.Slot.Mesh.Backpack", "Tag for the Backpack equipment slot.");
	AddTag(Equipment_Slot_Necklace, "Narrative.Equipment.Slot.Mesh.Necklace", "Tag for the Necklace equipment slot.");
	AddTag(Equipment_Slot_Throwable, "Narrative.Equipment.Slot.Mesh.Throwable", "Tag for the Throwable equipment slot.");
	AddTag(Equipment_Slot_Glasses, "Narrative.Equipment.Slot.Mesh.Glasses", "Tag for the Glasses equipment slot.");

	AddTag(Equipment_Slot_Weapon_HipLeft, "Narrative.Equipment.Slot.Weapon.HipLeft", "Tag for the weapon slot where the weapon equips to the players hip left. ");
	AddTag(Equipment_Slot_Weapon_HipRight, "Narrative.Equipment.Slot.Weapon.HipRight", "Tag for the weapon slot where the weapon equips to the players hip Right. ");
	AddTag(Equipment_Slot_Weapon_BackA, "Narrative.Equipment.Slot.Weapon.BackA", "Tag for the weapon slot where the weapon equips to the players Back A. ");
	AddTag(Equipment_Slot_Weapon_BackB, "Narrative.Equipment.Slot.Weapon.BackB", "Tag for the weapon slot where the weapon equips to the players Back B. ");

	AddTag(Weapon_WieldSlot_Mainhand, "Narrative.Equipment.WieldSlot.Mainhand", "Wield slot identifying the players main hand. ");
	AddTag(Weapon_WieldSlot_Offhand, "Narrative.Equipment.WieldSlot.Offhand", "Wield slot identifying the players main hand. ");

	//We support groom versions for mesh slots, but may remove support for this as grooms arent really game ready anyways... 
	AddTag(Equipment_Slot_Groom_Eyebrows, "Narrative.Equipment.Slot.Groom.Eyebrows", "Tag for the Eyebrows equipment slot.");
	AddTag(Equipment_Slot_Groom_Hair, "Narrative.Equipment.Slot.Groom.Hair", "Tag for the Hair equipment slot.");
	AddTag(Equipment_Slot_Groom_Beard, "Narrative.Equipment.Slot.Groom.Beard", "Tag for the Beard equipment slot.");
	AddTag(Equipment_Slot_Groom_Fuzz, "Narrative.Equipment.Slot.Groom.Fuzz", "Tag for the Fuzz equipment slot.");
	AddTag(Equipment_Slot_Groom_Moustache, "Narrative.Equipment.Slot.Groom.Moustache", "Tag for the Moustache equipment slot.");
	AddTag(Equipment_Slot_Groom_Eyelashes, "Narrative.Equipment.Slot.Groom.Eyelashes", "Tag for the Eyelashes equipment slot.");

	//Mesh versions for facial hair - these are the ones that will likely be used by peoples games 
	AddTag(Equipment_Slot_Mesh_Eyebrows, "Narrative.Equipment.Slot.Mesh.Eyebrows", "Tag for the Eyebrows equipment slot.");
	AddTag(Equipment_Slot_Mesh_Hair, "Narrative.Equipment.Slot.Mesh.Hair", "Tag for the Hair equipment slot.");
	AddTag(Equipment_Slot_Mesh_Beard, "Narrative.Equipment.Slot.Mesh.Beard", "Tag for the Beard equipment slot.");
	AddTag(Equipment_Slot_Mesh_Fuzz, "Narrative.Equipment.Slot.Mesh.Fuzz", "Tag for the Fuzz equipment slot.");
	AddTag(Equipment_Slot_Mesh_Moustache, "Narrative.Equipment.Slot.Mesh.Moustache", "Tag for the Moustache equipment slot.");
	AddTag(Equipment_Slot_Mesh_Eyelashes, "Narrative.Equipment.Slot.Mesh.Eyelashes", "Tag for the Eyelashes equipment slot.");

	
	AddTag(Equipment_Slot_Character_Mesh, "Narrative.Equipment.Slot.Mesh.CharacterMesh", "The default skeletal mesh that exists on ACharacter. - ie Character.GetMesh()");
	AddTag(Equipment_Slot_Character_LocalMesh, "Narrative.Equipment.Slot.Mesh.LocalMesh", "This is a special mesh that is used as a leader mesh for first person mode. Only the local player gets given one. ");
	AddTag(Equipment_Slot_Face, "Narrative.Equipment.Slot.Mesh.Face", "Tag for the Face equipment slot.");

	AddTag(Narrative_Anim_OverrideLayer_Ragdoll, "Narrative.Anim.OverrideLayer.Ragdoll", "Override layer to apply when Ragdoll.");
	AddTag(Narrative_Anim_OverrideLayer_Swimming, "Narrative.Anim.OverrideLayer.Swimming", "Override layer to apply when swimming.");
	AddTag(Narrative_Anim_OverrideLayer_Climbing, "Narrative.Anim.OverrideLayer.Climbing", "Override layer to apply when Climbing.");
	AddTag(Narrative_Anim_OverrideLayer_Driving, "Narrative.Anim.OverrideLayer.Driving", "Override layer to apply when Driving.");

	AddTag(Narrative_AnimSets_Flinch_Back, "Narrative.Anim.AnimSets.Flinch.Back", "Anim Set for flinching when hit from the back. ");
	AddTag(Narrative_AnimSets_Flinch_Left, "Narrative.Anim.AnimSets.Flinch.Left", "Anim Set for flinching when hit from the Left. ");
	AddTag(Narrative_AnimSets_Flinch_Right, "Narrative.Anim.AnimSets.Flinch.Right", "Anim Set for flinching when hit from the Right. ");
	AddTag(Narrative_AnimSets_Flinch_Forward, "Narrative.Anim.AnimSets.Flinch.Forward", "Anim Set for flinching when hit from the Forward. ");

	AddTag(Narrative_AnimSets_Stumble_Back, "Narrative.Anim.AnimSets.Stumble.Back", "Anim Set for stumbling when hit from the back. ");
	AddTag(Narrative_AnimSets_Stumble_Left, "Narrative.Anim.AnimSets.Stumble.Left", "Anim Set for stumbling when hit from the Left. ");
	AddTag(Narrative_AnimSets_Stumble_Right, "Narrative.Anim.AnimSets.Stumble.Right", "Anim Set for stumbling when hit from the Right. ");
	AddTag(Narrative_AnimSets_Stumble_Forward, "Narrative.Anim.AnimSets.Stumble.Forward", "Anim Set for stumbling when hit from the Forward. ");

	AddTag(Narrative_AnimSets_Attack_Unarmed_Light, "Narrative.Anim.AnimSets.Attacks.Unarmed.Light", "Anim Set for a normal unarmed punch attack.");
	AddTag(Narrative_AnimSets_Attack_Unarmed_Heavy, "Narrative.Anim.AnimSets.Attacks.Unarmed.Heavy", "Anim Set for a heavy unarmed punch attack.");

	AddTag(State_NPC_Activity_Idle, "Narrative.State.NPC.Activity.Idle", "Tag added to NPCs whilst they are running the idle activity.");
	AddTag(State_NPC_Activity_Following, "Narrative.State.NPC.Activity.Following", "Tag added to NPCs whilst they are running the FollowCharacter activity.");
	AddTag(State_NPC_Activity_Attacking, "Narrative.State.NPC.Activity.Attacking", "Tag added to NPCs whilst they are running the attacking activity.");

	AddTag(State_NPC_IsBusy, "Narrative.State.NPC.IsBusy", "Tag added to NPCs whilst they are busy and can't talk.");
	AddTag(State_NPC_IsAggressive, "Narrative.State.NPC.Aggressive", "Tag is added to NPCs that are aggressive towards someone.");
	AddTag(State_NPC_DisableAggro, "Narrative.State.NPC.DisableAggro", "When added to NPCs aggro will be disabled.");
	AddTag(State_NPC_DisableLooting, "Narrative.State.NPC.DisableLooting", "When added to NPCs looting will be disabled.");
	AddTag(State_NPC_PauseIdleSequence, "Narrative.State.NPC.PauseIdleSequence", "When added to NPCs will stop BPA_ReturnToSpawn, which will stop their idle sequence playing.");

	AddTag(State_Player_Camera_ForceThirdPerson, "Narrative.State.Player.Camera.ForceThirdPerson", "When tag is present we'll force player into third person view. Useful for anim states that don't support third person. ");
	AddTag(State_Player_WantsCinematicBars, "Narrative.State.Player.WantsCinematicBars", "Tag is added to player to notify we want cinematic bars added to HUD.");
	AddTag(State_Player_WantsHideHUD, "Narrative.State.Player.WantsHideHUD", "Tag is added to player to notify we want HUD hidden.");
	AddTag(State_Player_WantsHideHUD_All, "Narrative.State.Player.WantsHideHUD.All", "Tag is added to player to notify we want HUD hidden, including essential widgets");
	AddTag(State_Player_DoNotDisturb, "Narrative.State.Player.DoNotDisturb", "If added to the player symbolizes that we are doing something important and don't want to be disturned. By default will turn off random encounters. ");
	AddTag(State_Player_IgnoreLookInput, "Narrative.State.Player.IgnoreLookInput", "Allows look input to be ignored via a tag.");

	AddTag(State_Weapon_IsReloading, "Narrative.State.Weapon.Reloading", "Tag is added whilst character is reloading.");
	AddTag(State_Weapon_IsFiring, "Narrative.State.Weapon.Firing", "Tag is added whilst character is firing.");
	AddTag(State_Weapon_IsAiming, "Narrative.State.Weapon.Aiming", "Tag is added whilst character is aiming.");
	AddTag(State_Weapon_IsAiming_Local, "Narrative.State.Weapon.Aiming.Local", "Tag is added whilst character is aiming locally. This is needed because clients predict their aiming and dont want to wait for server. ");
	AddTag(State_Weapon_Equipping, "Narrative.State.Weapon.Equipping", "Tag is added whilst we our pulling our weapon out, but haven't yet equipped it.");
	AddTag(State_Weapon_Equipped, "Narrative.State.Weapon.Equipped", "Tag is added whilst our weapon out and ready to fire.");
	AddTag(State_Weapon_BlockFiring, "Narrative.State.Weapon.BlockFiring", "Tag will block fire ability.");
	AddTag(State_Weapon_Blocking, "Narrative.State.Weapon.Blocking", "True if we're blocking with a weapon that supports it.");
	AddTag(State_Weapon_ForceHolster, "Narrative.State.Weapon.ForceHolster", "Character contains code to force unequip the weapon whenever this tag is added. Handles re-equipping when tag is removed. ");

	AddTag(State_IsDead, "Narrative.State.IsDead", "Tag is added to characters that are dead. ");
	AddTag(State_Busy, "Narrative.State.Busy", "Tag is added to characters that are seated, stunned, etc. Will lock movement, prevent attacking, etc.");
	AddTag(State_OnMount, "Narrative.State.OnMount", "Tag is added to characters that are on a mount currently.");
	AddTag(State_Movement_PostponePathUpdates, "Narrative.State.Movement.PostponePathUpdates", "When added will defer movement updates - usually added to AI when gameplay code (MoveToLocationAndRotation node) is controlling our character and we don't anything else like MoveTo BehaviorTree tasks interrupting that until done. ");
	AddTag(State_Movement_Lock, "Narrative.State.Movement.Lock", "When added to a narrative character the move component will lock us in place. ");
	AddTag(State_Movement_Falling, "Narrative.State.Movement.Falling", "Added to character whilst character is falling so abilities and other GAS related things can check this via tag. ");
	AddTag(State_Movement_Swimming, "Narrative.State.Movement.Swimming", "Added to character whilst character is Swimming so abilities and other GAS related things can check this via tag. ");
	AddTag(State_Movement_Climbing, "Narrative.State.Movement.Climbing", "Added to character whilst character is Climbing so abilities and other GAS related things can check this via tag. ");
	AddTag(State_Movement_Ragdoll, "Narrative.State.Movement.Ragdoll", "Added to character whilst character is ragdoll so abilities and other GAS related things can check this via tag. ");
	AddTag(State_Movement_Walking, "Narrative.State.Movement.Walking", "Added to character whilst character is Walking so abilities and other GAS related things can check this via tag. ");
	AddTag(State_Movement_SlowWalking, "Narrative.State.Movement.SlowWalking", "When added to a narrative character the movement will make us slow walk. ");
	AddTag(State_Movement_InCover, "Narrative.State.Movement.InCover", "Tag is added to characters that are taking cover.");

	AddTag(State_InvisibleToEnemies, "Narrative.State.InvisibleToEnemies", "Enemies will ignore this person if they have this tag.");
	AddTag(State_Invulnerable, "Narrative.State.Invulnerable", "If this tag is added to the owner damage will be nullified.");
	AddTag(State_SequencerControlled, "Narrative.State.SequencerControlled", "Tag is added to character if a cinematic is controlling it.");
	AddTag(State_RootMotionControlled, "Narrative.State.RootMotionControlled", "If this tag is added to character will cause ABP to enable 'Root Motion From Everything'. Useful when cinematics need to move character around. Doesn't work in networked games. ");
	AddTag(State_DialogueControlled, "Narrative.State.DialogueControlled", "Tag is added to character if they are in a dialogue.");
	AddTag(State_DontReturnToSpawn, "Narrative.State.DontReturnToSpawn", "When added to a character they won't return to their spawn until tag is removed.");
	AddTag(State_BlockFastTravel, "Narrative.State.BlockFastTravel", "If the player has this tag we'll deny them the ability to fast travel.");
	AddTag(State_BlockSaving, "Narrative.State.BlockSaving", "If the player has this tag we'll deny them the ability to save their game.");
	AddTag(State_Interacting, "Narrative.State.Interacting", "Tag is added to characters that are interacting with something, a bench, a lever, etc. Added whilst interact abilities are running.");
	AddTag(State_UI_HideCrosshair, "Narrative.State.UI.HideCrosshair", "If the player has this tag, the crosshair will be hidden until this tag is removed.");

	AddTag(TaggedDialogue_Greet, "Narrative.TaggedDialogue.Greet", "Fires the NPCs greet dialogue. 'Hello', 'How are you doing friend?' etc ");
	AddTag(TaggedDialogue_Farewell, "Narrative.TaggedDialogue.Farewell", "Fires the NPCs farewell dialogue. 'Farewell', 'See you later!' etc");
	AddTag(TaggedDialogue_Taunt, "Narrative.TaggedDialogue.Taunt", "Fires the NPCs taunt dialogue");
	AddTag(TaggedDialogue_BeginAttacking, "Narrative.TaggedDialogue.BeginAttacking", "Fires the NPCs begin attacking dialogue. 'You're going to regret coming here!', 'Its over for you!' ");
	AddTag(TaggedDialogue_Investigate_HeardSound_StartSearch, "Narrative.TaggedDialogue.Investigate.HeardSound.StartSearch", "Tagged dialogue we fire when an NPC starts investigating a sound - 'What was that!' ");
	AddTag(TaggedDialogue_Investigate_HeardSound_CouldntFindAnything, "Narrative.TaggedDialogue.Investigate.HeardSound.CouldntFindAnything", "Tagged dialogue we fire when an NPC started investigating a sound but couldnt find anything ");
	AddTag(TaggedDialogue_Investigate_HeardSound_FoundEnemy, "Narrative.TaggedDialogue.Investigate.HeardSound.FoundEnemy", "Tagged dialogue we fire when an NPC starts investigating a sound and finds an enemy ");
	AddTag(TaggedDialogue_Investigate_SearchForEnemy_StartSearch, "Narrative.TaggedDialogue.Investigate.SearchForEnemy.StartSearch", "Tagged dialogue we fire when an NPC starts searching for an enemy they lost sight of");
	AddTag(TaggedDialogue_Investigate_SearchForEnemy_FoundEnemy, "Narrative.TaggedDialogue.Investigate.SearchForEnemy.FoundEnemy", "Tagged dialogue we fire when an NPC finds an enemy after searching for them");
	AddTag(TaggedDialogue_Investigate_SearchForEnemy_CouldntFindEnemy, "Narrative.TaggedDialogue.Investigate.SearchForEnemy.CouldntFindEnemy", "Tagged dialogue we fire when an NPC couldnt find an NPC it tried searching for.");
	AddTag(TaggedDialogue_DidntFindEnemy, "Narrative.TaggedDialogue.DidntFindEnemy", "Fires the NPCs failed search for NPC, if an NPC cant find the footsteps. 'Huh, I guess I was just hearing things.' ");
	AddTag(TaggedDialogue_FriendlyFire, "Narrative.TaggedDialogue.FriendlyFire", "Fires the NPCs friendly fire dialogue, when you shoot a friendly. 'Ow, stop that!', etc. ");

	AddTag(GameplayEvent_BlockedAttack, "GameplayEvent.BlockedAttack", "Event used to tell someone blocking that they blocked an attack.");
	AddTag(GameplayEvent_Interact, "GameplayEvent.Interact", "Event used to initiate the Interact ability.");
	AddTag(GameplayEvent_Interact_SkipEntry, "GameplayEvent.Interact.SkipEntry", "Event used to initiate the Interact ability, and that it should skip the entry");
	AddTag(GameplayEvent_Interact_Steal, "GameplayEvent.Interact.Steal", "Event used to initiate the Interact ability and notify we are stealing. ");
	AddTag(GameplayEvent_KilledEnemy, "GameplayEvent.KilledEnemy", "Event that fires when we kill someone.");
	AddTag(GameplayEvent_NotifyHolster, "GameplayEvent.NotifyHolster", "Event that fires from the holster montage to indicate we should swap the weapon.");
	AddTag(GameplayEvent_Death, "GameplayEvent.Death", "Event that fires on death. This event only fires on the server.");
	AddTag(GameplayEvent_ToggleWield_On, "GameplayEvent.ToggleWield.On", "Event that fires when the player wants to wield a weapon. The weapon is passed via instigator tags.");
	AddTag(GameplayEvent_ToggleWield_Off, "GameplayEvent.ToggleWield.Off", "Event that fires when the player wants to unwield a weapon. The weapon is passed via instigator tags.");
	AddTag(GameplayEvent_Reload, "GameplayEvent.Reload", "Event that fires on Reload. ");
	AddTag(GameplayEvent_MeleeHit, "GameplayEvent.AttackApex", "Event that is sent when an abilities attack montage reaches its apex, ie when the wand should cast, sword should hit, etc.");
	AddTag(GameplayEvent_EndAttack, "GameplayEvent.EndAttack", "Event that fires from attack montage when the attack is done. This may be before the montage ends. ");
	AddTag(GameplayEvent_NotifyInteract, "GameplayEvent.NotifyInteract", "Event that fires from interact montage when our interactable needs to do something. ie lever pulled, switch pressed, etc.");
	AddTag(GameplayEvent_WantsEndInteract, "GameplayEvent.WantsEndInteract", "Event that fires when a character wants to end their interaction. ");

	AddTag(SetByCaller_Heal, "SetByCaller.Heal", "SetByCaller tag used by heal gameplay effects.");
	AddTag(SetByCaller_Damage, "SetByCaller.Damage", "SetByCaller tag used by damage gameplay effects.");
	AddTag(SetByCaller_Duration, "SetByCaller.Duration", "SetByCaller tag used to define the duration of looping gameplay effects.");
	AddTag(SetByCaller_AttackDamage, "SetByCaller.AttackDamage", "SetByCaller tag used to add attack damage to our attributes.");
	AddTag(SetByCaller_AttackRating, "SetByCaller.AttackRating", "SetByCaller tag used to add attack rating damage to our attributes.");
	AddTag(SetByCaller_StealthRating, "SetByCaller.StealthRating", "SetByCaller tag used to add stealth rating damage to our attributes.");
	AddTag(SetByCaller_Health, "SetByCaller.Health", "SetByCaller tag used to add health to our attributes");
	AddTag(SetByCaller_MaxHealth, "SetByCaller.MaxHealth", "SetByCaller tag used to add max health to our attributes");
	AddTag(SetByCaller_Armor, "SetByCaller.Armor", "SetByCaller tag used to add armor rating damage to our attributes");
	AddTag(SetByCaller_Stamina, "SetByCaller.Stamina", "SetByCaller tag used to add stamina to our attributes.");
	AddTag(SetByCaller_MaxStamina, "SetByCaller.MaxStamina", "SetByCaller tag used to add max stamina to our attributes.");
	AddTag(SetByCaller_XP, "SetByCaller.XP", "SetByCaller tag used to add XP to our attributes.");
	AddTag(SetByCaller_Sneak, "SetByCaller.Sneak", "SetByCaller tag used to add Sneak to our attributes.");

	AddTag(Narrative_Settlements, "Narrative.Settlements", "Settlements should be added under this subtag.");
	AddTag(Narrative_POIs, "Narrative.POIs", "POIs should be added under this subtag.");
	AddTag(Narrative_Factions, "Narrative.Factions", "Factions should be added as subtags to this tag.");

	//These tags are used by the demo level. Feel free to remove them to clean up your project! 
	AddTag(Narrative_Settlements_Test_DemoHall, "Narrative.Settlements.Demo.DemoHall", "Demo settlement used by the demo level that comes with Narrative Pro.");
	AddTag(Narrative_Settlements_Test_BanditCamp, "Narrative.Settlements.Demo.BanditCamp", "Demo settlement used by the demo level that comes with Narrative Pro.");
	AddTag(Narrative_Settlements_Test_WeaponStore, "Narrative.Settlements.Demo.WeaponStore", "Demo settlement used by the demo level that comes with Narrative Pro.");
	AddTag(Narrative_Factions_Heroes, "Narrative.Factions.Heroes", "Demo heroes faction used by the demo level.");
	AddTag(Narrative_Factions_Bandits, "Narrative.Factions.Bandits", "Demo bandits faction used by the demo level.");
	
	AddTag(UI_Layer_Game, "UI.Layer.Game", "This layer is used for menus that exist on the game layer."); 
	AddTag(UI_Layer_Menu, "UI.Layer.Menu", "This layer is used for menus that exist on the Menu layer.");
	AddTag(UI_Layer_Modal, "UI.Layer.Modal", "This layer is used for menus that exist on the Modal layer.");

	AddTag(UI_Navigator_MapLayer_Default, "Narrative.UI.Navigator.MapLayer.Default", "Default Map Layer.");
	AddTag(UI_Navigator_MapLayer_Interior, "Narrative.UI.Navigator.MapLayer.Interior", "Interior Map Layer. This is one of the defaults, feel free to add your own. ");
	AddTag(UI_Navigator_MapLayer_FirstFloor, "Narrative.UI.Navigator.MapLayer.FirstFloor", "FirstFloor Map Layer. This is one of the defaults, feel free to add your own. ");

	AddTag(Narrative_Factions_FriendlyAll, "Narrative.Factions.FriendlyAll", "This faction will be Friendly to all factions");
	AddTag(Narrative_Factions_HostileOthers, "Narrative.Factions.HostileOthers", "This faction will be hostile to other factions");
	AddTag(Narrative_Factions_HostileAll, "Narrative.Factions.HostileAll", "This faction will be hostile to other factions");

	AddTag(Narrative_POIs_Test_DemoHall, "Narrative.POIs.Demo.DemoHall", "Demo POI used by the demo level that comes with Narrative Pro.");
	AddTag(Narrative_POIs_Test_BanditCamp, "Narrative.POIs.Demo.BanditCamp", "Demo POI used by the demo level that comes with Narrative Pro.");
	AddTag(Narrative_POIs_Test_WeaponStore, "Narrative.POIs.Demo.WeaponStore", "Demo POI used by the demo level that comes with Narrative Pro.");

	AddTag(Narrative_Input_None, "Narrative.Input.None", "None - no input defined");
	AddTag(Narrative_Input_Confirm, "Narrative.Input.Confirm", "Confirm action for abilities");
	AddTag(Narrative_Input_Cancel, "Narrative.Input.Cancel", "Cancel action for abilities");
	AddTag(Narrative_Input_Attack, "Narrative.Input.Attack", "3 LMB - Generic attack ability bound to attack slot");
	AddTag(Narrative_Input_AltAttack, "Narrative.Input.AltAttack", "4 RMB - Generic ability bound to alt attack slot (aim for firearms, block for melee, etc)");
	AddTag(Narrative_Input_Ability1, "Narrative.Input.Ability1", "5 Q - Generic ability bound to ability1 slot");
	AddTag(Narrative_Input_Ability2, "Narrative.Input.Ability2", "6 E - Generic ability bound to ability2 slot");
	AddTag(Narrative_Input_Ability3, "Narrative.Input.Ability3", "7 F - Generic ability bound to ability3 slot");
	AddTag(Narrative_Input_Reload, "Narrative.Input.Reload", "8 R - reloads if our weapon supports");
	AddTag(Narrative_Input_Jump, "Narrative.Input.Jump", "Space bar - Jumps");
	AddTag(Narrative_Input_Crouch, "Narrative.Input.Crouch", "L-Ctrl - Crouch");
	AddTag(Narrative_Input_Sprint, "Narrative.Input.Sprint", "Left shift - sprint");
	AddTag(Narrative_Input_Throw, "Narrative.Input.Throw", "G - Throwable attack");

	AddTag(Narrative_Music_Theme_Default, "Narrative.Music.Theme.Default", "Default music theme that will play if using the MusicComponent.");
	AddTag(Narrative_Music_Theme_Combat, "Narrative.Music.Theme.Combat", "Default combat theme that will play if using the MusicComponent.");
}

void FNarrativeGameplayTags::AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment)
{
	OutTag = UGameplayTagsManager::Get().AddNativeGameplayTag(FName(TagName), FString(TEXT("(Native) ")) + FString(TagComment));
}

FGameplayTag FNarrativeGameplayTags::FindTagByString(FString TagString, bool bMatchPartialString)
{
	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	FGameplayTag Tag = Manager.RequestGameplayTag(FName(*TagString), false);

	if (!Tag.IsValid() && bMatchPartialString)
	{
		FGameplayTagContainer AllTags;
		Manager.RequestAllGameplayTags(AllTags, true);

		for (const FGameplayTag TestTag : AllTags)
		{
			if (TestTag.ToString().Contains(TagString))
			{
				UE_LOG(LogTemp, Display, TEXT("Could not find exact match for tag [%s] but found partial match on tag [%s]."), *TagString, *TestTag.ToString());
				Tag = TestTag;
				break;
			}
		}
	}

	return Tag;
}
