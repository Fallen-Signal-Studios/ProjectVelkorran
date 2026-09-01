// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "NarrativeCharacter.h"
#include "NarrativeSave.h"
#include "AI/Activities/NPCGoalItem.h"
#include "AI/Activities/NPCActivitySchedule.h"
#include "LevelSequence.h"
#include "NarrativeNPCCharacter.generated.h"

struct FMassEntityHandle;
//Represents a tagged dialogue - this is essentially a dialogue that can be kicked off via a tag "TaggedDialogue.Taunt, TaggedDialogue.Greet, etc. "
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FTaggedDialogue
{
	GENERATED_BODY()

	FTaggedDialogue() 
	{
		Cooldown = 30.f;
		LastPlayTime = -10000.f;
		MaxDistance = 5000.f;
	};

	/** The tag that will kick off this dialogue. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tagged Dialogue", meta = (Categories = "Narrative.TaggedDialogue"))
	FGameplayTag Tag;

	/** The dialogue to begin */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tagged Dialogue")
	TSoftClassPtr<class UDialogue> Dialogue;

	/** The amount of time we should cooldown before playing this dialogue again. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tagged Dialogue")
	float Cooldown;

	/** Tagged dialogue wont play unless we're within this range from it */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tagged Dialogue")
	float MaxDistance;

	/** Tags that will be required for the NPC to begin this tagged dialogue */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tagged Dialogue")
	FGameplayTagContainer RequiredTags;

	/** Tags that if owned by the NPC, will prevent this dialogue beginning. For example, we wouldn't want to greet a player if we were fighting someone. */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Tagged Dialogue")
	FGameplayTagContainer BlockedTags;

	UPROPERTY(BlueprintReadOnly, Category = "Tagged Dialogue")
	float LastPlayTime;
};


//Params for overriding the default options set up in the NPC definition. 
USTRUCT(BlueprintType)
struct FNPCSpawnParams
{
	GENERATED_BODY()

public:

	FNPCSpawnParams()
	{
	bOverride_NPCName = false;

	bOverride_LevelRange = false;

	MinLevel = 1;
	MaxLevel = 1;

	bOverride_DefaultFactions = false;
	bOverride_DefaultOwnedTags = false;
	bOverride_ActivityConfiguration = false;
	ActivityConfiguration = nullptr; 
	bOverride_DefaultItemLoadout = false; 
	DefaultItemLoadout = {};
	bOverride_DefaultAppearance = false;
	DefaultAppearance = nullptr; 
	bOverride_TriggerSets = false;
	TriggerSets = {};
	bOverride_CharacterRandomSeed = false;
	CharacterRandomSeed = -1;
	OptionalIdleSequence = nullptr; 
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_NPCName : 1;

	//The icon to use for this location 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Overrides", meta = (EditCondition="bOverride_NPCName"))
	FText NPCName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_LevelRange : 1;

	/** The NPCs level will be randomized between these values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Overrides", meta = (EditCondition="bOverride_LevelRange", ClampMin=1, ClampMax=5000))
	int32 MinLevel = 1;

	/** The NPCs level will be randomized between these values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Overrides", meta = (EditCondition="bOverride_LevelRange", ClampMin=1, ClampMax=5000))
	int32 MaxLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_DefaultFactions : 1;

	/**The factions this character will be in by default - you can set the characters faction to something else later if desired. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Factions", meta = (Categories = "Narrative.Factions", EditCondition="bOverride_DefaultFactions"))
	FGameplayTagContainer DefaultFactions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_DefaultOwnedTags : 1;

	/** Add any custom tags the character needs in here, for example State.Invulnerable if you want the character to never take damage, or LockMovement to make the character locked in place. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Tags", meta = (Categories = "Narrative.State,Sov.Character", EditCondition="bOverride_DefaultOwnedTags"))
	FGameplayTagContainer DefaultOwnedTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_ActivityConfiguration : 1;

	/** The NPCs activity config. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Overrides", meta = (EditCondition="bOverride_ActivityConfiguration"))
	TSoftObjectPtr<class UNPCActivityConfiguration> ActivityConfiguration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_DefaultItemLoadout : 1;

	/** The items we should grant the character by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Overrides", meta = (EditCondition="bOverride_DefaultItemLoadout"))
	TArray<FLootTableRoll> DefaultItemLoadout;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_DefaultAppearance : 1;

	/**The characters default appearance we want to override to*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Appearance", meta = (EditCondition="bOverride_DefaultAppearance"))
	TSoftObjectPtr<class UCharacterAppearanceBase> DefaultAppearance; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_TriggerSets : 1;

	/** The triggers we want to use as the override */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Triggers", meta = (EditCondition="bOverride_TriggerSets"))
	TArray<TSoftObjectPtr<class UTriggerSet>> TriggerSets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint8 bOverride_CharacterRandomSeed : 1;

	/** Used if you want to manually set the characters random seed instead of having it auto-generate one.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Seed", meta = (EditCondition="bOverride_CharacterRandomSeed"))
	int32 CharacterRandomSeed = -1;

	//Optional sequence to play on the NPC whilst they are milling around. Defined as spawnparam as its more likely to be used that way rather than being tied to an NPC def. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Spawn Component")
	TObjectPtr<class ULevelSequence> OptionalIdleSequence; 
}; 
 
//Information about where the NPC spawned, what owns it
USTRUCT(BlueprintType)
struct FNPCSpawnInfo
{
	GENERATED_BODY()

	FNPCSpawnInfo() = default;

	/** GUID of the spawner that created us. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame, Category = "NPC")
	FGuid OwningSpawnerGUID;

	/** Name of our spawn in that spawner. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame, Category = "NPC")
	FName SpawnName;

	/** Weak ptr to our spawn, only valid if WP still has our spawn loaded in. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Transient, Category = "NPC")
	TWeakObjectPtr<class UNPCSpawnComponent> OwningSpawn;

	/** The save GUID our spawner assigned us. */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame, Category = "NPC")
	FGuid SpawnAssignedSaveGUID;

	/** Transform of our spawn, so we know roughly in which directly to run back to our settlement*/
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame, Category = "NPC")
	FTransform SpawnTransform;

	/**The spawn params that were passed in by whatever spawned us */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame, Category = "NPC")
	FNPCSpawnParams SpawnParams;
};

/**
 * Base class for a NarrativeCharacter that is AI Controlled. 
 */
UCLASS()
class NARRATIVEARSENAL_API ANarrativeNPCCharacter : public ANarrativeCharacter, public INarrativeSavableActor
{
	GENERATED_BODY()
	
public:

	friend class UNarrativeCharacterSubsystem;
	friend class UNPCSpawnComponent;
	friend class ANarrativePlayerController; 

	ANarrativeNPCCharacter(const class FObjectInitializer& ObjectInitializer);

	virtual FGameplayTagContainer GetFactions() const override;
	virtual class UNarrativeInventoryComponent* GetInventoryComponent() const;

protected:



	//NPCs have a seperate inventory for trading from. 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UNarrativeInventoryComponent> TradingInventoryComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UNPCInteractable> NPCInteractableComponent;

	virtual void BeginPlay() override;
	virtual void FellOutOfWorld(const class UDamageType& dmgType);
	virtual ECapsuleRotationSetting GetCapsuleRotationSettings_Implementation() const override; 
	virtual void SpawnDefaultController() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	//FGenericTeamId GetGenericTeamId() const override;
	virtual void AddFaction(const FGameplayTag& Faction) override;
	virtual void RemoveFaction(const FGameplayTag& Faction) override;
	virtual int32 GetCharacterLevel() const override;
	virtual class UCharacterDefinition* GetCharacterDefinition() const override;
	virtual void InitNewCharacter_Implementation(UCharacterDefinition* NewDefinition) override;
	virtual FString GetHumanReadableName() const override;
	virtual void HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	virtual bool IsPlayerControlled() const override;
	virtual bool IsBotControlled() const override;

	//Loading stuff 
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition) override;
	virtual void OnCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId) override;
	virtual TArray<FLootTableRoll> GetDefaultItemLoadout() const override;
	virtual TSoftObjectPtr<UCharacterAppearanceBase> GetDefaultAppearance() const override; 
	virtual TArray<TSoftObjectPtr<class UTriggerSet>> GetDefaultTriggerSets() const override;
	virtual void OnCharacterVisualInitialized() override;

	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

	//Called if we need to load overrides, which will defer CDef loading.  
	UFUNCTION()
	virtual void HandleSpawnParamOverridesLoaded();

	//Called when our activity config is ready 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	void ApplyActivityConfig(class UNPCActivityConfiguration* NPCActivityConfig);
	virtual void ApplyActivityConfig_Implementation(class UNPCActivityConfiguration* NPCActivityConfig);

	//App
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void ApplyActivitySchedules(const TArray<TSoftObjectPtr<class UNPCActivitySchedule>>& ActivitySchedules);
	virtual void ApplyActivitySchedules_Implementation(const TArray<TSoftObjectPtr<class UNPCActivitySchedule>>& ActivitySchedules);

	//Called when our dialogue is set 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void ApplyDialogue(TSubclassOf<class UDialogue> NPCDialogue);
	virtual void ApplyDialogue_Implementation(TSubclassOf<class UDialogue> NPCDialogue);
	
protected:

	//In order to link NPCs and their AIController save records, we have NPCs store their AIC record. That works nicely. 
	UPROPERTY(SaveGame)
	FNarrativeActorRecord AICRecord;

	/** The NPCs data asset - this is set automatically by the NPC subsystem when it detects the NPC has spawned in */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing=OnRep_NPCDefinition, Category = "NPC")
	TObjectPtr<class UNPCDefinition> NPCDefinition;

	/** Owning spawn info. It is marked as savegame, and you can modify it whenever you like for example for changing an NPCs spawn.  */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame, Category = "NPC")
	FNPCSpawnInfo SpawnInfo;

	/** This NPCs level - NPCs levels are defined by their NPC asset */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame, Replicated, Category = "NPC")
	int32 NPCLevel;

	/** This NPCs faction - can be changed at anytime and will be saved to disk */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame, ReplicatedUsing=OnRep_NPCFactions, Category = "NPC")
	FGameplayTagContainer NPCFactions;

	/** If true, this NPC will start attacking anyone that causes damage to them, provided they aren't in the same faction. */
	UPROPERTY(BlueprintReadOnly, Category = "NPC")
	bool bAggressiveOnTakeDamage;

	/** If true, this NPC will start attacking anyone that causes damage to them, provided they aren't in the same faction. */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "NPC")
	TArray<FTaggedDialogue> TaggedDialogues;

	/** We consider anything in this list a "hostile override" - that is to say even if the actor is neutral or even friendly, we'll treat them as a hostile and attack.
	We need this because there are occasions where we want friendlies to attack each other, maybe during a quest. We also often want to attack neutrals that have attacked us, 
	but by default their attitude will be neutral. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "NPC")
	TArray<TObjectPtr<AActor>> Hostiles;

	UFUNCTION()
	virtual void OnRep_NPCDefinition();

	UFUNCTION(BlueprintImplementableEvent)
	void NPCDataReady();

	/**Allows a chance to let us be hostile towards any perceived target if you return true, even if they aren't in a hostile faction. 
	This would let you do things like have friendly NPCs turn on the player if you've damaged them enough, etc. */
	UFUNCTION(BlueprintNativeEvent, Category = "Hostiles")
	bool ShouldBeAggressiveTowardsTarget(const class AActor* Target) const;
	virtual bool ShouldBeAggressiveTowardsTarget_Implementation(const class AActor* Target) const;

public:


	//Get the npc definition from the character
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	FORCEINLINE class UNPCDefinition* GetNPCDefinition() const {return NPCDefinition;};

	//Grab the NPCs name
	UFUNCTION(BlueprintPure, Category = "NPC")
	FText GetNPCName() const;

	FText GetCharacterName() const override; 

	virtual void SetNPCDefinition(class UNPCDefinition* NPCData);
	  
	//Notify the NPC a tagged dialogue should try play, like a greeting, farewell, taunt, etc. 
	UFUNCTION(BlueprintCallable, Category = "NPC")
	void PlayTaggedDialogue(UPARAM(meta = (Categories = "Narrative.TaggedDialogue")) FGameplayTag Tag, AActor* DialogueInstigator);

	//Notify the NPC a tagged dialogue should try play - this is blueprint implementable 
	UFUNCTION(BlueprintImplementableEvent, Category = "NPC")
	void ExecutePlayTaggedDialogue(FTaggedDialogue Dialogue, AActor* DialogueInstigator);

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class UNPCActivityComponent* GetActivityComponent() const;

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	ANarrativeNPCController* GetNPCController() const;

	virtual AController* GetOwningController() const override;

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UNarrativeInventoryComponent* GetTradingInventoryComponent() const {return TradingInventoryComponent;};

protected:

	/** Required to grab the NPCController even if its not current possessing us, say because we're in a vehicle*/
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "NPC")
	ANarrativeNPCController* CachedController;

	UFUNCTION()
	virtual void OnRep_NPCFactions();
};
