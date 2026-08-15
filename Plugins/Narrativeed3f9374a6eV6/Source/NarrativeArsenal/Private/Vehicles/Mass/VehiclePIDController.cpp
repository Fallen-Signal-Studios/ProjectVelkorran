// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehiclePIDController.h"
#include "Vehicles/Mass/VehicleFragments.h"

float FVehiclePIDController::Tick(float Goal, float Actual,
                                  const FPIDSettings& Params)
{
	float Error = Goal - Actual;

	float Proportional = Params.ProportionalGain * Error;
	float Integral = Params.IntegralGain * ErrorIntegral;
	float Derivative = Params.DerivativeGain * (Error - LastError);

	LastError = Error;

	return Proportional + Integral + Derivative;
}
