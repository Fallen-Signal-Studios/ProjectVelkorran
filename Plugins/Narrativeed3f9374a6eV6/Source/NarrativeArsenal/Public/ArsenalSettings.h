// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <Sound/SoundWave.h>
#include "Containers/EnumAsByte.h"
#include "Engine/EngineTypes.h"
#include "GameplayEffect.h"
#include "AI/Cover/CoverTypes.h"
#include "ArsenalSettings.generated.h"

class UAbilitySystemComponent; 

class UTaggedMusicSet;
/**
 * Configurable settings for Narrative Pro. 
 */
UCLASS(BlueprintType, config = Engine, defaultconfig)
class NARRATIVEARSENAL_API UArsenalSettings : public UObject
{
	GENERATED_BODY()


public: 

	UArsenalSettings();

	/** This is the default save game name - it is recommended you don't change this unless you know what you are doing */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Save System")
	FString DefaultSaveName; 

	//** If empty we'll keep Unreal assigned username, but if set to a valid string, game mode will override your player to use this.  */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Gameplay")
	FString DefaultUsername;

	/** The map that the default main menu will load. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly,Category = "Narrative Pro|Maps", meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath GameEntryMap;

	/** The map that we'll open when the player wants to open the character creator */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly,Category = "Narrative Pro|Maps", meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath CharacterCreatorMap;

	/** If true, starting a new game will load the character creator, instead of loading the entry map. (The default character creator will then throw you into the entry map when you finish creation.) */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly,Category = "Narrative Pro|Maps")
	bool bLoadCharacterCreatorOnNewGame;

	/** How many save slots the default menu should support */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Save System")
	int32 NumSaveSlots;

	/** The save file name we'll use to store metadata about our main saves. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Save System")
	FString MetadataSaveFileName;
	
	/** TargetLocation BB key name  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Blackboard Keys")
	FName BBKey_TargetLocation;

	/** TargetRotation BB key name  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Blackboard Keys")
	FName BBKey_TargetRotation;

	/** Delay BB key name  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Blackboard Keys")
	FName BBKey_Delay;

	/** PlayerPawn BB key name  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Blackboard Keys")
	FName BBKey_PlayerPawn;

	/** FollowTarget BB key name  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Blackboard Keys")
	FName BBKey_FollowTarget;

	/** Attack target BB key name  */
	UPROPERTY(VisibleAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Blackboard Keys")
	FName BBKey_AttackTarget;

	/** Overall sound class */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	FSoftObjectPath MasterSoundClass;

	/** SFX sound class */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	FSoftObjectPath SFXSoundClass;

	/** UI sound class */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	FSoftObjectPath UISoundClass;

	/** Dialogue sound class */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	FSoftObjectPath DialogueSoundClass;

	/** Music sound class */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	FSoftObjectPath MusicSoundClass;

	UPROPERTY(EditDefaultsOnly, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds")
	TSoftObjectPtr<USoundBase> MasterMetaSound;

	UPROPERTY(EditDefaultsOnly, config, BlueprintReadOnly, Category = "Narrative Pro|Sounds")
	TSoftObjectPtr<UTaggedMusicSet> DefaultMusicSet;
	
	// Gameplay effect used to apply damage.  Uses SetByCaller for the damage magnitude.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Pro|GAS")
	TSubclassOf<UGameplayEffect> HealGameplayEffect_SetByCaller;

	// Gameplay effect used to apply damage.  Uses SetByCaller for the damage magnitude.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Pro|GAS")
	TSubclassOf<UGameplayEffect> DamageGameplayEffect_SetByCaller;

	// Gameplay effect used to add and remove dynamic tags.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Pro|GAS")
	TSubclassOf<UGameplayEffect> DynamicTagGameplayEffect;

	//** Define a nice display name for any gameplay tags that need it here.   */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Narrative Pro|GAS")
	TMap<FGameplayTag, FText> TagFriendlyDisplayNames;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, meta=(GetOptions="Engine.KismetSystemLibrary.GetCollisionProfileNames"), Category = "Narrative Pro|Trace Channels")
	FName InteractionTraceProfile = "Interaction";

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Trace Channels")
	TEnumAsByte<ECollisionChannel> WeaponTraceChannel;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Trace Channels")
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Interaction")
	TSubclassOf<class AItemPickup> DefaultInteractablePickup;

	/* TODO: describe this better */
	/// defines the maximum angle difference (in degrees) between each link in a cover chain.
	/// changing this value will cause cover to regenerate. for the current world
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Cover", meta=(ClampMin=1, ClampMax=180, ForceUnits="Degrees"))
	float ChainLinkAngleTolerance;

	// prevents chain segments less than the set length.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Cover")
	float SmallestChainLinkLength;

	// any difference between two links less than the angle set are merged into one chain link.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Cover", meta=(ClampMin=1, ClampMax=90, ForceUnits="Degrees"))
	float ChainLinkCorrectionAngleTolerance;

	// number of times to run correction.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Cover", meta=(ClampMin=1, ClampMax=4))
	uint8 CorrectionIterationCount;
	
	// the distance between points along a cover chain.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Narrative Pro|Cover")
	FCoverTraceConfig CoverTraceConfig;

#if WITH_EDITORONLY_DATA

	/// when true if project settings do not match some of the required Narrative Pro settings then a pop-up is displayed.
	UPROPERTY(EditAnywhere, config, Category = "Narrative Pro|Editor")
	bool bDisplayProjectSettingsNotification;
	
	/// If true, we'll check if your projects settings match the Narrative Pro intended settings.
	/// You usually want to uncheck this once you've applied the NP settings so you can then edit the settings to your needs. 
	UPROPERTY(EditAnywhere, config, Category = "Narrative Pro|Editor")
	bool bCheckProjectSettingsOnStartup;

	/*If true, we'll skip the NP project settings and just make sure add-ons settings are applied. */
	UPROPERTY(EditAnywhere, config, Category = "Narrative Pro|Editor")
	bool bCheckAddOnSettingsOnStartup;
#endif

private:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
};
