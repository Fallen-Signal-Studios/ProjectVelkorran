// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Abilities/SovGameplayAbility_TarrikEcho.h"

#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"

USovGameplayAbility_TarrikEchoBase::USovGameplayAbility_TarrikEchoBase()
{
	RequiredCharacterTag = FSovGameplayTags::Get().Character_Player_Tarrik;
}

USovGameplayAbility_TarrikCinderSlam::USovGameplayAbility_TarrikCinderSlam()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderSlam;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderSlam;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Velkorran;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderSlamName", "Cinder Slam");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderSlamDescription",
		"Drive Velkorran into the ground, releasing a radial Cinder blast with heavy Poise pressure and a brief protective ward around Tarrik.");
}

USovGameplayAbility_TarrikVelkorransHunger::USovGameplayAbility_TarrikVelkorransHunger()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 50.0f;
	EchoCost = 50.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_VelkorransHunger;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_VelkorransHunger;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Velkorran;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "VelkorransHungerName", "Velkorran's Hunger");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"VelkorransHungerDescription",
		"Sling an inferno from Velkorran's edge, dealing direct projectile damage and applying Burn.");
}

USovGameplayAbility_TarrikCinderStickyGrenade::USovGameplayAbility_TarrikCinderStickyGrenade()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 35.0f;
	EchoCost = 35.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderStickyGrenade;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderStickyGrenade;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability1;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Universal;
	WeaponGatePolicy = ESovEchoWeaponGatePolicy::AnyAllowedWielded;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderStickyGrenadeName", "Cinder Sticky Grenade");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderStickyGrenadeDescription",
		"Throw a Cinder charge that adheres to a target or surface, then detonates after a short fuse with thermal damage and Burn.");
}

USovGameplayAbility_TarrikCinderJudgement::USovGameplayAbility_TarrikCinderJudgement()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 50.0f;
	EchoCost = 50.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderJudgement;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderJudgement;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability2;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Cinderline;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderJudgementName", "Cinder Judgement");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderJudgementDescription",
		"Fire an overcharged Cinderline shot that deals direct impact damage and detonates in a controlled Cinder blast.");
}

USovGameplayAbility_TarrikCinderlineRequiem::USovGameplayAbility_TarrikCinderlineRequiem()
{
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	MinimumEchoRequired = 90.0f;
	EchoCost = 90.0f;
	EchoSpendTag = Tags.Ability_Echo_Tarrik_CinderlineRequiem;
	FGameplayTagContainer AssetTags = GetAssetTags();
	AssetTags.AddTag(EchoSpendTag);
	SetAssetTags(AssetTags);
	AbilityAnimSetTag = Tags.AnimSet_Ability_Tarrik_CinderlineRequiem;
	InputTag = FNarrativeGameplayTags::Get().Narrative_Input_Ability3;
	WeaponFamily = ESovTarrikEchoWeaponFamily::Cinderline;
	AbilityDisplayName = NSLOCTEXT("SovTarrikEcho", "CinderlineRequiemName", "Cinderline Requiem");
	AbilityDescription = NSLOCTEXT(
		"SovTarrikEcho",
		"CinderlineRequiemDescription",
		"Release a penetrating Cinderline shot that marks its path, then erupts into a chained burning line with extreme Poise pressure.");
}
