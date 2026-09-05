// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "GAS/NarrativeGameplayAbility.h"
#include "Engine/NetSerialization.h"
#include "NarrativeActorProvider.h"
#include "NarrativeCombatAbility.generated.h"

/** Automatic preserves existing range metadata; lunges and hybrid attacks can declare their real pressure class. */
UENUM(BlueprintType)
enum class ESovBotAttackPressure : uint8 { Automatic, Melee, Ranged, Support };

//Stored on both weapons and our player for unarmed combat. Replaces the need for expensive targeting actors, GAs just generate target data themselves
USTRUCT(BlueprintType)
struct FCombatTraceData
{

	GENERATED_BODY()

	FCombatTraceData()
	{
		TraceDistance = 500.f;
		TraceRadius = 0.f;
		bTraceMulti = false;
	};

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Combat Trace Data")
	float TraceDistance;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Combat Trace Data")
	float TraceRadius;

	//True if we want to trace multi instead of single 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Combat Trace Data")
	bool bTraceMulti;
};

/**
 * Our targeting transform uses instanced objects so we can de-couple targeting behavior from the combat ability, since we need more of a composition based approach rather than inheritance based for GAS Combat abilities. 
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, AutoExpandCategories = ("Targeting Provider"), meta = (DisplayName = "Targeting Transform Provider"))
class NARRATIVEARSENAL_API UTargetingTransformProvider : public UObject
{
	GENERATED_BODY()
	
public:

	 UTargetingTransformProvider();

	//Provide a targeting transform that combat gameplay abilities can use to specify targeting without needing lots of code. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Provider")
	FTransform ProvideTargetingTransform(const AController* Controller) const;
	virtual FTransform ProvideTargetingTransform_Implementation(const AController* Controller) const;

};

/**
 * Targeting transform that uses the camera as the starting point, looking forwards our focal point. 
 */
UCLASS(EditInlineNew, AutoExpandCategories = ("Targeting Provider"), meta = (DisplayName = "Camera Towards Focus"))
class NARRATIVEARSENAL_API UTargetingTransformProvider_CameraTowardsFocus : public UTargetingTransformProvider
{
	GENERATED_BODY()
	
public:

	 UTargetingTransformProvider_CameraTowardsFocus();

	 virtual FTransform ProvideTargetingTransform_Implementation(const AController* Controller) const override;
};

/**
 * Targeting transform that uses the weapon as the starting point, looking forwards our focal point. 
 */
UCLASS(EditInlineNew, AutoExpandCategories = ("Targeting Provider"), meta = (DisplayName = "Weapon Towards Focus"))
class NARRATIVEARSENAL_API UTargetingTransformProvider_WeaponTowardsFocus : public UTargetingTransformProvider
{
	GENERATED_BODY()
	
public:

	 UTargetingTransformProvider_WeaponTowardsFocus();

	 virtual FTransform ProvideTargetingTransform_Implementation(const AController* Controller) const override;

	//Whether to use the weapon visual, or a socket on our character mesh instead. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "TargetingTransformProvider")
	bool bUseWeaponVisualMesh;

	 //The bone on the weapon visual/character to use. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="TargetingTransformProvider")
	FName BoneName;

	/*Defines an amount that the project will be pushed forward from the bone, in order to prevent projectiles impacting the person/weapon shooting them.
	We default this to zero now as we no longer push forward as we instead solve owner collisions by making projectiles ignore owners for a short time, however this can be used if required.  */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "TargetingTransformProvider")
	float ForwardPushDist;
};

//Allows blueprints to easily define a targeting transform by selecting the one they want in a dropdown. 
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FInstancedTargetingTransform
{
	GENERATED_BODY()

	FInstancedTargetingTransform(){};

	//The instanced goal
	UPROPERTY(Instanced, EditAnywhere, BlueprintReadOnly, Category = "Provider")
	TObjectPtr<UTargetingTransformProvider> Provider; 

};


/**
 * Ability that has all of the hitscan, collision checking, damage dealing etc built in. Used by both melee and hitscan weapons. 
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeCombatAbility : public UNarrativeGameplayAbility
{
	GENERATED_BODY()
	
protected:
	
	UNarrativeCombatAbility();

	virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	FGuid CurrentCombatAttackId;
	FGameplayAbilitySpecHandle CombatAttackSpecHandle;
	bool bChargedReleaseCommitted = false;
	bool bDefensiveCancelCommitted = false;
	bool bExertionCommitPending = false;
	bool bCombatEndPending = false;
	bool TryPayAttackExertion(float Cost);

public:
	/** Receipt factories capture this identity; the mutable ability itself is deliberately not a receipt. */
	bool GetSovAttackIdentity(const AActor* ExpectedSource, FGuid& OutAttackId) const;

	/** Call only after release geometry and authored charge-tier admission succeed. One release per activation. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Combat|Exertion")
	bool TryCommitChargedRelease(int32 ReleasedChargeTier);

	/** Caller first validates its authored defensive cancel node/window. Does not cancel the ability itself. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Combat|Exertion")
	bool TryCommitDefensiveCancel(float StaminaCost);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Combat|Exertion")
	bool IsChargedReleaseCommitted() const { return bChargedReleaseCommitted; }

	/** Admission shared by target-data and native direct-damage payloads. */
	bool CanDispatchNativeAttack() const;
	/** Exact GAS activation-owned contribution, for read-only defensive-cancel admission. */
	bool OwnsCombatActivationTag(FGameplayTag Tag) const { return IsActive() && ActivationOwnedTags.HasTagExact(Tag); }

	/** Opt in for charged melee nodes; ordinary attacks and existing Echo payloads remain unchanged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat|Exertion")
	bool bRequiresChargedRelease = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat|Exertion", meta = (ClampMin = "0"))
	int32 MinimumStaminaChargeTier = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Combat|Exertion", meta = (ClampMin = "0"))
	float ChargedReleaseStaminaCost = 20.f;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const;

protected:
	/** Renew the receipt after a validated finite combo-node transition, without a second GAS activation. */
	bool BeginNextSovCombatAttack();

	UFUNCTION(BlueprintPure, Category = "Combat")
	virtual bool HasAmmo() const;

	// Generate some target data using a trace. HitTarget Attack Event will be called by default if the trace hits something. 
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void GenerateTargetDataUsingTrace(const FCombatTraceData& TraceData, const FTransform& TraceStart, FGameplayTag ApplicationTag = FGameplayTag());

	// Get target data using a trace - doesn't do anything with it, up to caller. 
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual	FGameplayAbilityTargetDataHandle GetTargetDataUsingTrace(const FCombatTraceData& TraceData, const FTransform& TraceStart);

	//After targeting data is generated, this actually sends the target data to server and calls delegates etc. 
	//TODO need to code a server-validation layer into this for networking 
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void FinalizeTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);

	UFUNCTION(BlueprintNativeEvent, DisplayName = "OnTargetDataReady", Category = "Attack Events")
	void HandleTargetData(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);
	virtual void HandleTargetData_Implementation(const FGameplayAbilityTargetDataHandle& TargetData, FGameplayTag ApplicationTag);

	//Perform the actual weapon trace. 
	FHitResult PerformTrace(const FVector& Start, const FVector& End, const float SweepRadius);
	TArray<FHitResult> PerformTraceMulti(const FVector& Start, const FVector& End, const float SweepRadius);

	//Basically just a generic default damage amount for the ability to deal, doesn't have to be used. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability")
	float GetAttackDamage() const;
	virtual float GetAttackDamage_Implementation() const;

	//Apply spread to the given targeting viewpoint. Will use the weapons current spread. Use ApplySpread if you want to specify the spread amount. 
	UFUNCTION(BlueprintPure, Category = "Narrative Combat Ability")
	virtual FTransform ApplyWeaponSpread(const FTransform& ViewPoint) const;

	//Apply a fixed amount of spread to the given targeting viewpoint
	UFUNCTION(BlueprintPure, Category = "Narrative Combat Ability")
	virtual FTransform ApplySpread(const FTransform& ViewPoint, const float Spread) const;

	//if true, we check our item has its ammo class, and if not the cost check will fail 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Narrative Ability")
	bool bRequiresAmmo;
	
	/** The amount of attack damage the ability will deal by default when it receives target data. If you need to be a dynamic 
	value you can override GetAttackDamage() or even override HitTarget() to do something different than dealing damage when we hit someone. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
	float DefaultAttackDamage;

	//Get the rate bots should use this attack at. ie sword swipe should return a lower frequency than a fully auto rifle. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
	float DefaultBotAttackFrequency;

	//Get the range bots should be within to use this attack. ie sword swipe should return a lower range than a fully auto rifle. 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
	float DefaultBotAttackRange;

public:

	//Get the rate bots should use this attack at. ie sword swipe should return a lower frequency than a fully auto rifle. Scaled by difficulty multiplier defined in arsenal settings. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability - Bots")
	float GetBotAttackFrequency() const;
	virtual float GetBotAttackFrequency_Implementation() const;

	//Get the range this attack should cover. ie sword swipe should return a lower distance than a rifle. Only used for bots. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability - Bots")
	float GetBotAttackRange() const;
	virtual float GetBotAttackRange_Implementation() const;

	/** Whole-repertoire NPC selection. Disable for reactions, passives or player-only actions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Narrative Combat Ability - Bots")
	bool bBotSelectionEnabled = true;

	/** Higher priority wins; equal-priority ready abilities rotate least recently used first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Narrative Combat Ability - Bots")
	float BotSelectionPriority = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Narrative Combat Ability - Bots")
	bool bBotRequiresLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Narrative Combat Ability - Bots")
	bool bBotRequiresAttackToken = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Narrative Combat Ability - Bots")
	ESovBotAttackPressure BotAttackPressure = ESovBotAttackPressure::Automatic;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability - Bots")
	float GetBotAttackMinimumRange() const;
	virtual float GetBotAttackMinimumRange_Implementation() const;

	/** Actual activation range, distinct from the preferred positioning range. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability - Bots")
	float GetBotAttackMaximumRange() const;
	virtual float GetBotAttackMaximumRange_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability - Bots")
	bool RequiresBotAttackToken() const;
	virtual bool RequiresBotAttackToken_Implementation() const;

	/** True only when the payload already acquires/releases Narrative's token lease itself. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability - Bots")
	bool ManagesBotAttackToken() const;
	virtual bool ManagesBotAttackToken_Implementation() const;

	//Get the weapon hand this combat ability acts on - can be overriden to specify a hand. Not neccesarily used by all combat abilities, some may not need. 
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Narrative Combat Ability")
	bool IsMainhand() const;
	virtual bool IsMainhand_Implementation() const;

	//Get the weapon that granted this ability
	UFUNCTION(BlueprintPure, Category = "Narrative Ability")
	virtual class UWeaponItem* GetAbilityWeapon() const;
	
	//Get the weapon that granted this ability and find its ammo object, if any. 
	UFUNCTION(BlueprintPure, Category = "Narrative Ability")
	virtual class UNarrativeItem* GetAbilityWeaponAmmo() const;

	//Get either the main or offhand weapon visual depending on what IsMainhand() returns. 
	UFUNCTION(BlueprintPure, Category = "Narrative Ability")
	virtual class AWeaponVisual* GetAbilityWeaponVisual() const;

private:

	void OnTargetDataReadyCallback(const FGameplayAbilityTargetDataHandle& InData, FGameplayTag ApplicationTag);

	FDelegateHandle OnTargetDataReadyCallbackDelegateHandle;
};
