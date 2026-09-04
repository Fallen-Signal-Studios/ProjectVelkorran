// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "SovCorruptionAttributeSet.generated.h"

#define SOV_CORRUPTION_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Narrow GAS storage for Eclipse corruption exposure.
 *
 * Policy, bands, sources, remedies, presentation, and checkpoint semantics live
 * in USovCorruptionComponent. Keeping these values in their own AttributeSet
 * lets the persistent PlayerState ASC replicate them without widening
 * Narrative's general-purpose combat set.
 */
UCLASS()
class PROJECTVELKORRAN_API USovCorruptionAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USovCorruptionAttributeSet();

	virtual void PreAttributeChange(
		const FGameplayAttribute& Attribute,
		float& NewValue) override;
	virtual void PostAttributeChange(
		const FGameplayAttribute& Attribute,
		float OldValue,
		float NewValue) override;
	virtual void PostGameplayEffectExecute(
		const FGameplayEffectModCallbackData& Data) override;
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption", ReplicatedUsing = OnRep_Corruption)
	FGameplayAttributeData Corruption;
	SOV_CORRUPTION_ATTRIBUTE_ACCESSORS(USovCorruptionAttributeSet, Corruption)

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Corruption", ReplicatedUsing = OnRep_MaxCorruption)
	FGameplayAttributeData MaxCorruption;
	SOV_CORRUPTION_ATTRIBUTE_ACCESSORS(USovCorruptionAttributeSet, MaxCorruption)

protected:
	UFUNCTION()
	void OnRep_Corruption(const FGameplayAttributeData& OldCorruption);

	UFUNCTION()
	void OnRep_MaxCorruption(const FGameplayAttributeData& OldMaxCorruption);
};

#undef SOV_CORRUPTION_ATTRIBUTE_ACCESSORS
