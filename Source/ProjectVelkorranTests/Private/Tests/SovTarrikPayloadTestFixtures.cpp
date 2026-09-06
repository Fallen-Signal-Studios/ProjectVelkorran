// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovTarrikPayloadTestFixtures.h"
#include "Items/WeaponItem.h"
USovTarrikSlamTestAbility::USovTarrikSlamTestAbility()
{
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
	SlamDamage = 20.f; SlamPoiseDamage = 10.f;
	WardDuration = 0.25f;
}
USovTarrikRequiemTestAbility::USovTarrikRequiemTestAbility()
{
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
	FallbackMuzzleOffset = FVector::ZeroVector;
	MaximumRange = 2000.f;
	PenetratingDamage = 20.f; PenetratingPoiseDamage = 10.f;
	LineDetonationDamage = 10.f; LineDetonationPoiseDamage = 10.f;
	LineDetonationSpacing = 200.f;
}
USovTarrikHungerTestAbility::USovTarrikHungerTestAbility()
{
	AllowedWeaponClasses.Add(UWeaponItem::StaticClass());
}
USovTarrikGrenadeTestAbility::USovTarrikGrenadeTestAbility() {}
