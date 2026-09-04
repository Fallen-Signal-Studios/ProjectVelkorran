// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Progression/SovTechniqueTypes.h"
#include "Progression/SovTechniqueComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"
#include "SkillTrees/SkillTreeComponent.h"

bool USovTechniquePerk::HasValidNativeGrantPolicy() const
{
	const UFunction* LevelFunction = GetClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UTreePerk, SetPerkLevel));
	if (!LevelFunction || LevelFunction->GetOuterUClass() != UTreePerk::StaticClass()
		|| MaxLevels < 1 || MaxLevels > 5 || RequiredBranchInvestment < 0
		|| (PersistentEffects.IsEmpty() && GrantedAbilities.IsEmpty())) { return false; }
	for (TSubclassOf<UGameplayEffect> Class : PersistentEffects)
	{
		const UGameplayEffect* Effect = Class.Get() ? Class->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!Effect || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
			|| !Effect->Executions.IsEmpty() || Effect->Period.GetValueAtLevel(1.f) > 0.f
			|| Effect->StackingType != EGameplayEffectStackingType::None) { return false; }
	}
	for (TSubclassOf<UGameplayAbility> Class : GrantedAbilities)
	{
		if (!Class.Get() || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated)) { return false; }
	}
	return true;
}
void USovTechniquePerk::RemoveNativeGrants()
{
	if (UAbilitySystemComponent* ASC = GrantASC.Get())
	{
		for (FGameplayAbilitySpecHandle Handle : AbilityHandles) { ASC->ClearAbility(Handle); }
		for (FActiveGameplayEffectHandle Handle : EffectHandles) { ASC->RemoveActiveGameplayEffect(Handle); }
	}
	AbilityHandles.Reset(); EffectHandles.Reset(); GrantASC.Reset();
}
void USovTechniquePerk::SetPerkLevel_Implementation(int32 NewPerkLevel)
{
	USkillTreeComponent* Component = GetOwningComponent();
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	auto* Policy = Cast<USovTechniqueComponent>(Component);
	if (!IsValid(Owner) || !Owner->HasAuthority() || !Policy || !Policy->MayApplyPerkGrant(this, NewPerkLevel)) { return; }
	TGuardValue<bool> GrantGuard(Policy->bApplyingGrant, true);
	bLastGrantSucceeded = false;
	RemoveNativeGrants();
	if (!Policy->IsGrantContextCurrent(this)) { return; }
	bLastGrantSucceeded = true;
	if (NewPerkLevel < 0) { Super::SetPerkLevel_Implementation(-1); return; }
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);
	if (!ASC || !HasValidNativeGrantPolicy() || NewPerkLevel >= MaxLevels)
	{
		bLastGrantSucceeded = false; Super::SetPerkLevel_Implementation(-1); return;
	}
	GrantASC = ASC;
	for (TSubclassOf<UGameplayEffect> Class : PersistentEffects)
	{
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext(); Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Class, NewPerkLevel + 1.f, Context);
		FActiveGameplayEffectHandle Handle = Spec.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
		if (Handle.IsValid()) { EffectHandles.Add(Handle); }
		if (!Handle.IsValid() || !Policy->IsGrantContextCurrent(this)) { bLastGrantSucceeded = false; break; }
	}
	if (bLastGrantSucceeded)
	{
		for (TSubclassOf<UGameplayAbility> Class : GrantedAbilities)
		{
			FGameplayAbilitySpec Spec(Class, NewPerkLevel + 1, INDEX_NONE, this);
			const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
			if (Handle.IsValid()) { AbilityHandles.Add(Handle); }
			if (!Handle.IsValid() || !Policy->IsGrantContextCurrent(this)) { bLastGrantSucceeded = false; break; }
		}
	}
	if (!bLastGrantSucceeded) { RemoveNativeGrants(); }
	Super::SetPerkLevel_Implementation(bLastGrantSucceeded ? NewPerkLevel : -1);
}

void USovTechniquePerk::GetOwnedPerkGrants(TArray<FGameplayAbilitySpecHandle>& Abilities, TArray<FActiveGameplayEffectHandle>& Effects) const
{
	Abilities = AbilityHandles; Effects = EffectHandles;
}
