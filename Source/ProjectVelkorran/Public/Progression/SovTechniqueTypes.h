// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "SkillTrees/TreeSkill.h"
#include "SkillTrees/TreePerk.h"
#include "SkillTrees/SovOwnedPerkGrants.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayEffectTypes.h"
#include "SovTechniqueTypes.generated.h"
class UGameplayAbility;
class UGameplayEffect;
class UAbilitySystemComponent;

/** Identity metadata on the existing Narrative skill/branch, not a second graph. */
UCLASS(Blueprintable, EditInlineNew)
class PROJECTVELKORRAN_API USovTechniqueSkill : public UTreeSkill
{
	GENERATED_BODY()
public:
	USovTechniqueSkill(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique") FGameplayTag Protagonist;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique") FName BranchId;
};

/** Data-only perk grant policy, so rank changes and free respec are reversible. */
UCLASS(Blueprintable, EditInlineNew)
class PROJECTVELKORRAN_API USovTechniquePerk : public UTreePerk, public ISovOwnedPerkGrants
{
	GENERATED_BODY()
public:
	USovTechniquePerk(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
	virtual void SetPerkLevel_Implementation(int32 NewPerkLevel) override;
	bool HasValidNativeGrantPolicy() const;
	virtual void GetOwnedPerkGrants(TArray<FGameplayAbilitySpecHandle>& Abilities, TArray<FActiveGameplayEffectHandle>& Effects) const override;
	bool DidLastGrantSucceed() const { return bLastGrantSucceeded; }
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique") TArray<TSubclassOf<UGameplayEffect>> PersistentEffects;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique") TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique", meta=(ClampMin="0")) int32 RequiredBranchInvestment = 0;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Technique") TArray<TSubclassOf<UTreePerk>> IncompatiblePerks;
private:
	void RemoveNativeGrants();
	TWeakObjectPtr<UAbilitySystemComponent> GrantASC;
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	TArray<FGameplayAbilitySpecHandle> AbilityHandles;
	bool bLastGrantSucceeded = true;
};
