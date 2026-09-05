// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include <GameplayEffectTypes.h>
#include "UnrealFramework/NarrativeTeamAgentInterface.h"
#include <Animation/AnimInstance.h>
#include <GameplayTagAssetInterface.h>
#include "GAS/NarrativeCombatAbility.h"
#include "NarrativeSavableActor.h"
#include "GAS/AttackComboAnimSet.h"
#include "CharacterCreator/CharacterCreatorAttributes.h"
#include "Interaction/InteractableComponent.h"
#include "Items/InventoryComponent.h"
#include <Engine/StreamableManager.h>
#include "Vehicles/NarrativeImpactInterface.h"

#include "Perception/AISightTargetInterface.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "TimerManager.h"
#include "CollisionQueryParams.h"
#include "NarrativeCharacter.generated.h"

struct FCollisionQueryParams;
class UChooserTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNarrativeCharacterEvent, class ANarrativeCharacter*, Character);

UENUM(BlueprintType)
enum class ETraversalActionType : uint8
{
	None,
	Hurdle,
	Mantle,
	Vault,
	Climb,
	ExitClimb
};

//Taken from GASP - this will be removed/refactored when NPro goes to Mover. 
USTRUCT(BlueprintType)
struct FAttachWarpProps : public FTableRowBase
{
	GENERATED_BODY()
	
public:

	FAttachWarpProps();

	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	TObjectPtr<class AActor> ClimbableActor;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	FTransform LedgeTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	FTransform CurrentLedgeTransform = FTransform::Identity;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	FVector BackLedgeLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	FVector BackFloorLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	UAnimMontage* SelectedMontage = nullptr;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	TEnumAsByte<EMovementMode> CurrentMovementMode = MOVE_Walking;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	TEnumAsByte<EMovementMode> NewMovementMode = MOVE_Walking;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float YawRotationToLedge = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float PlayRate = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float StartTime = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float Speed = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float ObstacleHeight = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float ObstacleDepth = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float BackLedgeHeight = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float OverrideLayerBlendIn = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float OverrideLayerBlendOut = 0.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	float OptionalBlendInTime = -1.f;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	bool bPressedJump = false;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	bool IsClimbableObject = false;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	bool HasFrontLedge = false;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	bool HasBackLedge = false;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	bool HasBackFloor = false;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	bool bIsCameraInsideHead = false;
	UPROPERTY(BlueprintReadWrite, Category = "Traversal Props")
	ETraversalActionType ActionType = ETraversalActionType::None;


	static void ClearProps(FAttachWarpProps& InAttachWarpProps)
	{
		InAttachWarpProps.LedgeTransform = FTransform::Identity;
		InAttachWarpProps.BackLedgeLocation = FVector::ZeroVector;
		InAttachWarpProps.BackFloorLocation = FVector::ZeroVector;
		InAttachWarpProps.SelectedMontage = nullptr;
		InAttachWarpProps.CurrentMovementMode = MOVE_Walking;
		InAttachWarpProps.NewMovementMode = MOVE_None;
		InAttachWarpProps.YawRotationToLedge = 0.f;
		InAttachWarpProps.PlayRate = 1.f;
		InAttachWarpProps.StartTime = 0.f;
		InAttachWarpProps.Speed = 0.f;
		InAttachWarpProps.ObstacleHeight = 0.f;
		InAttachWarpProps.ObstacleDepth = 0.f;
		InAttachWarpProps.BackLedgeHeight = 0.f;
		InAttachWarpProps.OverrideLayerBlendIn = 0.f;
		InAttachWarpProps.OverrideLayerBlendOut = 0.f;
		InAttachWarpProps.OptionalBlendInTime = -1.f;
		InAttachWarpProps.bPressedJump = false;
		InAttachWarpProps.IsClimbableObject = false;
		InAttachWarpProps.HasFrontLedge = false;
		InAttachWarpProps.HasBackLedge = false;
		InAttachWarpProps.HasBackFloor = false;
	}

};


//In order to replicate weapon wields, we need to track all weapons, along with their target hand. We do so using two tag containers. 
// One for the equipment slots we're wielding from, and one for the target hands the weapons go into. 
// IE if slots[0]=HipL, and slots[1]=HipR, and hands[0]=Mainhand, hands[1]=Offhand, we'd be wielding HipL into our mainhand, and HipR into offhand. 
USTRUCT(BlueprintType)
struct FWeaponWieldState
{
	
	GENERATED_BODY()

	FWeaponWieldState()
	{
		EquipSlots = FGameplayTagContainer();
		EquipWeapons = {};
		WieldSlots = FGameplayTagContainer();
	}

	//The equipment slots we're wielding the weapons from
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Wield State")
	FGameplayTagContainer EquipSlots;

	//The weapons in those EquipSlots - we cannot look these up and must replicate them as clients may not have their EquipmentComponent equips repped back yet. 
	UPROPERTY(BlueprintReadWrite, Category = "Wield State")
	TArray<TObjectPtr<UWeaponItem>> EquipWeapons;
	
	//The target hands to wield the weapons into 
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Wield State")
	FGameplayTagContainer WieldSlots;

	bool IsValidWieldState() const;
};

//Melee abilities use this to operate, both with unarmed attacks and melee weapons. 
USTRUCT(BlueprintType)
struct FMeleeCombatData
{

	GENERATED_BODY()

	FMeleeCombatData()
	{
		TraceData.TraceDistance = 300.f;
		TraceData.TraceRadius = 100.f;
	};

	//Attacking without a weapon will use this trace data 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Combat Data")
	FCombatTraceData TraceData;

	//Combo montages for our melee attacks 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Combat Data")
	TArray<TObjectPtr<UNarrativeAnimSet>> AttackCombos;

	//Combo montages for our heavy melee attacks 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Combat Data")
	TArray<TObjectPtr<UNarrativeAnimSet>> HeavyAttackCombos;

};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFactionUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTeleported);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnASCInitialized);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTraverse, const FAttachWarpProps&, TraversalProps);



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCharacterJumped);

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class NARRATIVEARSENAL_API UNarrativeCharacterOwner : public UInterface
{
	GENERATED_UINTERFACE_BODY()
	
};

//Lots of things can be associated with a NarrativeCharacter - NPC/Player controllers, character visuals, weapon visuals, and so on. This interface provides a nice way for code to grab the narrativecharacter associated with the object. 
class NARRATIVEARSENAL_API INarrativeCharacterOwner
{
	GENERATED_IINTERFACE_BODY()

public:

	//Return the Narrative Character this controlled is associated with, even if we aren't currently possessing it 
	UFUNCTION(BlueprintCallable, Category = "Narrative Character", meta = (BlueprintThreadSafe))
	virtual class ANarrativeCharacter* GetNarrativeCharacter() const = 0;

};

/**UE's capsule rotation settings are super messy. There is bUseControllerRotationYaw. OrientToMovement, and a seperate controller rotation yaw value on the CMC.
 *
 * Since none of these can be enabled at the same time anyway its much easier to clean these up into a nice simple enum that defines what the capsule should do 
 */
UENUM(BlueprintType)
enum class ECapsuleRotationSetting : uint8
{
	//CMC will not rotate or do anything with the capsule. Nice if something else needs to control it. 
	NoRotation,

	//CMC will orient capsule towards movement using CMCs rotation rate. 
	OrientTowardsMovement,

	//bUseControllerRotationYaw will be set to true, meaning capsule snaps directly to Controller Yaw rot. 
	UseControllerYawDirect,

	//Same as above however instead of snapping, the capsules will rotation towards controller yaw rot using CMCs rotation rate. 
	UseControllerYawSmoothed
};

/**Base class for characters built on Narrative Pro framework. Sets up some core stuff you probably want - a navigation marker, interactions, inventories, etc. */
UCLASS()
class NARRATIVEARSENAL_API ANarrativeCharacter : public ACharacter, 
public IAbilitySystemInterface, public IGameplayTagAssetInterface, public INarrativeTeamAgentInterface, public IAISightTargetInterface, public INarrativeCharacterOwner, public INarrativeImpactInterface
{
	GENERATED_BODY()

public:

	friend class ANarrativeCharacterVisual;

	// Sets default values for this character's properties
	ANarrativeCharacter(const class FObjectInitializer& ObjectInitializer);

	virtual class ANarrativeCharacter* GetNarrativeCharacter() const override;
	
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;
	virtual bool IsMoveInputIgnored() const override;
	virtual void Destroyed() override;
	virtual void FellOutOfWorld(const class UDamageType& dmgType) override;
	virtual void Landed(const FHitResult& Hit) override; 
	virtual void OnJumped_Implementation() override; 
	
	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FCharacterJumped OnJumpedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Appearance")
	FNarrativeCharacterEvent CharacterVisualInitialized;
	
	#if WITH_GAMEPLAY_DEBUGGER
	virtual void DescribeSelfToGameplayDebugger(FGameplayDebuggerCategory* DebuggerCategory) const;
#endif // WITH_GAMEPLAY_DEBUGGER

	// Interfaces
	virtual void HandleVehicleImpact_Implementation(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "ASC")
	class UNarrativeAbilitySystemComponent* GetNarrativeAbilitySystemComponent() const;

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;
	virtual bool HasMatchingGameplayTag(FGameplayTag TagToCheck) const override;
	virtual bool HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;
	virtual bool HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const override;

	virtual UAISense_Sight::EVisibilityResult CanBeSeenFrom(const FCanBeSeenFromContext& Context, FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed, int32& OutNumberOfAsyncLosCheckRequested, float& OutSightStrength, int32* UserData = nullptr, const FOnPendingVisibilityQueryProcessedDelegate* Delegate = nullptr) override;
	bool PerformSightTrace(FHitResult& Hit, const FVector& Start, const FVector& End, const FCanBeSeenFromContext& Context, float& OutSightStrength, FVector& OutSeenLocation, int32& OutNumberOfLoSChecksPerformed);

	/*Checks if we have a weapon, are in first person, etc and updates UseControllerRotationYaw and Orient Rotation to movement accordingly.
	 */  
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	ECapsuleRotationSetting GetCapsuleRotationSettings() const;
	virtual ECapsuleRotationSetting GetCapsuleRotationSettings_Implementation() const;

	//Calculate how strongly we can be seen by the Looker - Start is looker eyes pos, end is our body/head depending on which is being checked. 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	float CalcSightStrength(const FVector& Start, const FVector& End, const AActor* Looker);
	virtual float CalcSightStrength_Implementation(const FVector& Start, const FVector& End, const AActor* Looker);

	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Attributes")
	virtual bool IsAlive() const;

	//Used by AnimBP to ask where the head bone should look at - players and bots can then implement their own seperate functionalities 
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	FVector GetHeadLookAtLocation(bool& bOutWantsLookAt) const;

	//Get the location of the floor, minus 2 units on Z, this is our root mesh bone location 
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Movement")
	FVector GetRootBoneLocation() const;

	//Get the location of the floor, optionally offset by a z
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Movement")
	FVector GetFloorLocation(const float ZOffset=0.f) const;

	//Called when this character becomes a dialogue avatar. 
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	void OnEnterDialogue(class UDialogue* Dialogue);

	//Called when this character finishes being a dialogue avatar. 
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	void OnEndDialogue(class UDialogue* Dialogue);

	//Get the character definition from the character
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	virtual class UCharacterDefinition* GetCharacterDefinition() const;

	//Get the inventory component from this character
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	class UNarrativeInventoryComponent* GetInventoryComponent() const;

	//Get the interactioncomponent from the character
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	virtual class UNarrativeInteractionComponent* GetInteractionComponent() const;

	//Return the owning controller of this NarrativeCharacter, even if it is not possessing us right now 
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	virtual AController* GetOwningController() const;

	//This is called when the NarrativeCharacters definition is updated. 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void OnDefinitionSet(UCharacterDefinition* NewDefinition);
	virtual void OnDefinitionSet_Implementation(UCharacterDefinition* NewDefinition);

	//Called when OnDefinitionSet sees we dont have any save data and new initialized for first time. 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void InitNewCharacter(UCharacterDefinition* NewDefinition);
	virtual void InitNewCharacter_Implementation(UCharacterDefinition* NewDefinition);

	bool bInitializedNewCharacter = false; 

	//Executes a NarrativeEvent with this character as the target - return true if we passed conds and event succeeded 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeEvents")
	bool SetEventActive(class UNarrativeEvent* Event, const bool bActivate);

	/*Return whether camera is inside the head - essentially, whether we're in First Person. We can be in first person mode
	but not fully transitioned into it yet, hence the naming being specific about what this is doing*/
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	virtual bool IsCameraInsideHead() const;

	// returns true when the character has the Narrative.State.Movement.Lock tag.
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	bool IsMovementLocked() const;

protected:

	virtual void BeginPlay() override; 
	virtual void Tick(float DeltaSeconds) override; 
	virtual void PostInitializeComponents() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TeleportSucceeded(bool bIsATest) override;

public:

	//Factions are getting a little messy - possibly fold this into a FactionComponent? 
	FOnFactionUpdated OnFactionUpdated;

	//Broadcast when we telport somewhere - NPCs need this to teleport with our player if they do 
	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnTeleported OnTeleported;

	//Broadcast when ASC has been initialized 
	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnASCInitialized OnASCInitialized;
	
	//Broadcast when a traversal has occured  
	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnTraverse OnStartTraversal;

protected: 

	bool RegisterCharacterMapMarker();
	
	//Whether our character needs a map marker
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Appearance")
	bool bWantsMapMarker;

	//Our characters map marker
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Appearance")
	TObjectPtr<class UCharacterMapMarker> MapMarker; 

	//Our characters current appearance asset
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Appearance")
	TObjectPtr<class UCharacterAppearanceBase> Appearance;

	/** Our characters visual - this allows us to have n number of meshes in addition to Character.GetMesh() */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing=OnRep_CharVisual, Category = "Narrative|Components|Body Meshes")
	TObjectPtr<class ANarrativeCharacterVisual> CharVisual;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UMotionWarpingComponent> MotionWarpingComponent;

	//Our characters inventory component. This is lightweight and won't effect performance if you don't use it. 
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UNarrativeInventoryComponent> InventoryComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components|GAS")
	TObjectPtr<class UNarrativeAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UNarrativeAttributeSetBase> AttributeSetBase;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Components")
	TObjectPtr<class UEquipmentComponent> EquipmentComp;

	// Default attributes for a character for initializing on spawn/respawn.
	// This is an instant GE that overrides the values for attributes that get reset on spawn/respawn.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Abilities")
	TSubclassOf<class UGameplayEffect> DefaultAttributes;

	// These effects are only applied one time on startup
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Abilities")
	TArray<TSubclassOf<class UGameplayEffect>> StartupEffects;

	// Default abilities to grant the player  
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Narrative|Abilities")
	TArray<TSubclassOf<class UNarrativeGameplayAbility>> DefaultAbilities;

	//When we level up, do we want to re-apply the base attribute set? This will cause our attributes to scale. 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Abilities")
	bool bReapplyAttributesOnLevelUp;
	
	//Lowering this number means the base XP required per level is higher 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Abilities")
	float LevelExponentX; 

	//Upping this value means higher jumps between levels, ie levels will grow exponentially 
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Narrative|Abilities")
	float LevelExponentY; 

	/** Our currently equipped weapon */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponItem> EquippedWeapon;

	/**Our currently wielded weapons, replicated as a single variable so equips can be processed in one go - important for dual wielding.  */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_WieldState, BlueprintReadOnly, Category = "Weapon")
	FWeaponWieldState WieldState;

	/** Our wield state from our last save game - we use this to restore wields.  */
	UPROPERTY(SaveGame)
	FWeaponWieldState SavedWieldState;

	/** Whether or not we're ragdolling. TODO consider making this a RagdollState struct which contains additional info like whether we want to play a GetUp anim etc.  */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_bIsRagdoll, BlueprintReadOnly, Category = "Narrative Character")
	bool bIsRagdoll;

	/** List of primitives to ignore whilst moving. UE doesn't replicate this list by default, but we need it replicated so 
	simulated proxies can replay moves and not get fighting. Can be removed if we fully move to ContextualSceneAnims. */
	UPROPERTY(VisibleAnywhere, ReplicatedUsing=OnRep_ReplicatedMoveIgnoreActors, BlueprintReadOnly, Category = "Narrative Character")
	TArray<TObjectPtr<AActor>> ReplicatedMoveIgnoreActors;

	/**This characters random seed, generated once and synced on client-server. Can be used for anything this character needs. 
	When we do networking we'll need to add a check to see if this is replicated yet */
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly, SaveGame, Category = "Narrative Character")
	int32 CharacterRandomSeed;

	// Set our ASC pointer, as well as initializing the avatar info. 
	//virtual void SetupAbilitySystemComponent(class UNarrativeAbilitySystemComponent* ASC);

	// Grant abilities on the Server. The Ability Specs will be replicated to the owning client.
	virtual void AddDefaultAbilities();

	// Removes all CharacterAbilities. Can only be called by the Server. Removing on the Server will remove from Client too.
	virtual void RemoveCharacterAbilities();

	// Initialize the Character's attributes. Must run on Server but we run it on Client too
	// so that we don't have to wait. The Server's replication to the Client won't matter since
	// the values should be the same.
	virtual void InitializeAttributes();
	virtual void AddStartupEffects();

	/**
	* Setters for Attributes. Only use these in special cases like Respawning, otherwise use a GE to change Attributes.
	* These change the Attribute's Base Value.
	*/
	virtual void SetHealth(float Health);
	virtual void SetStamina(float Stamina);

public:

	//Called whenever we gain XP 
	virtual void OnXPChanged(const float OldXP, const float NewXP);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NarrativeCharacter")
	void OnLevelUp(const int32 NewLevel);
	virtual void OnLevelUp_Implementation(const int32 NewLevel);
	
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	int32 XPToLevel(const float XP) const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float LevelToXP(const int32 Level) const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetPercentToNextLevel() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	virtual int32 GetCharacterLevel() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetXP() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetStealthRating() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetStamina() const;

	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Attributes")
	float GetMaxStamina() const;
protected:
	
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	virtual void HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead);
	/** Project recovery can restore resources explicitly without resetting a persistent ASC's attributes or startup grants. */
	virtual bool ShouldResetAttributesOnRevive() const { return true; }

	//Called when our spawned data bundle is loaded 
	UFUNCTION()
	virtual void OnCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId);

	//Called after OnCharacterDefinitionDataLoaded, for anything that needs to be done after definition data is applied
	UFUNCTION()
	virtual void OnPostCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId);

	//Called once all our character definition stuff has been applied/loaded from disk. IE default items, factions, etc. 
	UFUNCTION()
	virtual void HandleCharacterDefinitionDataLoaded(FPrimaryAssetId LoadedId);

	//Check if our character is waiting to be loaded in - default loading screen uses this. 
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter")
	virtual bool IsCharacterPendingLoad() const;

	TSharedPtr<FStreamableHandle> CharacterDefinitionLoadHandle; 

	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void SpawnCharacterVisual(class UCharacterAppearance* DefaultAppearance);
	virtual void SpawnCharacterVisual_Implementation(class UCharacterAppearance* DefaultAppearance);

	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	TSubclassOf<class ANarrativeCharacterVisual> GetCharacterVisualClass(class UCharacterAppearance* DefaultAppearance)const;
	virtual TSubclassOf<class ANarrativeCharacterVisual> GetCharacterVisualClass_Implementation(class UCharacterAppearance* DefaultAppearance) const;

	UFUNCTION()
	virtual void OnRep_CharVisual();

	UFUNCTION()
	virtual void OnCharacterVisualInitialized();

	//Change appearance - different from apply appearance in that you can call this at runtime to dynamically change appearance! 
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	void ChangeAppearance(class UCharacterAppearance* DefaultAppearance);
	virtual void ChangeAppearance_Implementation(class UCharacterAppearance* DefaultAppearance);

	//Apply an appearance to the spawned character visual 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void ApplyAppearance(class UCharacterAppearance* DefaultAppearance);
	virtual void ApplyAppearance_Implementation(class UCharacterAppearance* DefaultAppearance);

	//Called when our default trigger sets are ready 
	UFUNCTION(BlueprintNativeEvent, Category = "Narrative|NarrativeCharacter")
	void ApplyTriggerSets(const TArray<class UTriggerSet*>& DefaultSet);
	virtual void ApplyTriggerSets_Implementation(const TArray<class UTriggerSet*>& DefaultSet);

	virtual TArray<FLootTableRoll> GetDefaultItemLoadout() const;
	virtual TSoftObjectPtr<UCharacterAppearanceBase> GetDefaultAppearance() const; 
	virtual TArray<TSoftObjectPtr<class UTriggerSet>> GetDefaultTriggerSets() const;

	//Add a trigger by copying the passed in template object 
	UFUNCTION(BlueprintCallable, Category = "Triggers")
	virtual class UNarrativeTrigger* AddTrigger(class UNarrativeTrigger* Template);

	UFUNCTION(BlueprintCallable, Category = "Triggers")
	virtual bool RemoveTrigger(class UNarrativeTrigger* Trigger);

public:

	/**Add an ability, and return the spec handle. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	virtual FGameplayAbilitySpecHandle AddAbility(TSubclassOf<class UNarrativeGameplayAbility> Ability, UObject* SourceObject=nullptr);

	//Add abilities, overriding any of the default ones our ability set has granted us 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	virtual TArray<FGameplayAbilitySpecHandle> GrantAbilities(TArray<TSubclassOf<class UNarrativeGameplayAbility>> Abilities, UObject* SourceObject = nullptr);

	//Remove abilities, and add any default ones back
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter")
	virtual void RemoveAbilities(TArray<FGameplayAbilitySpecHandle> Abilities);

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE int32 GetCharacterRandomSeed() const { return CharacterRandomSeed; };

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class ANarrativeCharacterVisual* GetCharacterVisual() const { return CharVisual;};

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UCharacterMapMarker* GetMarkerComponent() const { return MapMarker;};

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UEquipmentComponent* GetEquipmentComponent() const {return EquipmentComp;};

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UMotionWarpingComponent* GetMotionWarpingComponent() const {return MotionWarpingComponent;};

	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE class UNarrativeAttributeSetBase* GetAttributeSetBase() const {return AttributeSetBase;};

	//Get the Narrative character movement component. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class UNarrativeCharacterMovement* GetNarrativeCharacterMovement() const;

	//Set the characters random seed, useful if you need to override the default one that gets assigned.
	void SetRandomSeed(const int32 NewSeed);
	
	//Set our wielded weapons 
	UFUNCTION(BlueprintCallable, Category = "Narrative|Getters/Setters")
	void SetWieldState(const FWeaponWieldState& NewWieldState);
	uint64 GetWeaponWieldRevision() const { return WeaponWieldRevision; }

	//WieldState can rep back to client, but several things need to be valid before we 
	virtual bool CanApplyWieldState() const;
	
	UFUNCTION()
	virtual void OnRep_WieldState(const FWeaponWieldState& OldWieldState);

	//Returns our characters current wield state. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	FORCEINLINE FWeaponWieldState GetWeaponWieldState() const {return WieldState;};

private:
	uint64 WeaponWieldRevision = 0;
public:

	//Returns our characters narrative anim instance, which should always be on Char mesh. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class UNarrativeAnimInstance* GetCharacterAnimInstance() const;

	//Returns the visual of the equipped weapon 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class AWeaponVisual* GetEquippedWeaponVisual() const;

	//Returns the visual of the weapon in the given wield slot
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class AWeaponVisual* GetWieldedWeaponVisual(const bool bMainhand = true) const;

	//Returns our weapon visual. This just a generic actor that each weapon item defines, and holds the weapons static mesh and FX assets. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class AWeaponVisual* GetWeaponVisual(UPARAM(meta = (Categories = "Narrative.Equipment.Slot.Weapon"))const FGameplayTag& WeaponSlot) const;

	//Returns our equipped weapon item. This is the item in our inventory driving our weapon, it holds data like the weapons abilities, spread, damage etc.
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	class UWeaponItem* GetWeapon(const bool bMainhand=true) const;

	//Return all wielded weapons. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	TArray<class UWeaponItem*> GetWieldedWeapons() const;

	//Returns our characters name. 
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	virtual FText GetCharacterName() const;

	/** TODO move these into a proper asset that lives on CharacterDefinition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events & Conditions")
	TArray<class UNarrativeTrigger*> Triggers;

	//Get the range we can attack someone from
	UFUNCTION(BlueprintPure, Category = "Narrative|Getters/Setters")
	float GetAttackRange() const;

	//Enter ragdoll. Replicates to other clients. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Movement")
	virtual void SetRagdoll(const bool bWantsRagdoll);

	//Ragdoll for the set amount of time. Useful for effects like getting knocked down, etc. Calling multiple times restarts the duration. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Movement")
	virtual void RagdollForDuration(const float Duration);
	
	//Ragdoll with damage and an impulse. Useful for impacts from things like cars where being hit should send us flying and deal some damage
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Movement")
	virtual void RagdollWithDamageAndImpulse(const float Duration, const FVector& Impulse, const float Damage);

	UFUNCTION(Server, Reliable)
	virtual void ServerStartRagdoll(const bool bWantsRagdoll);

	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Movement")
	virtual bool CanRagdoll() const;

	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Movement")
	virtual bool CanExitRagdoll() const;

protected:

	FTimerHandle RagdollTimerHandle; 

	//Ragdoll for the set amount of time. Useful for effects like getting knocked down, etc. 
	UFUNCTION()
	virtual void GetUpFromTimedRagdoll();

	UFUNCTION()
	virtual void OnRep_bIsRagdoll();

public:
	
	//Check if we're in ragdoll - bCheckGettingUp makes function return true if we're not ragdolling, but are getting up from a ragdoll. 
	UFUNCTION(BlueprintPure, Category = "Narrative|NarrativeCharacter|Movement")
	virtual bool IsRagdoll(const bool bCheckGettingUp=true) const;

public:

	//Add a move ignore actor for our capsule. Replicated to simulated proxies. We can remove this if/when we move to ContextualAnims, as the CAComp handles replicating this. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|NarrativeCharacter|Movement")
	virtual void SetIgnoreActorWhenMoving(AActor* IgnoreActor, const bool bShouldIgnore);

	//Helper function to make some query params setup to ignore our character, and anything attached to it. 
	FCollisionQueryParams GetIgnoreCharacterParams() const;


protected:

	UFUNCTION()
	virtual void OnRep_ReplicatedMoveIgnoreActors();

public: 

	/** Allows us to define how our wielder should rotate with this item equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item - Weapon")
	ECapsuleRotationSetting DefaultCapsuleRotationSetting;
	
	UPROPERTY()
	TArray<UObject*> TraversalMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Traversal")
	UChooserTable* TraversalTable;

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	FAttachWarpProps AttachWarpProps;

	//Set OptionalInBlendTime to -1 if you want to use the default montage blend times  
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	bool TryAttachWarp(bool PressedJump, FVector2D InputVector, float OptionalInBlendTime);

	//Set OptionalInBlendTime to -1 if you want to use the default montage blend times  
	UFUNCTION(BlueprintCallable, Category = "Traversal")
	void PlayAttachWarp(const FAttachWarpProps& InAttachWarpProps);

	UFUNCTION(Server, Reliable, Category = "Traversal")
	void ServerPlayAttachWarp(FAttachWarpProps InTraversalProps);

	UFUNCTION(NetMulticast, Reliable, Category = "Traversal")
	void MultiCastPlayAttachWarp(FAttachWarpProps InTraversalProps);

	UPROPERTY(BlueprintReadOnly, Category = "Traversal")
	bool IsPlayingAttachWarpMontage;


};
