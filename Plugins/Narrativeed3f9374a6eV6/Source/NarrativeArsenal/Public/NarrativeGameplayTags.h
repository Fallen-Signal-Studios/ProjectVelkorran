// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "GameplayTagContainer.h"
#include "HAL/Platform.h"

class UGameplayTagsManager;

/**
 * Gameplay tags singleton class, we use a technique learned from Lyra. 
 * We define these natively so Narrative pro users don't need to manually add a whole bunch of tags, which would be quite annoying.
 * This way they can hit the ground running. 
 */
struct NARRATIVEARSENAL_API FNarrativeGameplayTags
{
public:

	static const FNarrativeGameplayTags& Get() { return GameplayTags; }

	static void InitializeNativeTags();

	static FGameplayTag FindTagByString(FString TagString, bool bMatchPartialString = false);

public:

	FGameplayTag Ability_ActivateFail_NoAmmo;
	FGameplayTag Ability_ActivateFail_IsDead;
	FGameplayTag Ability_ActivateFail_Cooldown;
	FGameplayTag Ability_ActivateFail_Cost;
	FGameplayTag Ability_ActivateFail_TagsBlocked;
	FGameplayTag Ability_ActivateFail_TagsMissing;
	FGameplayTag Ability_ActivateFail_Networking;
	FGameplayTag Ability_ActivateFail_ActivationGroup;
	
	FGameplayTag Ability_DamageType_Heavy;
	FGameplayTag Ability_DamageType_Melee;
	FGameplayTag Ability_DamageType_Ranged;
	FGameplayTag Ability_DamageType_Poison;

	FGameplayTag Ability_WeaponFire;
	FGameplayTag Ability_Death;
	FGameplayTag Ability_MeleeAttack;
	FGameplayTag Ability_MagicAttack;
	FGameplayTag Ability_Aim;
	FGameplayTag Ability_Reload;
	FGameplayTag Ability_WeaponBash;
	FGameplayTag Ability_Jump;
	FGameplayTag Ability_Crouch;
	FGameplayTag Ability_Sprint;
	FGameplayTag Ability_Dodge;
	FGameplayTag Ability_Block;
	FGameplayTag Ability_WieldWeapon;
	FGameplayTag Ability_WieldWeapon_Off;
	FGameplayTag Ability_Interact_Sit;

	FGameplayTag Camera_FirstPerson_ForceFollowHeadBoneLocked;
	FGameplayTag Camera_FirstPerson_Follow3PHeadLocation;
	FGameplayTag Camera_FirstPerson_Follow3PHeadRotation;
	FGameplayTag Camera_FirstPerson_FollowControlRotation;
	FGameplayTag Camera_FirstPerson_CameraInsideHead;
	FGameplayTag Camera_FirstPerson_DisableFirstPersonRendering;
	FGameplayTag Camera_FirstPerson_AlwaysTickFirstPersonMesh;

	FGameplayTag Camera_Perspective_FirstPerson;
	FGameplayTag Camera_Perspective_ThirdPerson;


	FGameplayTag GameplayCue_TakeDamage;
	FGameplayTag GameplayCue_Weapon_Fire;
	FGameplayTag GameplayCue_Weapon_Impact;

	FGameplayTag GameplayEvent_BlockedAttack;
	FGameplayTag GameplayEvent_Interact;
	FGameplayTag GameplayEvent_Interact_SkipEntry;
	FGameplayTag GameplayEvent_Interact_Steal;

	FGameplayTag GameplayEvent_KilledEnemy;
	FGameplayTag GameplayEvent_NotifyHolster;
	FGameplayTag GameplayEvent_Death;
	FGameplayTag GameplayEvent_Reload;
	FGameplayTag GameplayEvent_ToggleWield_On;
	FGameplayTag GameplayEvent_ToggleWield_Off;
	FGameplayTag GameplayEvent_MeleeHit;
	FGameplayTag GameplayEvent_EndAttack;
	FGameplayTag GameplayEvent_NotifyInteract;
	FGameplayTag GameplayEvent_WantsEndInteract;

	FGameplayTag CharacterCreator_Scalars;
	FGameplayTag CharacterCreator_Vectors;
	FGameplayTag CharacterCreator_Form_Male;
	FGameplayTag CharacterCreator_Form_Female;

	FGameplayTag Attachment_Slot_Scope;
	FGameplayTag Attachment_Slot_Muzzle;

	FGameplayTag Equipment_Slot_Ammo;
	FGameplayTag Equipment_Slot_Helmet;
	FGameplayTag Equipment_Slot_Offhand;
	FGameplayTag Equipment_Slot_Body;
	FGameplayTag Equipment_Slot_Torso;
	FGameplayTag Equipment_Slot_Torso_1P;
	FGameplayTag Equipment_Slot_Hands_1P;
	FGameplayTag Equipment_Slot_Legs;
	FGameplayTag Equipment_Slot_Feet;
	FGameplayTag Equipment_Slot_Hands;
	FGameplayTag Equipment_Slot_Backpack;
	FGameplayTag Equipment_Slot_Necklace;
	FGameplayTag Equipment_Slot_Weapon_Back;
	FGameplayTag Equipment_Slot_Weapon_Hip;
	FGameplayTag Equipment_Slot_Throwable;
	FGameplayTag Equipment_Slot_Glasses;
	FGameplayTag Equipment_Slot_Weapon_HipLeft;
	FGameplayTag Equipment_Slot_Weapon_HipRight;
	FGameplayTag Equipment_Slot_Weapon_BackA;
	FGameplayTag Equipment_Slot_Weapon_BackB;

	FGameplayTag Weapon_WieldSlot_Mainhand;
	FGameplayTag Weapon_WieldSlot_Offhand;

	FGameplayTag Equipment_Slot_Groom_Hair;
	FGameplayTag Equipment_Slot_Groom_Eyebrows;
	FGameplayTag Equipment_Slot_Groom_Beard;
	FGameplayTag Equipment_Slot_Groom_Fuzz;
	FGameplayTag Equipment_Slot_Groom_Moustache;
	FGameplayTag Equipment_Slot_Groom_Eyelashes;

	FGameplayTag Equipment_Slot_Mesh_Hair;
	FGameplayTag Equipment_Slot_Mesh_Eyebrows;
	FGameplayTag Equipment_Slot_Mesh_Beard;
	FGameplayTag Equipment_Slot_Mesh_Fuzz;
	FGameplayTag Equipment_Slot_Mesh_Moustache;
	FGameplayTag Equipment_Slot_Mesh_Eyelashes;

	FGameplayTag Equipment_Slot_Character_Mesh;
	FGameplayTag Equipment_Slot_Character_LocalMesh;
	FGameplayTag Equipment_Slot_Face;

	FGameplayTag Narrative_Anim_OverrideLayer_Ragdoll;
	FGameplayTag Narrative_Anim_OverrideLayer_Swimming;
	FGameplayTag Narrative_Anim_OverrideLayer_Driving;
	FGameplayTag Narrative_Anim_OverrideLayer_Climbing;

	FGameplayTag Narrative_AnimSets_Flinch_Back;
	FGameplayTag Narrative_AnimSets_Flinch_Left;
	FGameplayTag Narrative_AnimSets_Flinch_Right;
	FGameplayTag Narrative_AnimSets_Flinch_Forward;

	FGameplayTag Narrative_AnimSets_Stumble_Back;
	FGameplayTag Narrative_AnimSets_Stumble_Left;
	FGameplayTag Narrative_AnimSets_Stumble_Right;
	FGameplayTag Narrative_AnimSets_Stumble_Forward;

	FGameplayTag Narrative_AnimSets_Attack_Unarmed_Light;
	FGameplayTag Narrative_AnimSets_Attack_Unarmed_Heavy;

	FGameplayTag State_NPC_Activity_Idle;
	FGameplayTag State_NPC_Activity_Following;
	FGameplayTag State_NPC_Activity_Attacking;

	FGameplayTag State_NPC_IsAggressive;
	FGameplayTag State_NPC_DisableAggro;
	FGameplayTag State_NPC_IsBusy;
	FGameplayTag State_NPC_DisableLooting;
	FGameplayTag State_NPC_PauseIdleSequence;

	FGameplayTag State_Player_Camera_ForceThirdPerson;
	FGameplayTag State_Player_WantsCinematicBars;
	FGameplayTag State_Player_WantsHideHUD;
	FGameplayTag State_Player_WantsHideHUD_All;
	FGameplayTag State_Player_DoNotDisturb;
	FGameplayTag State_Player_IgnoreLookInput;
	FGameplayTag State_InvisibleToEnemies;
	FGameplayTag State_Invulnerable;
	FGameplayTag State_SequencerControlled;
	FGameplayTag State_RootMotionControlled;
	FGameplayTag State_DialogueControlled;
	FGameplayTag State_DontReturnToSpawn;
	FGameplayTag State_Sitting;
	FGameplayTag State_Sleeping;
	FGameplayTag State_Movement_PostponePathUpdates;

	FGameplayTag State_Movement_Lock;
	FGameplayTag State_Movement_Falling;
	FGameplayTag State_Movement_Swimming;
	FGameplayTag State_Movement_Walking;
	FGameplayTag State_Movement_SlowWalking;
	FGameplayTag State_Movement_Climbing;
	FGameplayTag State_Movement_Ragdoll;
	FGameplayTag State_Movement_InCover;
	FGameplayTag State_IsDead;
	FGameplayTag State_Busy;
	FGameplayTag State_OnMount;
	FGameplayTag State_Interacting;
	FGameplayTag State_Weapon_IsReloading;
	FGameplayTag State_Weapon_IsFiring;
	FGameplayTag State_Weapon_IsAiming;
	FGameplayTag State_Weapon_IsAiming_Local;
	FGameplayTag State_Weapon_Equipping;
	FGameplayTag State_Weapon_Equipped;
	FGameplayTag State_Weapon_BlockFiring;
	FGameplayTag State_Weapon_Blocking;
	FGameplayTag State_Weapon_ForceHolster;
	FGameplayTag State_BlockFastTravel;
	FGameplayTag State_BlockSaving;
	FGameplayTag State_UI_HideCrosshair;

	FGameplayTag TaggedDialogue_Greet;
	FGameplayTag TaggedDialogue_Farewell;
	FGameplayTag TaggedDialogue_Taunt;
	FGameplayTag TaggedDialogue_Attack;
	FGameplayTag TaggedDialogue_BeginAttacking;

	FGameplayTag TaggedDialogue_Investigate_HeardSound_StartSearch;
	FGameplayTag TaggedDialogue_Investigate_HeardSound_CouldntFindAnything;
	FGameplayTag TaggedDialogue_Investigate_HeardSound_FoundEnemy;
	FGameplayTag TaggedDialogue_Investigate_SearchForEnemy_StartSearch;
	FGameplayTag TaggedDialogue_Investigate_SearchForEnemy_FoundEnemy;
	FGameplayTag TaggedDialogue_Investigate_SearchForEnemy_CouldntFindEnemy;

	FGameplayTag TaggedDialogue_DidntFindEnemy;
	FGameplayTag TaggedDialogue_FriendlyFire;

	FGameplayTag UI_Layer_Game;
	FGameplayTag UI_Layer_Menu;
	FGameplayTag UI_Layer_Modal;

	FGameplayTag UI_Navigator_MapLayer_Default;
	FGameplayTag UI_Navigator_MapLayer_Interior;
	FGameplayTag UI_Navigator_MapLayer_FirstFloor;

	FGameplayTag SetByCaller_Damage;
	FGameplayTag SetByCaller_Heal;
	FGameplayTag SetByCaller_AttackDamage;
	FGameplayTag SetByCaller_AttackRating;
	FGameplayTag SetByCaller_StealthRating;
	FGameplayTag SetByCaller_Armor;
	FGameplayTag SetByCaller_Health;
	FGameplayTag SetByCaller_MaxHealth;
	FGameplayTag SetByCaller_Stamina;
	FGameplayTag SetByCaller_MaxStamina;
	FGameplayTag SetByCaller_Duration;
	FGameplayTag SetByCaller_XP;
	FGameplayTag SetByCaller_Sneak;

	FGameplayTag Narrative_Settlements;
	FGameplayTag Narrative_Settlements_Test_DemoHall;
	FGameplayTag Narrative_Settlements_Test_BanditCamp;
	FGameplayTag Narrative_Settlements_Test_WeaponStore;

	FGameplayTag Narrative_POIs;
	FGameplayTag Narrative_POIs_Test_DemoHall;
	FGameplayTag Narrative_POIs_Test_BanditCamp;
	FGameplayTag Narrative_POIs_Test_WeaponStore;

	FGameplayTag Narrative_Factions;
	FGameplayTag Narrative_Factions_Heroes;
	FGameplayTag Narrative_Factions_Bandits;
	FGameplayTag Narrative_Factions_HostileAll;
	FGameplayTag Narrative_Factions_HostileOthers;
	FGameplayTag Narrative_Factions_FriendlyAll;

	FGameplayTag Narrative_Input_None;
	FGameplayTag Narrative_Input_Confirm;
	FGameplayTag Narrative_Input_Cancel;
	FGameplayTag Narrative_Input_Attack;
	FGameplayTag Narrative_Input_AltAttack;
	FGameplayTag Narrative_Input_Ability1;
	FGameplayTag Narrative_Input_Ability2;
	FGameplayTag Narrative_Input_Ability3;
	FGameplayTag Narrative_Input_Reload;
	FGameplayTag Narrative_Input_Jump;
	FGameplayTag Narrative_Input_Crouch;
	FGameplayTag Narrative_Input_Sprint;
	FGameplayTag Narrative_Input_Throw;


	FGameplayTag Narrative_Music_Theme_Default;
	FGameplayTag Narrative_Music_Theme_Combat;


protected:

	void AddAllTags(UGameplayTagsManager& Manager);
	void AddTag(FGameplayTag& OutTag, const ANSICHAR* TagName, const ANSICHAR* TagComment);

private:

	static FNarrativeGameplayTags GameplayTags;
};
