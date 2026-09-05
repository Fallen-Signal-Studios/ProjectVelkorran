// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "NarrativeSavePhases.generated.h"

/** Ordering policy within the existing Narrative record owner, not another world-state store. */
UENUM(BlueprintType)
enum class ENarrativeRestorePhase : uint8
{
    World,
    Structure,
    Interactables,
    Encounters,
    Companions,
    Player,
    Presentation,
    MissionResume
};
