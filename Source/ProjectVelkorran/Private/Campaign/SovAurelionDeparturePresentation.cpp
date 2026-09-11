// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovAurelionDeparturePresentation.h"
#include "Campaign/SovAurelionMissionDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovCompanionCommandActivity.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"

ASovAurelionDeparturePresentation::ASovAurelionDeparturePresentation()
{ PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .2f; }
void ASovAurelionDeparturePresentation::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || GetNetMode() != NM_Standalone || !GetWorld() || IsActorBeingDestroyed()) { return; }
    auto* PC = Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController());
    auto* State = PC ? PC->GetCampaignState() : nullptr;
    auto* Mission = State ? State->GetActiveMission() : nullptr;
    auto* Player = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    if (!State || !State->IsStateValid() || State->IsMutationInProgress()
        || !Mission || !Mission->IsA<USovAurelionContraryWitnessMissionDefinition>()
        || Mission->MissionId != TEXT("M13_ContraryWitness") || !State->IsMissionComplete(Mission->MissionId)
        || !State->IsBeatComplete(Mission->MissionId, TEXT("SeparateDepartures"))
        || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle
        || !IsValid(Player) || Player->GetController() != PC || !Player->IsCharacterReady() || !Player->IsAlive()) { return; }
    auto* CompanionOwner = PC->GetConvergenceCompanionState();
    auto* Companion = CompanionOwner ? CompanionOwner->GetActiveCompanion() : nullptr;
    auto* Component = IsValid(Companion) ? Companion->GetCompanionComponent() : nullptr;
    auto* AI = IsValid(Companion) ? Cast<ANarrativeNPCController>(Companion->GetController()) : nullptr;
    if (!IsValid(Companion) || Companion->IsActorBeingDestroyed() || Companion->GetWorld() != GetWorld()
        || Companion->GetOwner() != PC || !Companion->IsAlive() || !Companion->IsEncounterSnapshotReady()
        || !Component || Component->GetCurrentLeader() != Player || !AI || AI->GetPawn() != Companion
        || !AI->GetActivityComponent()) { return; }
    if (Component->HasAcceptedHoldPosition(Companion)) { return; }
    FString Reason;
    // Saved actor transforms restore through the companion owner. This only
    // restores ordinary standing intent after scene completion or checkpoint load.
    Component->RequestCommand(Player, ESovCompanionCommand::HoldPosition, Companion, Reason);
}
