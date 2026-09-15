// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "UObject/Interface.h"
#include "SovEnvironmentDamage.generated.h"

struct FHitResult;

/** Marks an authored scenery owner that admits combat damage without an ability system. */
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))
class NARRATIVEARSENAL_API USovEnvironmentDamageable : public UInterface
{
	GENERATED_BODY()
};

class NARRATIVEARSENAL_API ISovEnvironmentDamageable
{
	GENERATED_BODY()
};

/**
 * The one admission path from character combat into scenery. Callers keep their own GAS
 * payloads; owners still enforce their own opt-in, identity and health.
 */
namespace SovEnvironmentDamage
{
	/** Point damage on the hit actor. Returns the amount the owner accepted. */
	NARRATIVEARSENAL_API float ApplyPoint(AActor* Source, const FHitResult& Hit, float Damage);

	/**
	 * Linear falloff from full damage at the origin to MinimumFraction at Radius, measured to each
	 * owner's nearest bounds point. Every candidate's sightline is resolved before any damage, so a
	 * break cannot open another owner to the same blast. Returns the number of owners damaged.
	 */
	NARRATIVEARSENAL_API int32 ApplyRadial(AActor* Source, const FVector& Origin, float Radius, float Damage,
		float MinimumFraction, bool bRequireLineOfSight, const FCollisionQueryParams& Query,
		TArray<AActor*>* OutDamaged = nullptr);
}
