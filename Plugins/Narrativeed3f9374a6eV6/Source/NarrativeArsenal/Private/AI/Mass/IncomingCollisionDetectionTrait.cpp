// Copyright Narrative Tools 2025.


#include "AI/Mass/IncomingCollisionDetectionTrait.h"

#include "MassEntityTemplateRegistry.h"

void UIncomingCollisionDetectionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
                                                     const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	
	BuildContext.AddFragment<FIncomingCollisionFragment>();
	
	const FConstSharedStruct& CollisionSharedFragment = EntityManager.GetOrCreateConstSharedFragment<FCollisionDetectionSharedFragment>(CollisionDetectionProperties);
	BuildContext.AddConstSharedFragment(CollisionSharedFragment);
}
