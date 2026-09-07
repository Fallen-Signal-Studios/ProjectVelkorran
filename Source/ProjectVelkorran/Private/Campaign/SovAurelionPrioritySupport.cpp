// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionPrioritySupport.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"

ASovAurelionPrioritySupport::ASovAurelionPrioritySupport()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .25f;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    WestCacheBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("WestCacheBarrier"));
    EastFlankBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("EastFlankBarrier"));
    for (UBoxComponent* Barrier : { WestCacheBarrier.Get(), EastFlankBarrier.Get() })
    {
        Barrier->SetupAttachment(SceneRoot); Barrier->SetBoxExtent(FVector(50.f, 150.f, 175.f));
        Barrier->SetCollisionProfileName(TEXT("BlockAllDynamic")); Barrier->SetGenerateOverlapEvents(false);
    }
    WestCacheBarrier->SetRelativeLocation(FVector(0.f, -400.f, 175.f));
    EastFlankBarrier->SetRelativeLocation(FVector(0.f, 400.f, 175.f));
}
const USovCampaignStateComponent* ASovAurelionPrioritySupport::Campaign() const
{
    const auto* PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    return IsValid(PC) && !PC->IsActorBeingDestroyed() ? PC->GetCampaignState() : nullptr;
}
ESovAurelionRescuePriority ASovAurelionPrioritySupport::ReadPriority(const USovCampaignStateComponent* State)
{
    if (!IsValid(State) || !State->IsStateValid()) { return ESovAurelionRescuePriority::Unset; }
    const FName Selected = State->GetSelectedChoice(TEXT("M12_FireAndFrost"), TEXT("ImmediateProtection"));
    if (Selected == TEXT("PriorityWestStretchers")) { return ESovAurelionRescuePriority::WestStretchers; }
    if (Selected == TEXT("PriorityEastWalkers")) { return ESovAurelionRescuePriority::EastWalkers; }
    return ESovAurelionRescuePriority::Unset;
}
FName ASovAurelionPrioritySupport::OutcomeBeat(ESovAurelionRescuePriority Priority)
{
    if (Priority == ESovAurelionRescuePriority::WestStretchers) { return TEXT("PriorityWestStretchers"); }
    if (Priority == ESovAurelionRescuePriority::EastWalkers) { return TEXT("PriorityEastWalkers"); }
    return NAME_None;
}
ESovAurelionRescuePriority ASovAurelionPrioritySupport::GetPriority() const { return ReadPriority(Campaign()); }
bool ASovAurelionPrioritySupport::IsWestCacheAccessible() const
{ return GetPriority() == ESovAurelionRescuePriority::WestStretchers; }
bool ASovAurelionPrioritySupport::IsEastFlankOpen() const
{
    const auto* State = Campaign();
    return ReadPriority(State) == ESovAurelionRescuePriority::EastWalkers
        || (State && State->IsStateValid() && State->IsBeatComplete(TEXT("M12_FireAndFrost"), TEXT("HandoffToTarrikCrucible")));
}
FName ASovAurelionPrioritySupport::GetAftermathConsequenceId() const
{
    switch (GetPriority())
    {
    case ESovAurelionRescuePriority::WestStretchers: return TEXT("M12_WestStretchersPrioritized");
    case ESovAurelionRescuePriority::EastWalkers: return TEXT("M12_EastWalkersPrioritized");
    default: return NAME_None;
    }
}
void ASovAurelionPrioritySupport::RefreshFromCampaign()
{
    if (bRefreshing || !HasAuthority() || GetNetMode() != NM_Standalone || IsActorBeingDestroyed()) { return; }
    TGuardValue<bool> Refreshing(bRefreshing, true);
    // No visibility or actor-lifetime changes: an already consumed cache stays consumed.
    const ECollisionEnabled::Type West = IsWestCacheAccessible() ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics;
    if (IsValid(WestCacheBarrier) && WestCacheBarrier->GetCollisionEnabled() != West) { WestCacheBarrier->SetCollisionEnabled(West); }
    if (!IsValid(this) || IsActorBeingDestroyed()) { return; }
    // Re-read after collision callbacks rather than caching a possibly retired decision.
    const ECollisionEnabled::Type East = IsEastFlankOpen() ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics;
    if (IsValid(EastFlankBarrier) && EastFlankBarrier->GetCollisionEnabled() != East) { EastFlankBarrier->SetCollisionEnabled(East); }
}
void ASovAurelionPrioritySupport::BeginPlay() { Super::BeginPlay(); RefreshFromCampaign(); }
void ASovAurelionPrioritySupport::Tick(float DeltaSeconds) { Super::Tick(DeltaSeconds); RefreshFromCampaign(); }
