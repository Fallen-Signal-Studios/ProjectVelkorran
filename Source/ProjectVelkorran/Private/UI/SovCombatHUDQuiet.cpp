// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/SovCombatHUDQuiet.h"
#include "AI/NarrativeNPCController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "UnrealFramework/NarrativeCharacter.h"

bool SovCombatHUDQuiet::HasNativeThreat(AActor* CurrentPawn)
{
    if (!IsValid(CurrentPawn) || CurrentPawn->IsActorBeingDestroyed() || !CurrentPawn->GetWorld()
        || CurrentPawn->GetWorld()->bIsTearingDown) { return false; }
    for (TActorIterator<ANarrativeNPCController> It(CurrentPawn->GetWorld()); It; ++It)
    {
        const auto* AI = *It;
        const auto* Source = Cast<ANarrativeCharacter>(AI->GetPawn());
        const auto* ASC = Source ? Source->GetNarrativeAbilitySystemComponent() : nullptr;
        if (AI->IsActorBeingDestroyed() || AI->IsThreatMemorySuspended() || !IsValid(Source)
            || Source->IsActorBeingDestroyed() || Source->IsHidden() || !Source->GetActorEnableCollision()
            || Source->GetWorld() != CurrentPawn->GetWorld() || Source->GetController() != AI
            || !IsValid(ASC) || ASC->GetAvatarActor() != Source || !Source->IsAlive()
            || !ASC->GetSet<UNarrativeAttributeSetBase>()) { continue; }
        FNarrativeThreatMemory Memory;
        // The existing getter rejects stale/wrong-world/nonhostile/dead targets and
        // expires confidence. Hearing/investigation can conservatively keep UI awake;
        // it does not authorize direct fire or expose the observation position.
        if (AI->GetBestThreatMemory(CurrentPawn, Memory) && Memory.Target.Get() == CurrentPawn) { return true; }
    }
    return false;
}

void SovCombatHUDQuiet::FState::Reset()
{ Pawn.Reset(); World.Reset(); Epoch=0; LastWake=0.; LastSample=-1.; }

void SovCombatHUDQuiet::FState::Wake(double Now)
{ if (FMath::IsFinite(Now)) { LastWake=Now; } }

float SovCombatHUDQuiet::FState::Update(AActor* CurrentPawn, uint64 ActorInfoEpoch, double Now,
    double ParticipationAge, bool bHasThreat, bool bCurrentAction)
{
    if (!IsValid(CurrentPawn) || CurrentPawn->IsActorBeingDestroyed() || !CurrentPawn->GetWorld()
        || CurrentPawn->GetWorld()->bIsTearingDown || !FMath::IsFinite(Now)
        || !FMath::IsFinite(ParticipationAge) || ParticipationAge < 0.) { Reset(); return 1.f; }
    if (Pawn.Get()!=CurrentPawn || World.Get()!=CurrentPawn->GetWorld() || Epoch!=ActorInfoEpoch
        || LastSample<0. || Now<LastSample)
    { Pawn=CurrentPawn; World=CurrentPawn->GetWorld(); Epoch=ActorInfoEpoch; LastWake=Now; }
    LastSample=Now;
    if (bHasThreat || bCurrentAction) { Wake(Now); }
    const double QuietFor=FMath::Min(ParticipationAge,FMath::Max(0.,Now-LastWake));
    // Six real game seconds, then a short opacity transition. No movement, pulse or delayed task.
    return float(1.-FMath::Clamp((QuietFor-6.)/.35,0.,1.));
}
