// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovSelenePayloadTestFixtures.h"
#include "Tests/SovAxiomRuntimeTestFixtures.h"
#include "Items/WeaponItem.h"
USovStillpointPayloadTestAbility::USovStillpointPayloadTestAbility() { AllowedWeaponClasses.Add(UWeaponItem::StaticClass()); }
USovZeroPayloadTestAbility::USovZeroPayloadTestAbility() { AllowedWeaponClasses.Add(UWeaponItem::StaticClass()); }
USovWakePayloadTestAbility::USovWakePayloadTestAbility() { AllowedWeaponClasses.Add(UWeaponItem::StaticClass()); }
USovDispatchPayloadTestAbility::USovDispatchPayloadTestAbility() { VerityWeaponClasses.Add(USovAxiomRuntimeTestWeapon::StaticClass()); }
