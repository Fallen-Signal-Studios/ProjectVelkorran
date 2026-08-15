// Copyright Narrative Tools 2024. 

#pragma once

#include "EntityLOD.generated.h"

UENUM(BlueprintType)
enum class EEntityLOD : uint8
{
	High,
	Medium,
	Low,
	Off,
	Max
};