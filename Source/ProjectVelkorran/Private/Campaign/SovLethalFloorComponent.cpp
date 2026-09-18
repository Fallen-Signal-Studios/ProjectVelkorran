// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovLethalFloorComponent.h"

#include "Net/UnrealNetwork.h"

USovLethalFloorComponent::USovLethalFloorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USovLethalFloorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovLethalFloorComponent, MinimumHealth);
	DOREPLIFETIME(USovLethalFloorComponent, bFloorHeld);
}

void USovLethalFloorComponent::SetFloorHeld(const bool bHeld)
{
	const AActor* const Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bFloorHeld == bHeld) { return; }
	bFloorHeld = bHeld;
	OnLethalFloorChanged.Broadcast(bFloorHeld);
}

void USovLethalFloorComponent::OnRep_FloorHeld()
{
	OnLethalFloorChanged.Broadcast(bFloorHeld);
}

bool USovLethalFloorComponent::LimitSovIncomingDamage(AActor* Instigator, const FGameplayEffectContextHandle& Context,
	const float CurrentHealth, float& InOutShieldDamage, float& InOutHealthDamage, float& InOutPoiseDamage) const
{
	static_cast<void>(Instigator);
	static_cast<void>(Context);
	static_cast<void>(InOutShieldDamage);
	static_cast<void>(InOutPoiseDamage);
	// Shield and poise are untouched: the floor exists to prevent a finish, not to blunt the fight.
	// Breaking poise while the mechanic is outstanding is exactly what the phase wants to encourage.
	if (!bFloorHeld) { return true; }
	const float Floor = FMath::IsFinite(MinimumHealth) ? FMath::Max(MinimumHealth, 1.f) : 1.f;
	if (!FMath::IsFinite(CurrentHealth) || CurrentHealth <= Floor)
	{
		// Already at or below the floor: this hit may not reduce health at all.
		InOutHealthDamage = 0.f;
		return true;
	}
	if (FMath::IsFinite(InOutHealthDamage))
	{ InOutHealthDamage = FMath::Min(InOutHealthDamage, CurrentHealth - Floor); }
	return true;
}
