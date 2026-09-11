// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
class AActor;
class UWorld;

namespace SovCombatHUDQuiet
{
/** Positive native threat memory keeps the HUD visible; this never admits an attack. */
PROJECTVELKORRAN_API bool HasNativeThreat(AActor* CurrentPawn);

/** Local presentation timing only. Native Echo owns participation; native AI owns threats. */
struct PROJECTVELKORRAN_API FState
{
    void Reset();
    void Wake(double Now);
    float Update(AActor* CurrentPawn, uint64 ActorInfoEpoch, double Now,
        double ParticipationAge, bool bHasThreat, bool bCurrentAction);
private:
    TWeakObjectPtr<AActor> Pawn;
    TWeakObjectPtr<UWorld> World;
    uint64 Epoch = 0;
    double LastWake = 0., LastSample = -1.;
};
}
