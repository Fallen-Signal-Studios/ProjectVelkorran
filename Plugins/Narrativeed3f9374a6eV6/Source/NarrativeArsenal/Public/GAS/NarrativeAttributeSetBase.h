// Copyright Narrative Tools 2024.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "NarrativeAttributeSetBase.generated.h"


// Uses macros from AttributeSet.h.
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

// Broadcasts authoritative attribute transitions that require gameplay reactions.
DECLARE_MULTICAST_DELEGATE_FourParams(
	FNarrativeAttributeEvent,
	AActor* /*EffectInstigator*/,
	AActor* /*EffectCauser*/,
	const FGameplayEffectSpec& /*EffectSpec*/,
	float /*EffectMagnitude*/);

/**
 * Core Narrative Pro attributes used by Project Velkorran.
 *
 * Damage is a transient meta attribute. Damage executions write one final incoming
 * amount to Damage, and this set routes that amount through Shield before Health.
 * Echo is a 0..MaxEcho momentum resource. Its protagonist-specific gain, decay, and
 * spending rules live in abilities/effects rather than inside the attribute set.
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeAttributeSetBase : public UAttributeSet
{
	GENERATED_BODY()

public:
	UNarrativeAttributeSetBase();

	// UAttributeSet
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Legacy Narrative Pro progression attribute. The campaign does not use XP, but
	// the attribute remains for plugin compatibility until project-owned sets replace it.
	UPROPERTY(BlueprintReadOnly, Category = "Progression", ReplicatedUsing = OnRep_XP, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData XP;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, XP)

	// Persistent bodily integrity. Negative changes should enter through Damage.
	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_Health, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, MaxHealth)

	// Regenerating first layer of survivability. Incoming Damage consumes Shield first.
	UPROPERTY(BlueprintReadOnly, Category = "Shield", ReplicatedUsing = OnRep_Shield, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData Shield;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Shield)

	UPROPERTY(BlueprintReadOnly, Category = "Shield", ReplicatedUsing = OnRep_MaxShield, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData MaxShield;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, MaxShield)

	// Immediate exertion resource for defense, sprinting, and selected cancels.
	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_Stamina, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData Stamina;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_MaxStamina, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData MaxStamina;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, MaxStamina)

	// Character-specific momentum earned through actions true to the protagonist.
	UPROPERTY(BlueprintReadOnly, Category = "Echo", ReplicatedUsing = OnRep_Echo, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData Echo;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Echo)

	UPROPERTY(BlueprintReadOnly, Category = "Echo", ReplicatedUsing = OnRep_MaxEcho, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData MaxEcho;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, MaxEcho)

	UPROPERTY(BlueprintReadOnly, Category = "Stamina", ReplicatedUsing = OnRep_StaminaRegenRate, meta = (NarrativeSaveAttribute))
	FGameplayAttributeData StaminaRegenRate;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, StaminaRegenRate)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackRating)
	FGameplayAttributeData AttackRating;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, AttackRating)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Armor)

	UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackDamage)
	FGameplayAttributeData AttackDamage;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, AttackDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Stealth", ReplicatedUsing = OnRep_StealthRating)
	FGameplayAttributeData StealthRating;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, StealthRating)

	// Transient meta attributes. They are consumed and reset in PostGameplayEffectExecute.
	UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes")
	FGameplayAttributeData Heal;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Heal)

	UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(UNarrativeAttributeSetBase, Damage)

	// Fired once when Health crosses from above zero to zero due to a resolved hit.
	FNarrativeAttributeEvent OnOutOfHealth;

	// Fired once when a resolved hit consumes the final point of Shield.
	FNarrativeAttributeEvent OnShieldBroken;

protected:
	// Maintains the current percentage when a maximum attribute changes.
	void AdjustAttributeForMaxChange(
		FGameplayAttributeData& AffectedAttribute,
		const FGameplayAttributeData& MaxAttribute,
		float NewMaxValue,
		const FGameplayAttribute& AffectedAttributeProperty);

	UFUNCTION()
	virtual void OnRep_XP(const FGameplayAttributeData& OldXP);

	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

	UFUNCTION()
	virtual void OnRep_Shield(const FGameplayAttributeData& OldShield);

	UFUNCTION()
	virtual void OnRep_MaxShield(const FGameplayAttributeData& OldMaxShield);

	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);

	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);

	UFUNCTION()
	virtual void OnRep_Echo(const FGameplayAttributeData& OldEcho);

	UFUNCTION()
	virtual void OnRep_MaxEcho(const FGameplayAttributeData& OldMaxEcho);

	UFUNCTION()
	virtual void OnRep_StaminaRegenRate(const FGameplayAttributeData& OldStaminaRegenRate);

	UFUNCTION()
	virtual void OnRep_Armor(const FGameplayAttributeData& OldArmor);

	UFUNCTION()
	virtual void OnRep_AttackRating(const FGameplayAttributeData& OldAttackRating);

	UFUNCTION()
	virtual void OnRep_StealthRating(const FGameplayAttributeData& OldStealthRating);

	UFUNCTION()
	virtual void OnRep_AttackDamage(const FGameplayAttributeData& OldAttackDamage);
};

/**
 * Reserved for character-only attributes that do not belong on every ASC owner.
 */
UCLASS()
class NARRATIVEARSENAL_API UNarrativeCharacterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UNarrativeCharacterAttributeSet();
};
