// Copyright Narrative Tools 2025.


#include "Items/Fragments/PoisonableFragment.h"

UPoisonableFragment::UPoisonableFragment(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{

}

bool UPoisonableFragment::CanBePoisonedBy(const class UPoisonItem* Poison) const
{
	//Override if your game needs certain items to accept/deny certain poisons. 
	return !IsValid(AppliedPoison);
}

void UPoisonableFragment::SetPoison(const TSubclassOf<UGameplayEffect>& Poison)
{
	AppliedPoison = Poison;
}

