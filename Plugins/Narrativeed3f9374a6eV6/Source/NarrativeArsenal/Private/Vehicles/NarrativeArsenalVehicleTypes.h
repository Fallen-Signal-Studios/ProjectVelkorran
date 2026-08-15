// Copyright Narrative Tools 2025.

#pragma once

#include "NarrativeArsenalVehicleTypes.generated.h"


/*
* Vehicle visualization parameters to be passed to vehicle ISMCs as PerInstanceCustomData and PrimitiveComponent's
* via UPrimitiveComponent::SetCustomPrimitiveDataFloat. Note, these raw values aren't passed directly - they're passed
* as packed data via FMassPackedVehicleInstanceCustomData
* 
* @see FMassPackedVehicleInstanceCustomData
*/
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FVehicleInstanceCustomData
{
	GENERATED_BODY()

	FVehicleInstanceCustomData() = default;
	
	FVehicleInstanceCustomData(const struct FPackedVehicleInstanceCustomData& PackedCustomData);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	float RandomFraction = 0.0f; // Packed as FFloat16 into PackedParam1[0 : 15]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bFrontLeftRunningLights = false; // PackedParam1[16 + 0]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bFrontRightRunningLights = false; // PackedParam1[16 + 1]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bRearLeftRunningLights = false; // PackedParam1[16 + 2]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bRearRightRunningLights = false; // PackedParam1[16 + 3]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bLeftBrakeLights = false; // PackedParam1[16 + 4]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bRightBrakeLights = false; // PackedParam1[16 + 5]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bLeftTurnSignalLights = false; // PackedParam1[16 + 6]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bRightTurnSignalLights = false; // PackedParam1[16 + 7]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bLeftHeadlight = false; // PackedParam1[16 + 8]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bRightHeadlight = false; // PackedParam1[16 + 9]
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bReversingLights = false; // PackedParam1[16 + 10]

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Vehicle Instance Data")
	bool bAccessoryLights = false; // PackedParam1[16 + 11] - Max is 15 !
};

/**
 * FVehicleInstanceCustomData packed into a single 32 bit float to be passed as ISMC PerInstanceCustomData
 * which is currently limited to a single float for Nanite ISMCs. We also pass this to PrimitiveComponent's via 
 * UPrimitiveComponent::SetCustomPrimitiveDataFloat
 *
 * @see FVehicleInstanceCustomData
 */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FPackedVehicleInstanceCustomData
{
	GENERATED_BODY()

	FPackedVehicleInstanceCustomData() {};
	
	explicit FPackedVehicleInstanceCustomData(const float InPackedParam1)
		: PackedParam1(InPackedParam1) {}
	
	FPackedVehicleInstanceCustomData(const FVehicleInstanceCustomData& UnpackedCustomData);

	/**
	 * Bit packed param with RandomFraction packed into the least significant
	 * bits
	 * e.g: [ 0000000000000000 | VisualizationFlags (not used) | RandomFraction ]
	 */
	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, Category = "Vehicle Instance Data")
	float PackedParam1 = 0.0f;
};