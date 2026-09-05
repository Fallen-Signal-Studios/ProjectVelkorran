// Copyright Narrative Tools 2024. 


#include "SkillTrees/SkillTreeComponent.h"
#include "SkillTrees/TreePerk.h"
#include "SkillTrees/SovOwnedPerkGrants.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "SkillTreeComponent"

// Sets default values for this component's properties
USkillTreeComponent::USkillTreeComponent()
{
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void USkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	PrerequisiteMap.Empty();

	//Build the prereq map 
	for(auto& SkillTreeSkill : SkillTreeSkills)
	{
		for (auto& PerkConfig : SkillTreeSkill->Perks)
		{
			if (PerkConfig.Perk)
			{
				for (auto& LinkedTo : PerkConfig.LinkedTo)
				{
					if (IsValid(LinkedTo))
					{
						FPerkArray& PArray = PrerequisiteMap.FindOrAdd(LinkedTo);
						PArray.Array.AddUnique(PerkConfig.Perk);
					}
				}
			}
		}
	}
}

bool USkillTreeComponent::BuyPerk(TSubclassOf<UTreePerk> Perk, UTreeSkill* OwnerSkill)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRestoringPerks) { return false; }

	check(OwnerSkill);

	if (OwnerSkill && Perk)
	{
		FText ErrText;
		if (CanBuyPerk(Perk, ErrText))
		{
			UTreePerk* PerkInstance = GetPerk(Perk);

			if (!PerkInstance)
			{
				PerkInstance = NewObject<UTreePerk>(this, Perk);
				PurchasedPerks.Add(PerkInstance);
			}

			if (PerkInstance)
			{
				ApplyPurchasedPerkLevel(PerkInstance, PerkInstance->PerkLevel + 1);

				//We now have 1 less point to spend, and buying a perk should level up the skill. 
				--SkillTreePoints;
				OwnerSkill->SkillLevel++;

				return true;
			}
		}
	}

	return false; 
}

bool USkillTreeComponent::CanBuyPerk(TSubclassOf<UTreePerk> Perk, FText& OutCantBuyReason)
{

	if (!Perk)
	{
		return false; 
	}

	if (UTreePerk* PurchasedPerk = GetPerk(Perk))
	{
		if (PurchasedPerk->PerkLevel >= PurchasedPerk->MaxLevels - 1) //PerkLevel is zero based so subtract 1 so designers so 1-based number 
		{
			OutCantBuyReason = LOCTEXT("CantBuyReason_MaxLevel", "Perk Maxed");
			return false;
		}
	}
	
	if (!HasRequiredPerks(Perk))
	{
		OutCantBuyReason = LOCTEXT("CantBuyReason_Locked", "Locked");
		return false;
	}

	if (SkillTreePoints <= 0)
	{
		OutCantBuyReason = LOCTEXT("CantBuyReason_NoPoints", "Not Enough Points");
		return false;
	}

	return true;  
}

bool USkillTreeComponent::HasRequiredPerks(TSubclassOf<UTreePerk> Perk)
{
	if (PrerequisiteMap.Contains(Perk))
	{
		FPerkArray PArray = *PrerequisiteMap.Find(Perk);
		for (auto& PrereqPerk : PArray.Array)
		{
			if (HasPerk(PrereqPerk))
			{
				return true; 
			}
		}

		return false; 
	}

	return true;
}

void USkillTreeComponent::GiveSkillPoints(const int32 Points)
{
	SkillTreePoints += Points;
}

int32 USkillTreeComponent::GetPerkLevel(TSubclassOf<UTreePerk> PerkClass)
{
	for (auto& Perk : PurchasedPerks)
	{
		if (Perk->GetClass() == PerkClass)
		{
			return Perk->PerkLevel;
		}
	}

	return -1; 
}

UTreePerk* USkillTreeComponent::GetPerk(TSubclassOf<UTreePerk> PerkClass) const
{
	for (auto& Perk : PurchasedPerks)
	{
		if (Perk->GetClass() == PerkClass)
		{
			return Perk;
		}
	}

	return nullptr; 
}

bool USkillTreeComponent::HasPerk(TSubclassOf<UTreePerk> PerkClass) const
{
	return IsValid(GetPerk(PerkClass));
}

FSavedSkill USkillTreeComponent::SkillToSaveData(const UTreeSkill* Skill) const
{
	FSavedSkill SavedSkill;

	SavedSkill.SkillClass = Skill->GetClass();
	SavedSkill.SkillLevel = Skill->SkillLevel;

	return SavedSkill;
}

FSavedPerk USkillTreeComponent::PerkToSaveData(const UTreePerk* Perk) const
{
	FSavedPerk SavedPerk;

	SavedPerk.PerkClass = Perk->GetClass();
	SavedPerk.PerkLevel = Perk->PerkLevel;

	return SavedPerk;
}

void USkillTreeComponent::Load_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bRestoringPerks) { return; }
	TGuardValue<bool> RestoreGuard(bRestoringPerks, true);
	ClearPurchasedPerksForRestore();
	for (UTreeSkill* Skill : SkillTreeSkills)
	{
		if (Skill) { Skill->SkillLevel = GetDefault<UTreeSkill>(Skill->GetClass())->SkillLevel; }
	}
	if (SkillTreeSaveData.HasSaveData())
	{
		//Set the skills back to their saved state
		for (auto& SavedSkill : SkillTreeSaveData.SavedSkills)
		{
			for (auto& Skill : SkillTreeSkills)
			{
				if (Skill && Skill->GetClass() == SavedSkill.SkillClass)
				{
					Skill->SkillLevel = SavedSkill.SkillLevel;
				}
			}
		}

		TSet<UClass*> RestoredClasses;
		for (auto& SavedPerk : SkillTreeSaveData.SavedPerks)
		{
			if (SavedPerk.PerkClass && !RestoredClasses.Contains(SavedPerk.PerkClass.Get()) && SavedPerk.PerkLevel >= 0)
			{
				RestoredClasses.Add(SavedPerk.PerkClass.Get());
				if (UTreePerk* PerkInstance = NewObject<UTreePerk>(this, SavedPerk.PerkClass))
				{
					PurchasedPerks.Add(PerkInstance);
					ApplyPurchasedPerkLevel(PerkInstance, FMath::Clamp(SavedPerk.PerkLevel, 0, FMath::Max(0, PerkInstance->MaxLevels - 1)));
				}
			}
		}
	}
}

void USkillTreeComponent::ApplyPurchasedPerkLevel(UTreePerk* Perk, int32 Level)
{
	if (!IsValid(Perk)) { return; }
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	TSet<FGameplayAbilitySpecHandle> BeforeAbilities;
	TSet<FActiveGameplayEffectHandle> BeforeEffects;
	if (ASC)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities()) { BeforeAbilities.Add(Spec.Handle); }
		for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery())) { BeforeEffects.Add(Handle); }
	}
	Perk->SetPerkLevel(Level);
	if (!IsValid(Perk) || !PurchasedPerks.Contains(Perk)) { return; }
	if (const ISovOwnedPerkGrants* Reporter = Cast<ISovOwnedPerkGrants>(Perk))
	{
		FOwnedPerkGrants ExactGrants;
		Reporter->GetOwnedPerkGrants(ExactGrants.Abilities, ExactGrants.Effects);
		OwnedPerkGrants.Add(Perk, MoveTemp(ExactGrants));
		return;
	}
	if (ASC)
	{
		FOwnedPerkGrants& Grants = OwnedPerkGrants.FindOrAdd(Perk);
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (!BeforeAbilities.Contains(Spec.Handle)) { Grants.Abilities.AddUnique(Spec.Handle); }
		}
		for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			if (!BeforeEffects.Contains(Handle)) { Grants.Effects.AddUnique(Handle); }
		}
	}
}

void USkillTreeComponent::ClearPurchasedPerksForRestore()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	const TArray<TObjectPtr<UTreePerk>> PreviousPerks = MoveTemp(PurchasedPerks);
	PurchasedPerks.Reset();
	const auto PreviousGrants = MoveTemp(OwnedPerkGrants);
	OwnedPerkGrants.Reset();
	if (ASC)
	{
		for (const auto& Pair : PreviousGrants)
		{
			for (const FGameplayAbilitySpecHandle Handle : Pair.Value.Abilities) { ASC->ClearAbility(Handle); }
			for (const FActiveGameplayEffectHandle Handle : Pair.Value.Effects) { ASC->RemoveActiveGameplayEffect(Handle); }
		}
	}
	for (UTreePerk* Perk : PreviousPerks)
	{
		if (Perk) { Perk->PerkLevel = -1; }
	}
}

void USkillTreeComponent::PrepareForSave_Implementation()
{
	//Iterate the skills, and store them in our structs which get serialized
	SkillTreeSaveData.ClearData();

	for (auto& Skill : SkillTreeSkills)
	{
		SkillTreeSaveData.SavedSkills.Add(SkillToSaveData(Skill));
	}

	for (auto& Perk : PurchasedPerks)
	{
		SkillTreeSaveData.SavedPerks.Add(PerkToSaveData(Perk));
	}
}

#undef LOCTEXT_NAMESPACE
