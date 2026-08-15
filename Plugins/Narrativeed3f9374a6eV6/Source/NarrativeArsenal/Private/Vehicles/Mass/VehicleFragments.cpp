// Copyright Narrative Tools 2025.


#include "Vehicles/Mass/VehicleFragments.h"

DEFINE_LOG_CATEGORY(LogMassVehicles)

FVehiclePassengerWeightedSampler::FVehiclePassengerWeightedSampler(const TArray<FPassengerWeightedProbability>& Probability) : PassengerCountProbability(Probability)
{
}

float FVehiclePassengerWeightedSampler::GetWeights(TArray<float>& OutWeights)
{
	float Sum = 0;
	for (auto& Weight : PassengerCountProbability)
	{
		OutWeights.Add(Weight.WeightedProbability);
		Sum += Weight.WeightedProbability;
	}
	return Sum;
}
