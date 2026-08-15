// Copyright Narrative Tools 2024. 

#include "ArsenalSettings.h"
#include "AI/Navigation/NarrativeNavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

UArsenalSettings::UArsenalSettings()
{
#if WITH_EDITOR
	bDisplayProjectSettingsNotification = true;
	bCheckProjectSettingsOnStartup = true;
	bCheckAddOnSettingsOnStartup = true; 
#endif
	
	DefaultSaveName = "NarrativeSave";
	NumSaveSlots = 5;
	MetadataSaveFileName = "NarrativeSaveMetadata";
	DefaultUsername = "You";
	InteractionTraceChannel = ECC_GameTraceChannel1;
	WeaponTraceChannel = ECC_GameTraceChannel2;
	
	GameEntryMap = FSoftObjectPath(TEXT("/Script/Engine.World'/NarrativePro/Pro/Demo/Maps/OpenWorld/L_DemoMap_OpenWorld.L_DemoMap_OpenWorld'"));
	CharacterCreatorMap = FSoftObjectPath(TEXT("/Script/Engine.World'/NarrativePro/Core/CharCreator/CharacterCreator.CharacterCreator'"));

	bLoadCharacterCreatorOnNewGame = false;

	BBKey_Delay = FName("Delay");
	BBKey_TargetLocation = FName("TargetLocation");
	BBKey_TargetRotation = FName("TargetRotation");
	BBKey_PlayerPawn = FName("PlayerPawn");
	BBKey_AttackTarget = FName("AttackTarget");
	BBKey_FollowTarget = FName("FollowTarget");

	MasterSoundClass = FSoftObjectPath(TEXT("/Script/Engine.SoundClass'/NarrativePro/Pro/Core/Audio/Classes/Narrative_Master.Narrative_Master'"));
	SFXSoundClass = FSoftObjectPath(TEXT("/Script/Engine.SoundClass'/NarrativePro/Pro/Core/Audio/Classes/Narrative_SFX.Narrative_SFX'"));
	UISoundClass = FSoftObjectPath(TEXT("/Script/Engine.SoundClass'/NarrativePro/Pro/Core/Audio/Classes/Narrative_UI.Narrative_UI'"));
	DialogueSoundClass = FSoftObjectPath(TEXT("/Script/Engine.SoundClass'/NarrativePro/Pro/Core/Audio/Classes/Narrative_Dialogue.Narrative_Dialogue'"));
	MusicSoundClass = FSoftObjectPath(TEXT("/Script/Engine.SoundClass'/NarrativePro/Pro/Core/Audio/Classes/Narrative_Music.Narrative_Music'"));

	ChainLinkAngleTolerance = 40.0f;
	SmallestChainLinkLength = 15.0f;
	ChainLinkCorrectionAngleTolerance = 10.0f;
	CorrectionIterationCount = 2;

	auto HealGameplayEffect_SetByCallerFinder = ConstructorHelpers::FClassFinder<UGameplayEffect>(TEXT("/Script/Engine.Blueprint'/NarrativePro/Pro/Core/Abilities/GameplayEffects/GE_Heal_SetByCaller.GE_Heal_SetByCaller_C'"));

	if (HealGameplayEffect_SetByCallerFinder.Succeeded())
	{
		HealGameplayEffect_SetByCaller = HealGameplayEffect_SetByCallerFinder.Class;
	}

	auto DamageGameplayEffect_SetByCallerFinder = ConstructorHelpers::FClassFinder<UGameplayEffect>(TEXT("/Script/Engine.Blueprint'/NarrativePro/Pro/Core/Abilities/GameplayEffects/GE_Damage_SetByCaller.GE_Damage_SetByCaller_C'"));

	if (DamageGameplayEffect_SetByCallerFinder.Succeeded())
	{
		DamageGameplayEffect_SetByCaller = DamageGameplayEffect_SetByCallerFinder.Class;
	}

	auto GameplayEffect_DynamicTagFinder = ConstructorHelpers::FClassFinder<UGameplayEffect>(TEXT("/Script/Engine.Blueprint'/NarrativePro/Pro/Core/Abilities/GameplayEffects/GE_DynamicTag.GE_DynamicTag_C'"));

	if (GameplayEffect_DynamicTagFinder.Succeeded())
	{
		DynamicTagGameplayEffect = GameplayEffect_DynamicTagFinder.Class;
	}

}

#if WITH_EDITOR
void UArsenalSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	UObject::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UArsenalSettings, ChainLinkAngleTolerance) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArsenalSettings, SmallestChainLinkLength) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArsenalSettings, ChainLinkCorrectionAngleTolerance) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArsenalSettings, CorrectionIterationCount) )
	{
		UNarrativeNavigationSystem::RebuildAllCover();
	}
}
#endif