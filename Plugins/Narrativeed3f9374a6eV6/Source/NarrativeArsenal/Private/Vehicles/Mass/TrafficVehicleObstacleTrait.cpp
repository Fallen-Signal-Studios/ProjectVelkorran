// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/TrafficVehicleObstacleTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Vehicles/Mass/VehicleFragments.h"

void UTrafficVehicleObstacleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext,
                                                 const UWorld& World) const
{
	BuildContext.AddFragment<FVehicleObstacleFragment>();
	
	BuildContext.RequireFragment<FAgentRadiusFragment>();
	
	BuildContext.RequireFragment<FTransformFragment>();
}
