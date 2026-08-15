// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleTrait.h"

#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "Vehicles/Mass/VehicleFragments.h"

void UVehicleTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	auto EntityManager = UE::Mass::Utils::GetEntityManager(&World);
	
	// Vehicle navigation
	BuildContext.AddFragment<FMassZoneGraphLaneLocationFragment>();
	BuildContext.AddFragment<FVehicleLocomotionFragment>();
	
	auto& PIDSharedFragment = EntityManager->GetOrCreateConstSharedFragment(PIDControllerSettings);
	BuildContext.AddConstSharedFragment(PIDSharedFragment);

	auto& VehicleSettingsSharedFragment = EntityManager->GetOrCreateConstSharedFragment(VehicleSettingsFragment);
	BuildContext.AddConstSharedFragment(VehicleSettingsSharedFragment);
	
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassVelocityFragment>();

	// Collision
	BuildContext.AddFragment(FConstStructView::Make(Radius));

	// Seed
	BuildContext.AddFragment(FConstStructView::Make(SeedFragment));

	// Passenger array
	auto& PassengerSharedFragment = EntityManager->GetOrCreateConstSharedFragment(Passengers);
	BuildContext.AddConstSharedFragment(PassengerSharedFragment);

	BuildContext.AddFragment<FVehiclePassengersFragment>();
}
