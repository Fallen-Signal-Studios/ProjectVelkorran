// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleMovementSyncTrait.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassAgentTraits.h"
#include "MassEntityView.h"
#include "MassMovementFragments.h"
#include "AI/Mass/MassAgentTraitsHelper.h"
#include "Vehicles/Mass/VehicleFragments.h"
#include "Vehicles/Mass/VehicleVisualizationProcessor.h"

void UVehicleMovementSyncTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassActorFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();
	BuildContext.AddFragment<FVehicleComponentWrapperFragment>();
	
	BuildContext.GetMutableObjectFragmentInitializers().Add([=](UObject& Owner, FMassEntityView& EntityView, const EMassTranslationDirection CurrentDirection)
	{
		if (auto MovementComp = FMassAgentTraitsHelper::AsComponent<UChaosWheeledVehicleMovementComponent>(Owner))
		{
			auto& ComponentFragment = EntityView.GetFragmentData<FVehicleComponentWrapperFragment>();
			ComponentFragment.Component = MovementComp;
		}
	});
	if (EnumHasAnyFlags(SyncDirection, EMassTranslationDirection::MassToActor) || BuildContext.IsInspectingData())
	{
		BuildContext.AddTranslator<UVehicleVisualizationProcessor>();
	}
}
