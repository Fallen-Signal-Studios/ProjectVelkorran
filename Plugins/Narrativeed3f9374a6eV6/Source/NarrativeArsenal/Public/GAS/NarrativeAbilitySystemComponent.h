// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "NarrativeSavableComponent.h"
#include "GAS/SovCombatTypes.h"
#include "GAS/NarrativeBotAttackSelection.h"
#include "../../../../Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/Abilities/GameplayAbilityTargetTypes.h"
#include "NarrativeAbilitySystemComponent.generated.h"

USTRUCT()
struct FSavedAttribute
{

GENERATED_BODY()

public: 

	FSavedAttribute() 
	{
		Value = 0.f;
	};	

	UPROPERTY(EditDefaultsOnly, SaveGame, Category = "Saving")
	FString AttributeName;

	UPROPERTY(EditDefaultsOnly, SaveGame, Category = "Saving")
	float Value;

};

//An attack token that has been created for a given attacker. Inspired by DOOMs push forward combat setup. 
USTRUCT()
struct FAttackToken
{

GENERATED_BODY()

public: 

	FAttackToken(){};

	FAttackToken(class ANarrativeNPCController* InAttacker, const float InTokenGrantedTime) : Owner(InAttacker), TokenGrantedTime(InTokenGrantedTime) {};

	//The NPC that owns this attack token
	UPROPERTY()
	TObjectPtr<class ANarrativeNPCController> Owner;

	//The game time that the token was granted 
	UPROPERTY()
	float TokenGrantedTime = 0.f; 

};

//Delegates 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDeathStateChanged, AActor*, KilledActor, UNarrativeAbilitySystemComponent*, KilledActorASC, const bool, bIsDead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealedBy, UNarrativeAbilitySystemComponent*, Healer, const float, Amount, const FGameplayEffectSpec&, Spec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDamagedBy, UNarrativeAbilitySystemComponent*, DamagerCauserASC, const float, Damage, const FGameplayEffectSpec&, Spec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDealtDamage, UNarrativeAbilitySystemComponent*, DamagedASC, const float, Damage, const FGameplayEffectSpec&, Spec);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeASCReadyEpochChanged, int32, ReadyEpoch);

/**
 * Custom Ability system component for Narrative pro. Has ISavableComponent for saving attributes.
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeAbilitySystemComponent : public UAbilitySystemComponent, public INarrativeSavableComponent
{
	GENERATED_BODY()
	
public:

	UNarrativeAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);

	bool bStartupEffectsApplied = false;

	bool bInitializedFromConfig = false;
	
	virtual int32 HandleGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	virtual void ClearActorInfo() override;
	/** Local transaction identity, including an avatar handoff away and back. Not save data. */
	uint64 GetCombatActorInfoEpoch() const { return CombatActorInfoEpoch; }
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec& DamageEffectSpec, float DamageMagnitude);
	virtual void Debug_Internal(struct FAbilitySystemComponentDebugInfo& Info) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//Get the frequency a given attack should fire off at, provided we have one. Used for bots. InputTag must have a combat ability on it. IE Input.Attack, AltAttack, etc. 
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS")
	virtual float GetBotAttackFrequency(FGameplayTag InputTag);

	//Get the range a given attack should cover, Used for bots. InputTag must have a combat ability on it. IE Input.Attack, AltAttack, etc. 
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS")
	virtual float GetBotAttackRange(FGameplayTag InputTag);

	/** Empty input filter enumerates every granted combat ability, including Ability1/2. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS|Bot Combat")
	TArray<FNarrativeBotAttackCandidate> GetBotAttackCandidates(AActor* Target, FGameplayTag InputFilter);

	/** Read-only choice. Activation must still revalidate the exact granted handle. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS|Bot Combat")
	bool SelectBotAttack(AActor* Target, FGameplayTag InputFilter, FNarrativeBotAttackCandidate& OutCandidate);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative|GAS|Bot Combat")
	bool TryActivateBotAttack(AActor* Target, FGameplayAbilitySpecHandle Handle);

	/** Tries ready candidates in ranked order; never broadcasts an input tag. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Narrative|GAS|Bot Combat")
	bool TryActivateBestBotAttack(AActor* Target, FGameplayTag InputFilter, FNarrativeBotAttackCandidate& OutCandidate);

	/** Active attack guard for BT tasks; its own Busy/IsFiring tags are permitted. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS|Bot Combat")
	bool IsBotAttackExecutionValid(AActor* Target, FGameplayAbilitySpecHandle Handle) const;

	/** Target reserved for this exact active attack; null when no selector-owned target exists. */
	AActor* GetBotAttackTarget(FGameplayAbilitySpecHandle Handle) const;

	/** A useful positioning range survives cooldown, LOS and temporary state blocks. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS|Bot Combat")
	float GetBotCombatMovementRange(AActor* Target, FGameplayTag InputFilter);

	//Workaround for attribute changed GEData not containing a valid instigator - we need the instigator so bots know when they receive damage. 
	virtual void HealedBy(UNarrativeAbilitySystemComponent* Healer, const float Amount, const FGameplayEffectSpec& Spec);

	//Workaround for attribute changed GEData not containing a valid instigator - we need the instigator so bots know when they receive damage. 
	virtual void DamagedBy(UNarrativeAbilitySystemComponent* DamageCauser, const float Damage, const FGameplayEffectSpec& Spec);

	//Workaround for attribute changed GEData not containing a valid instigator - we need the instigator so bots know when they receive damage. 
	virtual void DealtDamage(UNarrativeAbilitySystemComponent* DamagedTarget, const float Damage, const FGameplayEffectSpec& Spec);

	/** Typed, ordered Sovereign result. Legacy float delegates remain supported. */
	virtual void DamageResolvedAsTarget(const FSovDamageResult& Result);
	virtual void DamageResolvedAsSource(const FSovDamageResult& Result);

	/**
	 * Broadcasts a structurally complete request to target-owned status
	 * consumers. The method is intentionally authority-neutral so callers with
	 * non-damage delivery paths can share the contract; consumers still enforce
	 * authoritative mutation.
	 */
	virtual void StatusApplicationRequested(const FSovStatusApplicationRequest& Request);

	/** Blueprint bridge for inspecting the raw spec supplied by legacy damage delegates. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Damage", meta = (DisplayName = "Gameplay Effect Spec Has Asset Tag"))
	static bool GameplayEffectSpecHasAssetTag(
		const FGameplayEffectSpec& Spec,
		FGameplayTag AssetTag);

	/** PlayerState-channel readiness fence for attributes and granted abilities. */
	void SetCharacterReadyEpoch(int32 NewReadyEpoch);

	/**
	 * Replaces the definition-owned loose-tag contribution without disturbing
	 * counts contributed by other systems. The applied set lives on the ASC so
	 * a PlayerState-backed ASC does not add the same tags again after respawn.
	 */
	void SetDefinitionOwnedTags(const FGameplayTagContainer& NewDefinitionTags);

	/** Replace/remove the persistent effects contributed by a character definition. */
	void ClearTrackedDefaultAttributesEffect();
	void TrackDefaultAttributesEffect(const FActiveGameplayEffectHandle& EffectHandle);
	void ClearTrackedStartupEffects();
	void TrackStartupEffect(const FActiveGameplayEffectHandle& EffectHandle);

	UFUNCTION(BlueprintPure, Category = "Narrative|Readiness")
	int32 GetCharacterReadyEpoch() const { return CharacterReadyEpoch; }

	UPROPERTY(BlueprintAssignable, Category = "Narrative|Readiness")
	FOnNarrativeASCReadyEpochChanged OnCharacterReadyEpochChanged;
	
	//Get the owning avatar - UE doesn't expose this to BP in base ASC. 
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS")
	class AActor* GetAvatarOwner() const;

	//Get the owning narrative char. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	class ANarrativeCharacter* GetCharacterOwner() const;

	//Called when ability input tags are pressed. BP callable incase BP needs to manually send one of these. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	/** Register one authored finite node window; delays are relative to this call. No ability retry loop. */
	FGuid RegisterCombatInputWindow(class UNarrativeCombatAbility* Ability, const FGameplayTagContainer& AllowedInputs,
		float OpensAfter, float ClosesAfter);
	/** The owning native node consumes the newest fresh semantic press only while its window is open. */
	bool ConsumeCombatInputWindow(class UNarrativeCombatAbility* Ability, FGuid WindowId,
		FGameplayTag& OutInput, bool& bOutStillHeld);
	void ClearCombatInputWindow(class UNarrativeCombatAbility* Ability, FGuid WindowId);
	/** Focus loss, modal UI, load and avatar transitions discard pending intent. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS|Input")
	void ClearCombatInputBuffer();

	/** Optional native encounter gate. Never replaces another live encounter owner. */
	bool SetBotAttackCoordinator(UObject* Coordinator);
	UObject* GetBotAttackCoordinator() const { return BotAttackCoordinator.Get(); }

	void ClearAbilitiesWithTag(const FGameplayTag& InputTag);

	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS", meta=(AutoCreateRefTerm="InputTag"))
	void FindAbilitiesWithTag(UPARAM(meta = (Categories="Narrative.Input")) const FGameplayTag& InputTag, TArray<FGameplayAbilitySpecHandle>& OutAbilitySpecs);

	//This defines the attributes that should be saved to disk 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Saving")
	TArray<FGameplayAttribute> AttributesToSave;

	//For ASC's that aren't players, you can enter a display name here as some widgets may want the name of the actor - ie "Explosive Barrel", "Sedan", etc. This lets you fill that out. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Avatar Info")
	FText AvatarFallbackDisplayName;

	//ATTACK TOKENS - maintained really just on the server, clients don't need to worry about these

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Tokens")
	int32 NumAttackTokens;

	//NPCs will use this value when they decide whether they should attack us or not. Higher priority people are attacked first 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack Tokens")
	float AttackPriority;

	/**The attackers we've granted a token to are in here. We don't actally bother with a TokenHandle 
	or anything extra as that overcomplicates things - tokens are just pointers to the token holder*/
	UPROPERTY()
	TArray<FAttackToken> GrantedAttackTokens;

	//NPCControllers call this when they want to claim one of our attack tokens. 
	bool TryClaimToken(class ANarrativeNPCController* Claimer);
	bool HasAttackTokenFor(
		const class ANarrativeNPCController* Claimer) const;

	//Return a token. Never fails. 
	void ReturnTokenAtIndex(int32 Index);
	void ReturnToken(class ANarrativeNPCController* Returner);

	//Return true if we can steal a token from the existing one. StealScore should be set to the score so we can use the best score to steal from.
	virtual bool ShouldImmediatelyStealToken(const FAttackToken& Token) const;
	virtual bool CanStealToken(class ANarrativeNPCController* Stealer, const FAttackToken& ExistingToken, float& StealScore) const;
	virtual int32 GetNumAttackTokens() const;
	int32 GetAvailableAttackTokens() const;
	int32 GetNumGrantedAttackTokens() const;

	virtual float GetAttackPriority() const;

	UFUNCTION(BlueprintPure, Category = "Narrative|GAS")
	bool IsDead() const {return bIsDead; } ;

	//Any ASC owner wishing to do something OnDeath should bind to this - it fires on server and all clients when the ASC dies. 
	UPROPERTY(BlueprintAssignable, Category = "Narrative|GAS")
	FOnDeathStateChanged OnDeathStateChanged;

	//Any ASC owner wishing to do something OnHeal should bind to this - it fires on server and all clients when the ASC dies. 
	UPROPERTY(BlueprintAssignable, Category = "Narrative|GAS")
	FOnHealedBy OnHealedBy;

	//Any ASC owner wishing to do something OnDamage should bind to this - it fires on server and all clients when the ASC dies. 
	UPROPERTY(BlueprintAssignable, Category = "Narrative|GAS")
	FOnDealtDamage OnDealtDamage;

	//Any ASC owner wishing to do something OnDamagedby should bind to this - it fires on server and all clients when the ASC is damaged
	UPROPERTY(BlueprintAssignable, Category = "Narrative|GAS")
	FOnDamagedBy OnDamagedBy;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Damage")
	FSovDamageResolvedSignature OnDamageResolvedAsTarget;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Damage")
	FSovDamageResolvedSignature OnDamageResolvedAsSource;

	UPROPERTY(BlueprintAssignable, Category = "Sovereign|Status")
	FSovStatusApplicationRequestedSignature OnStatusApplicationRequested;

	//Use this in rare cases when you want to deal damage without using your own gameplay effect. Typically if we take fall damage, fall out of world etc. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	virtual void DealDamage(const float Damage);

	//Use this in rare cases when you want to deal damage without using your own gameplay effect. Typically if we take fall damage, fall out of world etc. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	virtual void Instakill();

	// Cleaner way to add some tags to the player that can be removed later. We opt for this over adding loose tags. 
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	FActiveGameplayEffectHandle AddDynamicTagsGameplayEffect(const FGameplayTagContainer& TagsToAdd);
	
	/*By default GAS only lets you do this inside a gameplay ability, but we often need damage to happen AFTER the ability ends, because we need to end the
	ability to free up the player for another attack. So for projectiles we expose this function to let those damage the target AFTER ability ends. 
	Requires Authority as obviously we dont have a prediction key to apply. */
	UFUNCTION(BlueprintCallable, Category = "Narrative|GAS")
	TArray<FActiveGameplayEffectHandle> ApplyGameplayEffectSpecToTargetData(const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData);

	//If we're dead, reset. Usually called by reviving players. 
	UFUNCTION()
	virtual void Revive();

protected:
	virtual void NotifyAbilityActivated(const FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_bIsDead, Category = "Narrative|GAS")
	bool bIsDead;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_CharacterReadyEpoch, Category = "Narrative|Readiness")
	int32 CharacterReadyEpoch = 0;

	UPROPERTY(Transient)
	FGameplayTagContainer AppliedDefinitionOwnedTags;

	UPROPERTY(Transient)
	FActiveGameplayEffectHandle TrackedDefaultAttributesEffect;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> TrackedStartupEffects;

	UFUNCTION()
	virtual void OnRep_bIsDead(const bool bOldIsDead);

	UFUNCTION()
	void OnRep_CharacterReadyEpoch();

	/** Contributions owned specifically by replicated death-state convergence. */
	bool bAppliedDeadStateTag = false;
	bool bAppliedFatalStateTag = false;

	//We use this to remember attribute -> attribute value 
	UPROPERTY(SaveGame)
	TArray<FSavedAttribute> SavedAttributes;

	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

private:
	uint64 CombatActorInfoEpoch = 0;
	uint64 SavedAttributeLoadEpoch = 0;
	friend struct FSovCombatInputTestAccess;
	uint64 InputActivationSerial = 0;
	bool IsCombatInputWindowValid() const;
	bool BufferCombatInput(const FGameplayTag& InputTag);
	TWeakObjectPtr<class UNarrativeCombatAbility> CombatInputOwner;
	TWeakObjectPtr<AActor> CombatInputAvatar;
	FGameplayAbilitySpecHandle CombatInputSpec;
	FGuid CombatInputAttackId;
	FGuid CombatInputWindowId;
	FGameplayTagContainer CombatInputAllowedTags;
	FGameplayTag BufferedCombatInput;
	double CombatInputOpensAt = 0.;
	double CombatInputClosesAt = 0.;
	double CombatInputPressedAt = 0.;
	int32 CombatInputReadyEpoch = 0;
	bool bBufferedCombatInputHeld = false;
	TWeakObjectPtr<UObject> BotAttackCoordinator;
	bool IsBotCombatContextValid(AActor* Target, bool bDuringOwnedAttack = false) const;
	void HandleBotAttackEnded(const FAbilityEndedData& Data);
	void ReleaseBotAttackLease(FGameplayAbilitySpecHandle Handle);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	TMap<FGameplayAbilitySpecHandle, FNarrativeBotAttackLease> BotAttackLeases;
	TMap<FGameplayAbilitySpecHandle, double> BotAttackNextAllowedTimes;
	TMap<FGameplayAbilitySpecHandle, uint64> BotAttackLastUsed;
	FDelegateHandle BotAttackEndedDelegate;
	uint64 BotAttackSelectionSerial = 0;
	bool bBotAttackActivationInProgress = false;
	void PruneInvalidAttackTokens();
	int32 GetValidAttackTokenCount() const;

};
