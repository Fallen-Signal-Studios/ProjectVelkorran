// Copyright Fallen Signal Studios. All Rights Reserved.
#include "AI/SovAurelionSweepScanner.h"
#include "AI/NarrativeNPCController.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovDroneNPCBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UI/SovAccessibilityPresentation.h"
#include "UI/SovFrontendComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "SovAurelionScanner"

ASovAurelionSweepScanner::ASovAurelionSweepScanner()
{
    PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = .05f;
    auto* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(Scene);
    Cone = CreateDefaultSubobject<USpotLightComponent>(TEXT("ScanCone")); Cone->SetupAttachment(Scene);
    Cone->SetMobility(EComponentMobility::Movable); Cone->SetIntensity(18000.f);
    Cone->SetCastShadows(true); Cone->SetVolumetricScatteringIntensity(2.f);
    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ScannerHead")); Head->SetupAttachment(Cone);
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision); Head->SetRelativeScale3D(FVector(.35,.35,.35));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded()) { Head->SetStaticMesh(Sphere.Object); }
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ScannerLabel")); Label->SetupAttachment(Scene);
    Label->SetRelativeLocation(FVector(0.,0.,50.)); Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(24.f);
}

bool ASovAurelionSweepScanner::ValidateConfiguration(FString& Error) const
{
    Error.Reset();
    if (ScannerId.IsNone() || MissionId.IsNone() || !IsValid(RelayDirector) || RelayDirector->GetWorld() != GetWorld()
        || RelayDroneIds.Num() != 2 || RelayDroneIds[0].IsNone() || RelayDroneIds[1].IsNone() || RelayDroneIds[0] == RelayDroneIds[1])
    { Error = TEXT("Scanner requires an ID, mission and exactly two unique relay participant IDs."); return false; }
    if (!FMath::IsFinite(ScanRange) || ScanRange < 100.f || ScanRange > 3000.f
        || !FMath::IsFinite(ConeHalfAngle) || ConeHalfAngle < 5.f || ConeHalfAngle > 45.f
        || !FMath::IsFinite(SweepHalfArc) || SweepHalfArc < 0.f || SweepHalfArc > 80.f
        || !FMath::IsFinite(SweepPeriod) || SweepPeriod < 1.f || SweepPeriod > 20.f
        || !FMath::IsFinite(Pitch) || Pitch < -60.f || Pitch > 30.f
        || !FMath::IsFinite(PhaseOffset) || PhaseOffset < 0.f || PhaseOffset > 1.f
        || !FMath::IsFinite(ObservationLifetime) || ObservationLifetime < 1.f || ObservationLifetime > 30.f
        || GetActorTransform().ContainsNaN() || !GetActorScale3D().Equals(FVector::OneVector))
    { Error = TEXT("Scanner geometry or finite observation lifetime is invalid."); return false; }
    for (const FName Id : RelayDroneIds)
    {
        const auto* Drone = Cast<ASovDroneNPCBase>(RelayDirector->GetParticipant(Id));
        if (!IsValid(Drone) || Drone->GetWorld() != GetWorld() || RelayDirector->FindParticipantId(Drone) != Id)
        { Error = TEXT("Scanner recipients must be the actual registered relay drones."); return false; }
    }
    return true;
}

bool ASovAurelionSweepScanner::GetCurrentPlayer(ASovPlayerCharacterBase*& Player, ASovPlayerController*& PC) const
{
    PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    Player = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
    const auto* ASC = Player ? Player->GetNarrativeAbilitySystemComponent() : nullptr;
    const auto* Campaign = PC ? PC->GetCampaignState() : nullptr;
    const auto* Mission = Campaign ? Campaign->GetActiveMission() : nullptr;
    const auto& NarrativeTags = FNarrativeGameplayTags::Get();
    return IsValid(PC) && !PC->IsActorBeingDestroyed() && IsValid(Player) && !Player->IsActorBeingDestroyed()
        && Player->GetWorld() == GetWorld() && Player->GetController() == PC && Player->IsCharacterReady() && Player->IsAlive()
        && !Player->IsHidden() && Player->GetActorEnableCollision() && ASC && ASC->GetAvatarActor() == Player
        && Player->GetProtagonistIdentityTag() == FSovGameplayTags::Get().Character_Player_Selene
        && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle
        && Campaign && Campaign->IsStateValid() && !Campaign->IsMutationInProgress() && Mission && Mission->MissionId == MissionId
        && Campaign->GetActiveProtagonist() == Player->GetProtagonistIdentityTag()
        && !ASC->HasMatchingGameplayTag(NarrativeTags.State_InvisibleToEnemies)
        && !ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
        && !ASC->HasMatchingGameplayTag(NarrativeTags.State_DialogueControlled);
}

bool ASovAurelionSweepScanner::CanSee(const ASovPlayerCharacterBase* Player, FVector& SeenPosition) const
{
    if (!IsValid(Player) || !Cone || !Cone->IsRegistered()) { return false; }
    SeenPosition = Player->GetActorLocation();
    const FVector Origin = Cone->GetComponentLocation();
    const FVector Offset = SeenPosition - Origin;
    if (SeenPosition.ContainsNaN() || Offset.SizeSquared() > FMath::Square(ScanRange)
        || Offset.SizeSquared() < 1. || FVector::DotProduct(Cone->GetForwardVector(), Offset.GetSafeNormal())
            < FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle))) { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AurelionScannerVisibility), false, this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, SeenPosition, ECC_Visibility, Query) || Hit.GetActor() == Player;
}

bool ASovAurelionSweepScanner::CaptureObservation()
{
    ASovPlayerCharacterBase* Player = nullptr; ASovPlayerController* PC = nullptr; FVector SeenPosition;
    if (!GetCurrentPlayer(Player, PC) || !CanSee(Player, SeenPosition)) { return false; }
    const auto State = RelayDirector->GetEncounterState();
    if (State != ESovEncounterState::Inactive && State != ESovEncounterState::Active) { return false; }
    if (State == ESovEncounterState::Active && !RelayDirector->HasEncounterPlayer(Player)) { return false; }
    FObservation Next;
    Next.ScannerId = ScannerId; Next.SensorTransform = GetActorTransform(); Next.Director = RelayDirector;
    Next.DirectorGeneration = RelayDirector->GetLifecycleGeneration(); Next.Attempt = RelayDirector->GetAttemptId();
    Next.bEncounterWasActive = State == ESovEncounterState::Active;
    Next.Player = Player; Next.Controller = PC; Next.ASC = Player->GetNarrativeAbilitySystemComponent();
    Next.ReadyEpoch = Next.ASC->GetCharacterReadyEpoch(); Next.ActorInfoEpoch = Next.ASC->GetCombatActorInfoEpoch();
    Next.TransitionEpoch = PC->GetCampaignTransitionEpoch(); Next.Mission = PC->GetCampaignState()->GetActiveMission();
    Next.Position = SeenPosition; Next.Lifetime = ObservationLifetime; Next.ExpiresAt = GetWorld()->GetTimeSeconds() + ObservationLifetime;
    for (const FName Id : RelayDroneIds)
    {
        auto* Drone = Cast<ASovDroneNPCBase>(RelayDirector->GetParticipant(Id));
        auto* AI = Drone ? Cast<ANarrativeNPCController>(Drone->GetController()) : nullptr;
        auto* ASC = Drone ? Drone->GetNarrativeAbilitySystemComponent() : nullptr;
        if (!IsValid(Drone) || !Drone->IsAlive() || !Drone->IsEncounterSnapshotReady() || !AI || AI->GetPawn() != Drone
            || !AI->bAcceptNetworkThreats || !ASC || ASC->GetAvatarActor() != Drone) { return false; }
        FReceiver& Receiver = Next.Receivers.AddDefaulted_GetRef();
        Receiver.Id = Id; Receiver.Pawn = Drone; Receiver.Controller = AI; Receiver.ASC = ASC;
        Receiver.ActorInfoEpoch = ASC->GetCombatActorInfoEpoch();
    }
    Observation = MoveTemp(Next); bPending = true;
    PresentCue(PC, LOCTEXT("Detected", "Scanner detected you. The relay is on alert."));
    return true;
}

bool ASovAurelionSweepScanner::OwnsObservation() const
{
    ASovPlayerCharacterBase* Player = nullptr; ASovPlayerController* PC = nullptr;
    if (!bPending || bEnding || !bEnabled || IsActorBeingDestroyed() || !GetWorld()
        || GetWorld()->GetTimeSeconds() >= Observation.ExpiresAt || !GetCurrentPlayer(Player, PC)
        || Observation.ScannerId != ScannerId || !Observation.SensorTransform.Equals(GetActorTransform())
        || Observation.Director.Get() != RelayDirector || !IsValid(RelayDirector) || RelayDirector->IsActorBeingDestroyed()
        || Observation.DirectorGeneration != RelayDirector->GetLifecycleGeneration()
        || Observation.Player.Get() != Player || Observation.Controller.Get() != PC
        || Observation.Mission.Get() != PC->GetCampaignState()->GetActiveMission()
        || Observation.TransitionEpoch != PC->GetCampaignTransitionEpoch()
        || Observation.ASC.Get() != Player->GetNarrativeAbilitySystemComponent()
        || Observation.ASC->GetCombatActorInfoEpoch() != Observation.ActorInfoEpoch
        || Observation.ASC->GetCharacterReadyEpoch() != Observation.ReadyEpoch
        || RelayDroneIds.Num() != Observation.Receivers.Num()) { return false; }
    const auto State = RelayDirector->GetEncounterState();
    if (State != ESovEncounterState::Inactive && State != ESovEncounterState::Active) { return false; }
    if (Observation.bEncounterWasActive && (State != ESovEncounterState::Active || Observation.Attempt != RelayDirector->GetAttemptId())) { return false; }
    if (Observation.Attempt.IsValid() && Observation.Attempt != RelayDirector->GetAttemptId()) { return false; }
    for (int32 Index = 0; Index < Observation.Receivers.Num(); ++Index)
    {
        const auto& Receiver = Observation.Receivers[Index];
        if (Receiver.Id != RelayDroneIds[Index] || !Receiver.Pawn.IsValid() || !Receiver.Controller.IsValid() || !Receiver.ASC.IsValid()
            || RelayDirector->GetParticipant(Receiver.Id) != Receiver.Pawn.Get() || !Receiver.Pawn->IsAlive()
            || !Receiver.Pawn->IsEncounterSnapshotReady() || Receiver.Pawn->GetController() != Receiver.Controller.Get()
            || Receiver.Controller->GetPawn() != Receiver.Pawn.Get() || !Receiver.Controller->bAcceptNetworkThreats
            || Receiver.Pawn->GetNarrativeAbilitySystemComponent() != Receiver.ASC.Get()
            || Receiver.ASC->GetAvatarActor() != Receiver.Pawn.Get() || Receiver.ASC->GetCombatActorInfoEpoch() != Receiver.ActorInfoEpoch) { return false; }
    }
    return true;
}

void ASovAurelionSweepScanner::DeliverObservation()
{
    if (!OwnsObservation()) { ClearObservation(); return; }
    if (RelayDirector->GetEncounterState() != ESovEncounterState::Active) { return; }
    if (!RelayDirector->HasEncounterPlayer(Observation.Player.Get()) || !RelayDirector->GetAttemptId().IsValid()) { ClearObservation(); return; }
    Observation.Attempt = RelayDirector->GetAttemptId(); Observation.bEncounterWasActive = true;
    const bool bWasComplete = GetAlertedRecipientCount() == 2;
    for (int32 Index = 0; Index < Observation.Receivers.Num(); ++Index)
    {
        if (!OwnsObservation()) { ClearObservation(); return; }
        const auto Receiver = Observation.Receivers[Index];
        const auto& NarrativeTags = FNarrativeGameplayTags::Get();
        if (Receiver.bDelivered || Receiver.Controller->IsThreatMemorySuspended()
            || Receiver.ASC->HasMatchingGameplayTag(NarrativeTags.State_Busy)
            || Receiver.ASC->HasMatchingGameplayTag(NarrativeTags.State_SequencerControlled)
            || Receiver.Pawn->IsHidden() || !Receiver.Pawn->GetActorEnableCollision()) { continue; }
        const float Remaining = static_cast<float>(Observation.ExpiresAt - GetWorld()->GetTimeSeconds());
        // Delayed admission must not rejuvenate the captured observation or sample unseen movement.
        const bool bAccepted = Receiver.Controller->ReportThreatObservation(Observation.Player.Get(), ENarrativeThreatSource::NetworkSensor,
            Observation.Position, 1.f, .8f * Remaining / Observation.Lifetime, Remaining);
        if (!OwnsObservation()) { ClearObservation(); return; }
        if (bAccepted) { Observation.Receivers[Index].bDelivered = true; }
    }
    if (!bWasComplete && GetAlertedRecipientCount() == 2)
    { PresentCue(Observation.Controller.Get(), LOCTEXT("Delivered", "Relay drones alerted.")); }
}

void ASovAurelionSweepScanner::ClearObservation() { bPending = false; Observation = FObservation(); }
int32 ASovAurelionSweepScanner::GetAlertedRecipientCount() const
{ int32 Count = 0; if (bPending) { for (const auto& Receiver : Observation.Receivers) { Count += Receiver.bDelivered ? 1 : 0; } } return Count; }
void ASovAurelionSweepScanner::PresentCue(ASovPlayerController* PC, const FText& Text)
{
    if (PC && PC->IsLocalController() && PC->GetFrontend())
    { if (auto* Presentation = PC->GetFrontend()->GetPresentation()) { Presentation->PresentCaption(Text, 4.f, GetActorLocation()); } }
}
void ASovAurelionSweepScanner::UpdatePresentation()
{
    if (!Cone || !Label) { return; }
    const double Seconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.;
    const double Yaw = FMath::IsFinite(SweepPeriod) && SweepPeriod > 0.f
        ? SweepHalfArc * FMath::Sin(2. * UE_DOUBLE_PI * (Seconds / SweepPeriod + PhaseOffset)) : 0.f;
    Cone->SetRelativeRotation(FRotator(Pitch, Yaw, 0.f)); Cone->SetAttenuationRadius(ScanRange);
    Cone->SetInnerConeAngle(ConeHalfAngle * .8f); Cone->SetOuterConeAngle(ConeHalfAngle);
    Cone->SetVisibility(bEnabled);
    const FLinearColor Color = bPending ? FLinearColor(1.f,.06f,.01f) : FLinearColor(.1f,.65f,1.f);
    Cone->SetLightColor(Color); Label->SetTextRenderColor(Color.ToFColor(true));
    Label->SetText(!bEnabled ? LOCTEXT("Offline", "SCANNER OFFLINE")
        : bPending ? LOCTEXT("Alert", "DETECTED") : LOCTEXT("Scan", "SCANNING"));
}
void ASovAurelionSweepScanner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bUpdating || bEnding) { return; }
    TGuardValue<bool> Updating(bUpdating, true); UpdatePresentation();
    FString Error;
    if (!HasAuthority() || GetNetMode() != NM_Standalone || !bEnabled || !ValidateConfiguration(Error)) { ClearObservation(); return; }
    // An invalidated request cannot be replaced in the same callback stack.
    if (bPending && !OwnsObservation()) { ClearObservation(); return; }
    if (!bPending) { CaptureObservation(); }
    if (bPending) { DeliverObservation(); }
    UpdatePresentation();
}
void ASovAurelionSweepScanner::OnConstruction(const FTransform& Transform) { Super::OnConstruction(Transform); UpdatePresentation(); }
void ASovAurelionSweepScanner::EndPlay(const EEndPlayReason::Type Reason) { bEnding = true; ClearObservation(); Super::EndPlay(Reason); }

#undef LOCTEXT_NAMESPACE
