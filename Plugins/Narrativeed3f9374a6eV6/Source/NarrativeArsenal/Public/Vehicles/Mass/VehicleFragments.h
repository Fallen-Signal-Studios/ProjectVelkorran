// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "MassCommonFragments.h"
#include "MassEntityTypes.h"
#include "MassVehicleSubsystem.h"
#include "VehiclePIDController.h"
#include "ZoneGraphTypes.h"
#include "MassEntityHandle.h"
#include "WeightedRandomSampler.h"
#include "VehicleFragments.generated.h"

class UNPCDefinition;
class UChaosWheeledVehicleMovementComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogMassVehicles, Log, All);

USTRUCT()
struct FVehicleLocomotionFragment : public FMassFragment
{
	GENERATED_BODY()

	FVehicleLocomotionFragment() = default;

	UPROPERTY()
	float DesiredSpeed = 0.f;

	// High LOD PID Controller
	FVehiclePIDController ThrottlePIDController;
	FVehiclePIDController SteeringPIDController;

	UPROPERTY()
	float Throttle = 0.f; // Range -1 to 1 where < 0 is braking

	UPROPERTY()
	float Steering = 0.f;

	UPROPERTY()
	float DistanceToNextVehicle = FLT_MAX;

	UPROPERTY()
	FMassEntityHandle NextVehicleHandle = FMassEntityHandle();

	FZoneGraphLaneHandle NextLane;

	UPROPERTY()
	float Color = 0.f;
};

USTRUCT()
struct FPIDSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Controller")
	float ProportionalGain = 0.f;

	UPROPERTY(EditAnywhere, Category = "Controller")
	float IntegralGain = 0.f;

	UPROPERTY(EditAnywhere, Category = "Controller")
	float DerivativeGain = 0.f;
};

USTRUCT()
struct FVehiclePIDControllerFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "PID Settings")
	FPIDSettings SteeringSettings;
	
	UPROPERTY(EditAnywhere, Category = "PID Settings")
	FPIDSettings ThrottleSettings;

	// Distance ahead of current lane to calculate steering
	UPROPERTY(EditAnywhere, Category = "PID Settings")
	float LookAheadDistance = 500.f;
};

USTRUCT()
struct FVehicleSettingsFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	// The minimum distance we want to be from an obstacle
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float MinimumDistanceToObstacle = 300.f;

	// The distance that we begin braking when there is an incoming obstacle
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float BrakingDistanceFromObstacle = 500.f;

	// The power that we use on the brakes to stop the vehicle on an incoming obstacle
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float ObstacleAvoidanceBrakingPower = 2.f;

	// Radius to search around for nearby obstacles
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float ObstacleSearchRadius = 1500.f;

	// The minimum distance to be from a closed lane
	UPROPERTY(EditAnywhere, Category = "Vehicle|Closed Lane")
	float ClosedLaneMinDistance = 100.f;

	// The distance that we begin to brake when there is an incoming closed lane
	UPROPERTY(EditAnywhere, Category = "Vehicle|Closed Lane")
	float ClosedLaneBrakingDistance = 300.f;

	// The power used on the brakes to have the vehicle come to a stop when there is an incoming closed lane
	UPROPERTY(EditAnywhere, Category = "Vehicle|Closed Lane")
	float ClosedLaneBrakingPower = 2.f;

	// The minimum distance we want to be from an obstacle
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float MinimumDistanceToNext = 300.f;

	// The distance that we begin braking when there is an incoming obstacle
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float BrakingDistanceFromNext = 500.f;

	// The power that we use on the brakes to stop the vehicle on an incoming obstacle
	UPROPERTY(EditAnywhere, Category = "Vehicle|Obstacle Avoidance")
	float NextVehicleAvoidanceBrakingPower = 2.f;

	// The max speed we would like the vehicle to be going
	UPROPERTY(EditAnywhere, Category = "Vehicle")
	float VehicleMaxSpeed = 400.f;

	// The range to search for a nearby zonegraph lane
	UPROPERTY(EditAnywhere, Category = "Vehicle")
	float LaneSearchRadius = 500.f;

	// The filter that will be used when picking a lane in the zonegraph
	UPROPERTY(EditAnywhere, Category = "Vehicle")
	FZoneGraphTagFilter VehicleLaneFilter;
};

USTRUCT()
struct FVehicleObstacleFragment : public FMassFragment
{
	GENERATED_BODY()

	// The stored location of this obstacle
	FVehicleObstacleHashGrid::FCellLocation CellLocation;
};

USTRUCT()
struct FVehicleComponentWrapperFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()

	TWeakObjectPtr<UChaosWheeledVehicleMovementComponent> Component;
};

USTRUCT()
struct FMassVehicleMovementToActorTag : public FMassTag
{
	GENERATED_BODY()
	
};

// Stores a seed for deterministic values
USTRUCT()
struct FSeedFragment : public FMassFragment
{
	GENERATED_BODY()

	FSeedFragment() = default;

	FSeedFragment(int32 InSeed) : Seed(InSeed) {};

	UPROPERTY(EditAnywhere, Category = "Seed")
	int32 Seed = 0;

	UPROPERTY()
	FRandomStream Stream;
};


/**
 * Stores information regarding weighted probability and # of passengers to give vehicle
 */
USTRUCT()
struct FPassengerWeightedProbability
{
	GENERATED_BODY()

	FPassengerWeightedProbability() = default;

	FPassengerWeightedProbability(float InWeightedProb, int InNumPassengers) : WeightedProbability(InWeightedProb), NumPassengers(InNumPassengers) {};

	UPROPERTY(EditAnywhere, Category = "Probability")
	float WeightedProbability = 1.f;

	UPROPERTY(EditAnywhere, Category = "Probability")
	int NumPassengers = 1;
};

USTRUCT()
struct FPassengerSettingsFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	FPassengerSettingsFragment() = default;

	// Defines the passengers that can be inside of the vehicle
	UPROPERTY(EditAnywhere, Category = "Vehicle Passengers")
	TArray<UNPCDefinition*> PassengerDefinitions;
	
	/**
	 * Weighted probability of getting x passengers
	 */
	UPROPERTY(EditAnywhere, Category = "Vehicle Passengers", meta=(ShowOnlyInnerProperties, TitleProperty="{WeightedProbability} weighted chance for {NumPassengers} passenger(s)"))
	TArray<FPassengerWeightedProbability> PassengerCountProbability = { FPassengerWeightedProbability(1.f, 1) };

	bool IsValid() const
	{
		return PassengerDefinitions.Num() > 0 && PassengerCountProbability.Num() > 0;
	}
};

/**
 * Stores entity-specific data about passengers within the vehicle
 */
USTRUCT()
struct FVehiclePassengersFragment : public FMassFragment
{
	GENERATED_BODY()

	FVehiclePassengersFragment() = default;

	// Key is the seat index, value is the passenger definition
	TArray<TPair<int,UNPCDefinition*>> Passengers;
};

template<>
struct TMassFragmentTraits<FVehiclePassengersFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

/**
 * Used for getting weighted probability for # of passengers in vehicle
 */
struct FVehiclePassengerWeightedSampler : public FWeightedRandomSampler
{
	FVehiclePassengerWeightedSampler(const TArray<FPassengerWeightedProbability>& Probability);

	virtual float GetWeights(TArray<float>& OutWeights) override;

	TArray<FPassengerWeightedProbability> PassengerCountProbability;
};

// Tag applied to entities when we want to destroy them when at Low LOD
USTRUCT()
struct FAutoDestroyTag : public FMassTag
{
	GENERATED_BODY();
};
