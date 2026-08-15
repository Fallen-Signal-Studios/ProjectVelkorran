// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "NarrativeSavableComponent.h"
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
	virtual void HandleOutOfHealth(AActor* DamageInstigator, AActor* DamageCauser, const FGameplayEffectSpec& DamageEffectSpec, float DamageMagnitude);
	virtual void Debug_Internal(struct FAbilitySystemComponentDebugInfo& Info) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//Get the frequency a given attack should fire off at, provided we have one. Used for bots. InputTag must have a combat ability on it. IE Input.Attack, AltAttack, etc. 
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS")
	virtual float GetBotAttackFrequency(FGameplayTag InputTag);

	//Get the range a given attack should cover, Used for bots. InputTag must have a combat ability on it. IE Input.Attack, AltAttack, etc. 
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Narrative|GAS")
	virtual float GetBotAttackRange(FGameplayTag InputTag);

	//Workaround for attribute changed GEData not containing a valid instigator - we need the instigator so bots know when they receive damage. 
	virtual void HealedBy(UNarrativeAbilitySystemComponent* Healer, const float Amount, const FGameplayEffectSpec& Spec);

	//Workaround for attribute changed GEData not containing a valid instigator - we need the instigator so bots know when they receive damage. 
	virtual void DamagedBy(UNarrativeAbilitySystemComponent* DamageCauser, const float Damage, const FGameplayEffectSpec& Spec);

	//Workaround for attribute changed GEData not containing a valid instigator - we need the instigator so bots know when they receive damage. 
	virtual void DealtDamage(UNarrativeAbilitySystemComponent* DamagedTarget, const float Damage, const FGameplayEffectSpec& Spec);
	
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

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_bIsDead, Category = "Narrative|GAS")
	bool bIsDead;

	UFUNCTION()
	virtual void OnRep_bIsDead(const bool bOldIsDead);

	//We use this to remember attribute -> attribute value 
	UPROPERTY(SaveGame)
	TArray<FSavedAttribute> SavedAttributes;

	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

};
