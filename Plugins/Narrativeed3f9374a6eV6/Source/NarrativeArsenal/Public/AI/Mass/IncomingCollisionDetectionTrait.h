// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "IncomingCollisionFragments.h"
#include "MassEntityTraitBase.h"
#include "IncomingCollisionDetectionTrait.generated.h"

/**
 * Trait for keeping track of entities that may eventually collide with us
 */
UCLASS()
class NARRATIVEARSENAL_API UIncomingCollisionDetectionTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = "Incoming Collision Detection")
	FCollisionDetectionSharedFragment CollisionDetectionProperties;
};
