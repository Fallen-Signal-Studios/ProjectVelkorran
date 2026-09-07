// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionCheckpoint.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Save/SovSaveSubsystem.h"

namespace
{
    struct FBinding { const TCHAR* Id; const TCHAR* Mission; const TCHAR* Before; const TCHAR* After; };
    FBinding Binding(ESovAurelionCheckpoint Boundary)
    {
        switch (Boundary)
        {
        case ESovAurelionCheckpoint::ContextCP0: return { TEXT("Aurelion.CP0"), TEXT("M12_FireAndFrost"), TEXT(""), TEXT("TarrikArrival") };
        case ESovAurelionCheckpoint::SeleneEntryCP2: return { TEXT("Aurelion.CP2"), TEXT("M12_FireAndFrost"), TEXT("HandoffToSelene"), TEXT("SeleneArrival") };
        case ESovAurelionCheckpoint::MeetingCP3: return { TEXT("Aurelion.CP3"), TEXT("M12_FireAndFrost"), TEXT("RelayOverlook"), TEXT("MeetingAndCarrierRescue") };
        case ESovAurelionCheckpoint::QuarantineCP6: return { TEXT("Aurelion.CP6"), TEXT("M12_FireAndFrost"), TEXT("SurvivorsClearAndQuarantine"), TEXT("ContraryWitnessRecognized") };
        case ESovAurelionCheckpoint::CoreCP7: return { TEXT("Aurelion.CP7"), TEXT("M13_ContraryWitness"), TEXT("GrammarPropagation"), TEXT("VoluntaryStay") };
        case ESovAurelionCheckpoint::ConversationCP8: return { TEXT("Aurelion.CP8"), TEXT("M13_ContraryWitness"), TEXT("VoluntaryStay"), TEXT("CauldronRecorderReceived") };
        case ESovAurelionCheckpoint::DepartureCP9: return { TEXT("Aurelion.CP9"), TEXT("M13_ContraryWitness"), TEXT("SeparateDepartures"), TEXT("") };
        default: return { TEXT(""), TEXT(""), TEXT(""), TEXT("") };
        }
    }
}
ASovAurelionCheckpoint::ASovAurelionCheckpoint()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .25f;
    Threshold = CreateDefaultSubobject<UBoxComponent>(TEXT("Threshold")); SetRootComponent(Threshold);
    Threshold->SetBoxExtent(FVector(150, 200, 120)); Threshold->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Threshold->SetCollisionResponseToAllChannels(ECR_Ignore); Threshold->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Threshold->SetGenerateOverlapEvents(true); Threshold->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
}
FName ASovAurelionCheckpoint::GetBoundaryId() const { return FName(Binding(Checkpoint).Id); }
bool ASovAurelionCheckpoint::MatchesProgress(ESovAurelionCheckpoint Boundary, const USovCampaignStateComponent* State)
{
    const auto B = Binding(Boundary); const FName MissionId(B.Mission), Prior(B.Before), Next(B.After);
    if (!IsValid(State) || !State->IsStateValid() || State->IsMutationInProgress() || MissionId.IsNone()
        || !State->GetActiveMission() || State->GetActiveMission()->MissionId != MissionId) { return false; }
    if ((!Prior.IsNone() && !State->GetActiveMission()->FindBeat(Prior))
        || (!Next.IsNone() && !State->GetActiveMission()->FindBeat(Next))) { return false; }
    return (Prior.IsNone() || State->IsBeatComplete(MissionId, Prior))
        && (Next.IsNone() || !State->IsBeatComplete(MissionId, Next));
}
bool ASovAurelionCheckpoint::RequestCheckpoint(ASovPlayerController* Controller, FString& Error)
{
    Error.Reset();
    const auto Reject = [this, &Error](const TCHAR* Message) { Error = Message; LastError = Error; return false; };
    auto* Player = IsValid(Controller) ? Cast<ASovPlayerCharacterBase>(Controller->GetPawn()) : nullptr;
    auto* ASC = IsValid(Player) ? Cast<UNarrativeAbilitySystemComponent>(Player->GetAbilitySystemComponent()) : nullptr;
    auto* State = IsValid(Controller) ? Controller->GetCampaignState() : nullptr;
    if (!HasAuthority() || GetNetMode() != NM_Standalone || bWriting || IsActorBeingDestroyed()
        || !IsValid(Controller) || Controller->IsActorBeingDestroyed() || !IsValid(Player) || Player->IsActorBeingDestroyed()
        || Controller->GetWorld() != GetWorld() || Player->GetController() != Controller || !ASC || ASC->GetAvatarActor() != Player
        || !Player->IsCharacterReady() || !Player->IsAlive() || !MatchesProgress(Checkpoint, State)
        || State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag()
        || Controller->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle)
    { return Reject(TEXT("Aurelion checkpoint requires its current ready protagonist and exact route boundary.")); }
    if (!ContainsPlayer(Player))
    { return Reject(TEXT("Move inside the authored checkpoint threshold.")); }
    const FName Boundary = GetBoundaryId();
    for (TActorIterator<ASovAurelionCheckpoint> It(GetWorld()); It; ++It)
    { if (*It != this && !It->IsActorBeingDestroyed() && It->GetBoundaryId() == Boundary)
        { return Reject(TEXT("Aurelion checkpoint identity is ambiguous.")); } }
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
    if (!Saves) { return Reject(TEXT("The campaign save owner is unavailable.")); }
    if (ObservedState.Get() != State)
    {
        if (ObservedState.IsValid()) { ObservedState->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleStateRestored); }
        ObservedState = State;
        State->OnCampaignStateRestored.AddUniqueDynamic(this, &ThisClass::HandleStateRestored);
        ++StateEpoch;
    }
    TGuardValue<bool> Writing(bWriting, true);
    const uint64 RestoreEpoch = StateEpoch;
    const uint64 ActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
    const uint64 Transition = Controller->GetCampaignTransitionEpoch();
    const int32 Ready = ASC->GetCharacterReadyEpoch(); const int32 JournalCount = State->GetJournal().Num();
    auto* Mission = State->GetActiveMission();
    const bool bSaved = Saves->WriteCheckpoint(ESovSaveBoundary::ExplicitCheckpoint, Boundary, Error) == ESovSaveResult::Success;
    if (!IsValid(this) || IsActorBeingDestroyed() || !IsValid(Controller) || !IsValid(Player) || !IsValid(ASC)
        || Controller->GetPawn() != Player || Player->GetAbilitySystemComponent() != ASC || ASC->GetAvatarActor() != Player
        || Controller->GetCampaignTransitionEpoch() != Transition || ASC->GetCharacterReadyEpoch() != Ready
        || ASC->GetCombatActorInfoEpoch() != ActorInfoEpoch
        || Controller->GetCampaignState() != State || !IsValid(State) || State->GetActiveMission() != Mission
        || State->GetJournal().Num() != JournalCount || GetBoundaryId() != Boundary || StateEpoch != RestoreEpoch
        || ObservedState.Get() != State)
    { Error = TEXT("Checkpoint context changed during storage publication."); return false; }
    LastError = Error; return bSaved;
}
void ASovAurelionCheckpoint::HandleOverlap(UPrimitiveComponent* Component, AActor* Actor, UPrimitiveComponent* Other,
    int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
    // Polling also covers spawning/restoring inside the volume before the pawn is ready.
    if (Cast<ASovPlayerCharacterBase>(Actor)) { bAutoAttempted = false; }
}
bool ASovAurelionCheckpoint::ContainsPlayer(const AActor* Player) const
{
    if (!IsValid(Player) || !IsValid(Threshold) || !Threshold->IsRegistered()
        || Threshold->GetComponentTransform().ContainsNaN() || Player->GetActorLocation().ContainsNaN()) { return false; }
    const FVector Scale = Threshold->GetComponentTransform().GetScale3D();
    if (FMath::Abs(Scale.X) <= SMALL_NUMBER || FMath::Abs(Scale.Y) <= SMALL_NUMBER || FMath::Abs(Scale.Z) <= SMALL_NUMBER) { return false; }
    const FVector Point = Threshold->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation());
    const FVector Extent = Threshold->GetUnscaledBoxExtent();
    return !Extent.ContainsNaN() && !Point.ContainsNaN() && Extent.X > 0 && Extent.Y > 0 && Extent.Z > 0
        && FMath::Abs(Point.X) <= Extent.X && FMath::Abs(Point.Y) <= Extent.Y && FMath::Abs(Point.Z) <= Extent.Z;
}
void ASovAurelionCheckpoint::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bCaptureOnOverlap || !HasAuthority() || GetNetMode() != NM_Standalone || bWriting || !GetWorld()) { return; }
    auto* Controller = Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController());
    auto* Player = Controller ? Cast<ASovPlayerCharacterBase>(Controller->GetPawn()) : nullptr;
    if (!ContainsPlayer(Player)) { bAutoAttempted = false; return; }
    if (bAutoAttempted || !Player->IsCharacterReady() || !Player->IsAlive()
        || Controller->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || !MatchesProgress(Checkpoint, Controller->GetCampaignState())) { return; }
    auto* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
    FString Error;
    if (!Saves || !Saves->CanCapture(Error)) { return; }
    // One write attempt per visit; a storage failure belongs to the existing save-failure UI.
    bAutoAttempted = true;
    RequestCheckpoint(Controller, Error);
}
void ASovAurelionCheckpoint::HandleStateRestored(bool bValid) { ++StateEpoch; bAutoAttempted = false; }
void ASovAurelionCheckpoint::EndPlay(EEndPlayReason::Type Reason)
{
    if (ObservedState.IsValid()) { ObservedState->OnCampaignStateRestored.RemoveDynamic(this, &ThisClass::HandleStateRestored); }
    Super::EndPlay(Reason);
}
