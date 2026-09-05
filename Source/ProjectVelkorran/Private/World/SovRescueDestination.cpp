// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovRescueDestination.h"
#include "World/SovCarryTargetComponent.h"
#include "World/SovCarryPolicy.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "Save/SovSaveSubsystem.h"
#include "UObject/StrongObjectPtr.h"

bool USovRescueDestinationInteractable::CanInteract_Implementation(APawn* Player,UNarrativeInteractionComponent* Interaction,FText& Error)
{
    const auto* Destination=Cast<ASovRescueDestination>(GetOwner()); USovCarryTargetComponent* Target=nullptr; FVector Center;
    return Super::CanInteract_Implementation(Player,Interaction,Error) && Destination && Destination->ValidateRescue(Player,Target,Center,Error);
}
bool USovRescueDestinationInteractable::Interact(APawn* Player,UNarrativeInteractionComponent* Interaction)
{ auto* Destination=Cast<ASovRescueDestination>(GetOwner()); FText Error; return Destination && CanInteract(Player,Interaction,Error) && Destination->RequestRescue(Player,Error); }
ASovRescueDestination::ASovRescueDestination()
{
    SceneRoot=CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")); SetRootComponent(SceneRoot);
    EntryBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("EntryBounds")); EntryBounds->SetupAttachment(SceneRoot);
    EntryBounds->SetBoxExtent(FVector(250,250,150)); EntryBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ReleaseMark=CreateDefaultSubobject<USceneComponent>(TEXT("ReleaseMark")); ReleaseMark->SetupAttachment(SceneRoot);
    ReleaseMark->SetRelativeLocation(FVector(140,0,0));
    Interactable=CreateDefaultSubobject<USovRescueDestinationInteractable>(TEXT("Interactable"));
    Interactable->InteractionDistance=350.f; Interactable->InteractionTime=.5f;
    Interactable->InteractionPriority=20;
    Interactable->InteractableActionText=NSLOCTEXT("SovCarry","Rescue","Move to safety");
}
bool ASovRescueDestination::ValidateRescue(const APawn* Pawn,USovCarryTargetComponent*& Target,FVector& Center,FText& Error) const
{
    Target=nullptr;
    const auto Reject=[&Error](const TCHAR* Reason) { Error=FText::FromString(Reason); return false; };
    const auto* Player=Cast<ASovPlayerCharacterBase>(Pawn); const auto* PC=Player ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    const auto* State=PC ? PC->GetCampaignState() : nullptr; const auto* Mission=State ? State->GetActiveMission() : nullptr;
    const auto* Beat=Mission ? Mission->FindBeat(CompletionBeat) : nullptr;
    if (!HasAuthority() || GetNetMode()!=NM_Standalone || bMutating || !Player || !PC || !Player->IsCharacterReady() || !Player->IsAlive()
        || Player->GetWorld()!=GetWorld() || PC->GetPawn()!=Player || PC->GetCampaignTransitionState()!=ESovCampaignTransitionState::Idle
        || !State || !State->IsStateValid() || State->IsMutationInProgress() || !Mission || Mission->MissionId!=MissionId
        || State->GetActiveProtagonist()!=Player->GetProtagonistIdentityTag() || DestinationId.IsNone() || RequiredCarryTargetId.IsNone()
        || !Beat || Beat->bInteractiveChoice || Beat->bRequiresCoActionProof || Beat->HandoffToProtagonist.IsValid() || !Beat->CinematicId.IsNone()
        || State->IsBeatComplete(MissionId,CompletionBeat)) { return Reject(TEXT("This rescue destination is not available for the current beat.")); }
    if ((Beat->RequiredProtagonist.IsValid() && Beat->RequiredProtagonist!=State->GetActiveProtagonist())
        || !State->HasKnowledge(State->GetActiveProtagonist(),Beat->RequiredKnowledge)) { return Reject(TEXT("The protagonist lacks rescue authorization.")); }
    for (FName Prior : Beat->PrerequisiteBeats) { if (!State->IsBeatComplete(MissionId,Prior)) { return Reject(TEXT("A required rescue prerequisite is missing.")); } }
    for (const auto& Fact : Beat->RequiredState) { if (State->GetStateValue(Fact.Key)!=Fact.Value) { return Reject(TEXT("Mission state does not permit this rescue.")); } }
    for (TActorIterator<ASovRescueDestination> It(GetWorld()); It; ++It)
    { if (*It!=this && It->DestinationId==DestinationId) { return Reject(TEXT("Rescue destination identity is ambiguous.")); } }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TInlineComponentArray<USovCarryTargetComponent*> CarryComponents(*It);
        for (auto* Carry : CarryComponents)
        {
            if (Carry && Carry->GetCarrier()==Player && Carry->IsCarried())
            { if (Target) { return Reject(TEXT("More than one carried target claims this player.")); } Target=Carry; }
        }
    }
    const FVector Local=EntryBounds->GetComponentTransform().InverseTransformPosition(Player->GetActorLocation());
    const FVector Extent=EntryBounds->GetUnscaledBoxExtent();
    const bool AtDestination=!Local.ContainsNaN() && FMath::Abs(Local.X)<=Extent.X && FMath::Abs(Local.Y)<=Extent.Y && FMath::Abs(Local.Z)<=Extent.Z;
    Center=ReleaseMark->GetComponentLocation()+FVector(0,0,Target ? Target->GetPlacementHalfHeight()+2.f : 0.f);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovRescueDestination),false,Player); Query.AddIgnoredActor(this);
    if (Target) { Query.AddIgnoredActor(Target->GetOwner()); } FHitResult Hit;
    const bool HasLineOfSight=!GetWorld()->LineTraceSingleByChannel(Hit,Player->GetPawnViewLocation(),Center,ECC_Visibility,Query);
    if (!SovCarryPolicy::CanRescue(Target && Target->OwnsCarrier(),Target && Target->CarryTargetId==RequiredCarryTargetId,
        AtDestination && HasLineOfSight,Target && !Target->bMutating && Target->CanPlaceAt(Center)
            && Target->SweepBody(Target->GetOwner()->GetActorLocation(),Center),true))
    { return Reject(TEXT("Bring the correct carried target into the clear rescue area.")); }
    return true;
}
bool ASovRescueDestination::RequestRescue(APawn* Player,FText& Error)
{
    USovCarryTargetComponent* Target=nullptr; FVector Center;
    if (!ValidateRescue(Player,Target,Center,Error)) { return false; }
    TStrongObjectPtr<USovCarryTargetComponent> KeepTarget(Target);
    TGuardValue<bool> Mutation(bMutating,true); TGuardValue<bool> TargetMutation(Target->bMutating,true);
    auto* PC=CastChecked<ASovPlayerController>(Player->GetController());
    if (!Target->ReleaseAt(Center,false)) { Error=FText::FromString(TEXT("The safe release point became blocked.")); return false; }
    if (!IsValid(Target) || !IsValid(Target->GetOwner()) || !Target->GetOwner()->GetActorLocation().Equals(Center,1.f)
        || !Target->CanPlaceAt(Center) || !IsValid(Player) || !IsValid(PC) || PC->GetPawn()!=Player
        || !CastChecked<ASovPlayerCharacterBase>(Player)->IsAlive() || !PC->GetCampaignState()->GetActiveMission()
        || PC->GetCampaignState()->GetActiveMission()->MissionId!=MissionId) { return false; }
    const ESovCampaignResult Result=PC->GetCampaignState()->CompleteBeat(CompletionBeat);
    if (Result!=ESovCampaignResult::Applied)
    { Error=FText::FromString(TEXT("The target is safely released, but the rescue beat could not commit.")); Target->OnCarryChanged.Broadcast(false,false); return false; }
    if (IsValid(Target) && IsValid(Target->GetOwner())) { Target->bRescued=true; Target->OnCarryChanged.Broadcast(false,true); }
    if (auto* GI=GetGameInstance()) { if (auto* Save=GI->GetSubsystem<USovSaveSubsystem>()) { Save->QueueAutosave(ESovSaveBoundary::CanonGate,DestinationId); } }
    return true;
}
