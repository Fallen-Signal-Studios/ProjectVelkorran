// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UI/SovObjectivePresentationTypes.h"

class AActor;
class ASovPlayerController;
class USceneComponent;

/** Navigation hints do not authorize interaction or supply an encounter/campaign receipt. */
struct FSovObjectiveWaypoint
{
    enum class EKind : uint8 { Interaction, Encounter, Receiver, Retry };
    FName MissionId, BeatId;
    TWeakObjectPtr<AActor> Source, Target;
    TWeakObjectPtr<USceneComponent> Anchor;
    EKind Kind = EKind::Interaction;
};

namespace SovObjectiveWaypoint
{
    PROJECTVELKORRAN_API bool IsSupportedSource(const AActor* Actor);
    /** Entries are the frontend's already-authorized view; state/knowledge are checked again here. */
    PROJECTVELKORRAN_API bool Resolve(const ASovPlayerController* Controller,
        const TArray<FSovObjectivePresentationEntry>& Entries,
        const TArray<TWeakObjectPtr<AActor>>& Sources, FSovObjectiveWaypoint& Out);
    PROJECTVELKORRAN_API bool IsCurrent(const ASovPlayerController* Controller, const FSovObjectiveWaypoint& Waypoint);
    /** Behind-camera bearings never mirror onto an apparent world target. */
    PROJECTVELKORRAN_API bool FitToSafeRect(FVector2D Projected, bool bProjectedInFront, FVector2D Bearing,
        FVector2D SafeMin, FVector2D SafeMax, FVector2D& Position, bool& bAtEdge);
}
