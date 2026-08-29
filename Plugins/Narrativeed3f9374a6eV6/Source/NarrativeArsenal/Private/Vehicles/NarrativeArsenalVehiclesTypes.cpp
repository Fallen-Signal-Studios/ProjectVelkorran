// Copyright Narrative Tools 2025.

#include "Vehicles/NarrativeArsenalVehicleTypes.h"
#include "Math/Float16.h"

FVehicleInstanceCustomData::FVehicleInstanceCustomData( const struct FPackedVehicleInstanceCustomData& PackedCustomData)
{
	const uint32& PackedParam1AsUint32 = reinterpret_cast<const uint32&>(PackedCustomData.PackedParam1);

	// Unpack half precision random fraction 
	FFloat16 HalfPrecisionRandomFraction;
	HalfPrecisionRandomFraction.Encoded = PackedParam1AsUint32;
	RandomFraction = HalfPrecisionRandomFraction;

	// Get light state bits
	bFrontLeftRunningLights = PackedParam1AsUint32 & 1UL << (16 + 0);
	bFrontRightRunningLights = PackedParam1AsUint32 & 1UL << (16 + 1);
	bRearLeftRunningLights = PackedParam1AsUint32 & 1UL << (16 + 2);
	bRearRightRunningLights = PackedParam1AsUint32 & 1UL << (16 + 3);
	bLeftBrakeLights = PackedParam1AsUint32 & 1UL << (16 + 4);
	bRightBrakeLights = PackedParam1AsUint32 & 1UL << (16 + 5);
	bLeftTurnSignalLights = PackedParam1AsUint32 & 1UL << (16 + 6);
	bRightTurnSignalLights = PackedParam1AsUint32 & 1UL << (16 + 7);
	bLeftHeadlight = PackedParam1AsUint32 & 1UL << (16 + 8);
	bRightHeadlight = PackedParam1AsUint32 & 1UL << (16 + 9);
	bReversingLights = PackedParam1AsUint32 & 1UL << (16 + 10);
	bAccessoryLights = PackedParam1AsUint32 & 1UL << (16 + 11); 
}

FPackedVehicleInstanceCustomData::FPackedVehicleInstanceCustomData(const FVehicleInstanceCustomData& UnpackedCustomData)
{
	uint32& PackedParam1AsUint32 = reinterpret_cast<uint32&>(PackedParam1);

	// Encode RandomFraction as 16-bit float in 16 least significant bits
	const FFloat16 HalfPrecisionRandomFraction = UnpackedCustomData.RandomFraction;
	PackedParam1AsUint32 = static_cast<uint32>(HalfPrecisionRandomFraction.Encoded);
	
	// Set light state bits
	if (UnpackedCustomData.bFrontLeftRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 0);
	}
	if (UnpackedCustomData.bFrontRightRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 1);
	}
	if (UnpackedCustomData.bRearLeftRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 2);
	}
	if (UnpackedCustomData.bRearRightRunningLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 3);
	}
	if (UnpackedCustomData.bLeftBrakeLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 4);
	}
	if (UnpackedCustomData.bRightBrakeLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 5);
	}
	if (UnpackedCustomData.bLeftTurnSignalLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 6);
	}
	if (UnpackedCustomData.bRightTurnSignalLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 7);
	}
	if (UnpackedCustomData.bLeftHeadlight)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 8);
	}
	if (UnpackedCustomData.bRightHeadlight)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 9);
	}
	if (UnpackedCustomData.bReversingLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 10);
	}
	if (UnpackedCustomData.bAccessoryLights)
	{
		PackedParam1AsUint32 |= 1UL << (16 + 11); 
	}
}
