// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovCarryTargetComponent.h"
#include "World/SovCarryPolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exertion/SovGameplayAbility_Exertion.h"
#include "Framework/SovPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "NarrativeSavableActor.h"
#include "Recovery/SovRecoveryExclusionVolume.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

USovCarryTargetComponent::USovCarryTargetComponent()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.bStartWithTickEnabled=true;
    InteractionDistance=220.f; InteractionTime=.3f;
    InteractableActionText=NSLOCTEXT("SovCarry","Carry","Carry / drag");
}
void USovCarryTargetComponent::BeginPlay() { Super::BeginPlay(); SetComponentTickEnabled(true); }
bool USovCarryTargetComponent::CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction, FText& Error)
{ return Super::CanInteract_Implementation(Player,Interaction,Error) && (bCarried ? Carrier.Get()==Player && !bMutating : CanCarry(Player,Error)); }
FText USovCarryTargetComponent::GetInteractableActionText_Implementation(APawn* Player, UNarrativeInteractionComponent* Interaction) const
{ return bCarried ? NSLOCTEXT("SovCarry","Release","Set down") : Super::GetInteractableActionText_Implementation(Player,Interaction); }
bool USovCarryTargetComponent::Interact(APawn* Player, UNarrativeInteractionComponent* Interaction)
{ FText Error; return CanInteract(Player,Interaction,Error) && (bCarried ? RequestRelease(Player,Error) : RequestCarry(Player,Error)); }
float USovCarryTargetComponent::GetPlacementHalfHeight() const
{ const auto* Capsule=GetOwner() ? Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()) : nullptr; return Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f; }
bool USovCarryTargetComponent::CanCarry(const APawn* Pawn, FText& Error) const
{
    const auto Reject=[&Error](const TCHAR* Reason) { Error=FText::FromString(Reason); return false; };
    const auto* Player=Cast<ASovPlayerCharacterBase>(Pawn);
    const auto* PC=Player ? Cast<ASovPlayerController>(Player->GetController()) : nullptr;
    AActor* Target=GetOwner(); const auto* Root=Target ? Cast<UCapsuleComponent>(Target->GetRootComponent()) : nullptr;
    const auto* ASC=Player ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ASovPlayerCharacterBase*>(Player)) : nullptr;
    const auto& N=FNarrativeGameplayTags::Get(); const auto& S=FSovGameplayTags::Get();
    if (!IsValid(Target) || Target->IsActorBeingDestroyed() || !Target->HasAuthority() || Target->GetNetMode()!=NM_Standalone
        || bCarried || bMutating || bReleasing || bRescued || bPlacementUnresolved || CarryTargetId.IsNone()
        || !Target->Implements<UNarrativeSavableActor>() || !Root || Root->IsSimulatingPhysics() || Root->GetAttachParent()
        || Root->GetUpVector().Z<.99f || Root->Mobility!=EComponentMobility::Movable
        || !Player || Player==Target || !PC || !ASC || GetWorld()!=Player->GetWorld() || !Player->IsCharacterReady() || !Player->IsAlive()
        || ASC->GetAvatarActor()!=Player || !Player->GetCharacterMovement()->IsMovingOnGround()
        || PC->GetCampaignTransitionState()!=ESovCampaignTransitionState::Idle || ASC->HasMatchingGameplayTag(N.State_Busy)
        || ASC->HasMatchingGameplayTag(N.State_Movement_Lock) || ASC->HasMatchingGameplayTag(N.State_SequencerControlled)
        || ASC->HasMatchingGameplayTag(N.State_DialogueControlled) || ASC->HasMatchingGameplayTag(S.State_Guarding)
        || ASC->HasMatchingGameplayTag(S.State_Deflecting)) { return Reject(TEXT("Carry is unavailable during the current action.")); }
    if (!SovCarryPolicy::ValidBody(Root->GetScaledCapsuleRadius(),Root->GetScaledCapsuleHalfHeight()) || CarryOffset.ContainsNaN()
        || CarryOffset.Size()>350.f || !FMath::IsFinite(MaximumCarrySeconds) || MaximumCarrySeconds<1.f || MaximumCarrySeconds>300.f)
    { return Reject(TEXT("Carry body or attachment tuning is invalid.")); }
    const auto* State=PC->GetCampaignState(); const auto* Mission=State ? State->GetActiveMission() : nullptr;
    if (!State || !State->IsStateValid() || State->IsMutationInProgress()
        || (RequiredProtagonist.IsValid() && RequiredProtagonist!=Player->GetProtagonistIdentityTag())
        || (!RequiredMission.IsNone() && (!Mission || Mission->MissionId!=RequiredMission)))
    { return Reject(TEXT("The required protagonist capability or mission is unavailable.")); }
    const auto* OtherASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (const auto* TargetPawn=Cast<APawn>(Target))
    {
        const auto* Team=Cast<INarrativeTeamAgentInterface>(Player);
        if (!OtherASC || OtherASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())<=0.f
            || (Team && Team->GetTeamAttitudeTowards(*TargetPawn)==ETeamAttitude::Hostile))
        { return Reject(TEXT("Only a living non-hostile rescue target can be carried.")); }
    }
    if (OtherASC && (OtherASC->HasMatchingGameplayTag(N.State_Busy) || OtherASC->HasMatchingGameplayTag(N.State_Movement_Ragdoll)))
    { return Reject(TEXT("The target must finish its current action.")); }
    TInlineComponentArray<UPrimitiveComponent*> Primitives(Target);
    for (const auto* Primitive : Primitives) { if (Primitive->IsSimulatingPhysics()) { return Reject(TEXT("Physics-driven bodies must be stabilized before carrying.")); } }
    if (FVector::DistSquared(Player->GetActorLocation(),Target->GetActorLocation())>FMath::Square(InteractionDistance))
    { return Reject(TEXT("Move closer to lift the target.")); }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCarryAlignment),false,Player); Query.AddIgnoredActor(Target); FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit,Player->GetPawnViewLocation(),Target->GetActorLocation(),ECC_Visibility,Query))
    { return Reject(TEXT("The target is obstructed.")); }
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TInlineComponentArray<USovCarryTargetComponent*> CarryComponents(*It);
        for (const auto* Carry : CarryComponents)
        {
            if (Carry && Carry!=this && (Carry->CarryTargetId==CarryTargetId || Carry->GetCarrier()==Player))
            { return Reject(TEXT("Carry identity or player ownership is ambiguous.")); }
        }
    }
    const FVector AttachPoint=Player->GetActorTransform().TransformPosition(CarryOffset);
    return !GetWorld()->SweepSingleByChannel(Hit,Target->GetActorLocation(),AttachPoint,FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Root->GetScaledCapsuleRadius(),Root->GetScaledCapsuleHalfHeight()),Query)
        || Reject(TEXT("There is not enough clearance to lift the target."));
}
bool USovCarryTargetComponent::SweepBody(FVector Start,FVector End) const
{
    if (!GetOwner() || Start.ContainsNaN() || End.ContainsNaN()) { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCarryBody),false,GetOwner()); if (Carrier.IsValid()) { Query.AddIgnoredActor(Carrier.Get()); }
    FHitResult Hit;
    return !GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(BodyRadius,BodyHalfHeight),Query);
}
bool USovCarryTargetComponent::CanPlaceAt(const FVector& Center) const
{
    const auto* Capsule=GetOwner() ? Cast<UCapsuleComponent>(GetOwner()->GetRootComponent()) : nullptr;
    if (!Capsule || Center.ContainsNaN()) { return false; }
    const float Radius=Capsule->GetScaledCapsuleRadius(), HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
    if (!SovCarryPolicy::ValidBody(Radius,HalfHeight) || ASovRecoveryExclusionVolume::ExcludesCapsule(GetWorld(),Center,Radius,HalfHeight)) { return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCarryRelease),false,GetOwner());
    if (GetWorld()->OverlapBlockingTestByChannel(Center,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,HalfHeight),Query)) { return false; }
    FHitResult Floor;
    return GetWorld()->LineTraceSingleByChannel(Floor,Center,Center-FVector(0,0,HalfHeight+8.f),ECC_Visibility,Query)
        && Floor.bBlockingHit && Floor.ImpactNormal.Z>=.7f && Floor.Distance>=HalfHeight-2.f;
}
bool USovCarryTargetComponent::FindSafeRelease(FVector& Center) const
{
    const auto* Player=Carrier.Get();
    if (Player)
    {
        const float Distance=BodyRadius+Player->GetCapsuleComponent()->GetScaledCapsuleRadius()+40.f;
        for (int32 I=0;I<8;++I)
        {
            const float Angle=I*PI/4.f;
            const FVector Candidate=Player->GetActorLocation()+FVector(FMath::Cos(Angle)*Distance,FMath::Sin(Angle)*Distance,0);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCarryReleaseFloor),false,GetOwner()); Query.AddIgnoredActor(Player); FHitResult Floor;
            if (GetWorld()->LineTraceSingleByChannel(Floor,Candidate+FVector(0,0,200),Candidate-FVector(0,0,300),ECC_Visibility,Query))
            {
                const FVector Point=Floor.ImpactPoint+FVector(0,0,BodyHalfHeight+2.f);
                if (SweepBody(GetOwner()->GetActorLocation(),Point) && CanPlaceAt(Point)) { Center=Point; return true; }
            }
        }
    }
    if (CanPlaceAt(Origin.GetLocation())) { Center=Origin.GetLocation(); return true; }
    return false;
}
bool USovCarryTargetComponent::OwnsCarrier() const
{
    return IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
        && Carrier.IsValid() && CarrierASC.IsValid() && Carrier->IsAlive() && CarrierASC->GetAvatarActor()==Carrier.Get()
        && Carrier->GetController() && Carrier->GetController()->GetPawn()==Carrier.Get();
}
bool USovCarryTargetComponent::RequestCarry(APawn* Pawn,FText& Error)
{
    if (!CanCarry(Pawn,Error)) { return false; }
    TGuardValue<bool> Mutation(bMutating,true);
    auto* Player=CastChecked<ASovPlayerCharacterBase>(Pawn); AActor* Target=GetOwner();
    auto* Root=CastChecked<UCapsuleComponent>(Target->GetRootComponent());
    BodyRadius=Root->GetScaledCapsuleRadius(); BodyHalfHeight=Root->GetScaledCapsuleHalfHeight(); Origin=Target->GetActorTransform();
    if (!CanPlaceAt(Origin.GetLocation())) { Error=FText::FromString(TEXT("The target needs a safe grounded pickup position.")); return false; }
    bOriginalCollision=Target->GetActorEnableCollision(); PreviousInteractionPriority=InteractionPriority;
    Carrier=Player; CarrierASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player); TargetASC=UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    const auto& N=FNarrativeGameplayTags::Get(); auto* ASC=CarrierASC.Get();
    const uint64 ExpectedEpoch=++LeaseEpoch; UAbilitySystemComponent* StartingTargetASC=TargetASC.Get();
    const auto HasLease=[this,ExpectedEpoch,Player,Target,ASC,StartingTargetASC]()
    {
        return LeaseEpoch==ExpectedEpoch && OwnsCarrier() && Carrier.Get()==Player && GetOwner()==Target && CarrierASC.Get()==ASC
            && TargetASC.Get()==StartingTargetASC && (!StartingTargetASC ||
                (IsValid(StartingTargetASC) && StartingTargetASC->GetAvatarActor()==Target
                && StartingTargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())>0.f));
    };
    const auto Abort=[this,ExpectedEpoch]() { if (LeaseEpoch==ExpectedEpoch) { CancelCarry(); } return false; };
    FGameplayEffectSpecHandle Spec=ASC->MakeOutgoingSpec(USovGameplayEffect_ExertionWindow::StaticClass(),1.f,ASC->MakeEffectContext());
    if (!Spec.IsValid()) { Carrier.Reset(); CarrierASC.Reset(); TargetASC.Reset(); return false; }
    Spec.Data->DynamicGrantedTags.AddTag(N.State_Busy); Spec.Data->DynamicGrantedTags.AddTag(N.State_Movement_SlowWalking);
    Spec.Data->SetDuration(MaximumCarrySeconds+1.f,true);
    const FActiveGameplayEffectHandle AppliedCarrierWindow=ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    if (!AppliedCarrierWindow.IsValid() || !HasLease())
    { if (IsValid(ASC) && AppliedCarrierWindow.IsValid()) { ASC->RemoveActiveGameplayEffect(AppliedCarrierWindow); } return Abort(); }
    CarrierWindow=AppliedCarrierWindow;
    bCarried=true; StartedAt=GetWorld()->GetTimeSeconds(); InteractionPriority=-30;
    if (auto* TargetCharacter=Cast<ACharacter>(Target))
    {
        auto* Movement=TargetCharacter->GetCharacterMovement(); PreviousTargetMode=Movement->MovementMode; PreviousTargetCustomMode=Movement->CustomMovementMode;
        bOwnTargetMovement=true; Movement->StopMovementImmediately(); if (!HasLease()) { return Abort(); } Movement->DisableMovement();
        if (!HasLease()) { return Abort(); }
        if (auto* AI=Cast<AAIController>(TargetCharacter->GetController()))
        {
            AI->StopMovement(); if (!HasLease()) { return Abort(); } PausedActivity=AI->FindComponentByClass<UNPCActivityComponent>();
            if (PausedActivity.IsValid() && PausedActivity->IsActive()) { bOwnActivityPause=true; PausedActivity->Deactivate(); }
            if (!HasLease()) { return Abort(); }
        }
    }
    if (TargetASC.IsValid())
    {
        FGameplayEffectSpecHandle TargetSpec=TargetASC->MakeOutgoingSpec(USovGameplayEffect_ExertionWindow::StaticClass(),1.f,TargetASC->MakeEffectContext());
        if (!TargetSpec.IsValid()) { return Abort(); }
        TargetSpec.Data->DynamicGrantedTags.AddTag(N.State_Busy); TargetSpec.Data->DynamicGrantedTags.AddTag(N.State_Movement_Lock);
        TargetSpec.Data->SetDuration(MaximumCarrySeconds+1.f,true);
        UAbilitySystemComponent* TargetAbilitySystem=TargetASC.Get();
        const FActiveGameplayEffectHandle AppliedTargetWindow=TargetAbilitySystem->ApplyGameplayEffectSpecToSelf(*TargetSpec.Data.Get());
        if (!AppliedTargetWindow.IsValid() || !HasLease() || TargetASC.Get()!=TargetAbilitySystem)
        { if (IsValid(TargetAbilitySystem) && AppliedTargetWindow.IsValid()) { TargetAbilitySystem->RemoveActiveGameplayEffect(AppliedTargetWindow); } return Abort(); }
        TargetWindow=AppliedTargetWindow;
    }
    Target->SetActorEnableCollision(false);
    if (!HasLease()) { return Abort(); }
    if (!Target->AttachToComponent(Player->GetRootComponent(),FAttachmentTransformRules::KeepWorldTransform) || !HasLease()) { return Abort(); }
    Target->SetActorRelativeLocation(CarryOffset); if (!HasLease()) { return Abort(); } PreviousBodyCenter=Target->GetActorLocation();
    AddTickPrerequisiteComponent(Player->GetCharacterMovement());
    OnCarryChanged.Broadcast(true,false); return HasLease() ? true : Abort();
}
bool USovCarryTargetComponent::ReleaseAt(const FVector& Center,bool bNotify)
{
    AActor* Target=GetOwner(); if (bReleasing || !IsValid(Target) || Target->IsActorBeingDestroyed() || !CanPlaceAt(Center)) { return false; }
    TGuardValue<bool> Releasing(bReleasing,true);
    ++LeaseEpoch;
    const auto SavedCarrier=Carrier; const auto SavedCarrierASC=CarrierASC; const auto SavedTargetASC=TargetASC;
    const bool HadTargetASC=SavedTargetASC.IsValid();
    const auto SavedActivity=PausedActivity; const auto SavedCarrierWindow=CarrierWindow; const auto SavedTargetWindow=TargetWindow;
    const bool RestoreMovement=bOwnTargetMovement, RestoreActivity=bOwnActivityPause, RestoreCollision=bOriginalCollision;
    const uint8 SavedMode=PreviousTargetMode, SavedCustomMode=PreviousTargetCustomMode;
    const bool Alive=!SavedTargetASC.IsValid() || SavedTargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())>0.f;
    const int32 OwnedBlock=SavedTargetASC.IsValid() && SavedTargetASC->GetActiveGameplayEffect(SavedTargetWindow) ? 1 : 0;
    const bool MayResume=Alive && (!SavedTargetASC.IsValid() ||
        (SavedTargetASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy)<=OwnedBlock
        && SavedTargetASC->GetTagCount(FNarrativeGameplayTags::Get().State_Movement_Lock)<=OwnedBlock));
    // Retire the lease before any attachment, movement, GAS or activity callback can reenter.
    CarrierWindow.Invalidate(); TargetWindow.Invalidate(); Carrier.Reset(); CarrierASC.Reset(); TargetASC.Reset(); PausedActivity.Reset();
    bCarried=false; bPlacementUnresolved=false; bOwnTargetMovement=false; bOwnActivityPause=false; InteractionPriority=PreviousInteractionPriority;
    if (SavedCarrier.IsValid()) { RemoveTickPrerequisiteComponent(SavedCarrier->GetCharacterMovement()); }
    Target->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    if (IsValid(Target) && !Target->IsActorBeingDestroyed()) { Target->SetActorLocation(Center,false,nullptr,ETeleportType::TeleportPhysics); }
    if (IsValid(Target) && !Target->IsActorBeingDestroyed()) { Target->SetActorEnableCollision(RestoreCollision); }
    if (SavedCarrierASC.IsValid() && SavedCarrierWindow.IsValid()) { SavedCarrierASC->RemoveActiveGameplayEffect(SavedCarrierWindow); }
    if (SavedTargetASC.IsValid() && SavedTargetWindow.IsValid()) { SavedTargetASC->RemoveActiveGameplayEffect(SavedTargetWindow); }
    const auto MayResumeNow=[MayResume,HadTargetASC,Target,&SavedTargetASC]()
    {
        return MayResume && (!HadTargetASC ||
            (SavedTargetASC.IsValid() && SavedTargetASC->GetAvatarActor()==Target
            && SavedTargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())>0.f
            && !SavedTargetASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy)
            && !SavedTargetASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_Lock)));
    };
    if (IsValid(Target) && !Target->IsActorBeingDestroyed())
    {
        if (auto* Character=Cast<ACharacter>(Target); Character && RestoreMovement && MayResumeNow() && Character->GetCharacterMovement()->MovementMode==MOVE_None)
        { Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(SavedMode),SavedCustomMode); }
        if (IsValid(Target) && !Target->IsActorBeingDestroyed() && RestoreActivity && MayResumeNow() && SavedActivity.IsValid() && !SavedActivity->IsActive())
        { if (const auto* AI=Cast<AAIController>(SavedActivity->GetOwner()); AI && AI->GetPawn()==Target) { SavedActivity->Activate(); } }
    }
    const bool Placed=IsValid(Target) && !Target->IsActorBeingDestroyed() && Target->GetActorLocation().Equals(Center,1.f);
    if (bNotify && IsValid(this)) { OnCarryChanged.Broadcast(false,bRescued); }
    return Placed;
}

bool USovCarryTargetComponent::RequestRelease(APawn* Player,FText& Error)
{
    if (!bCarried || bMutating || Player!=Carrier.Get() || !OwnsCarrier()) { return false; }
    TGuardValue<bool> Mutation(bMutating,true); FVector Center;
    if (!FindSafeRelease(Center) || !ReleaseAt(Center,true)) { Error=FText::FromString(TEXT("Move to a clear area to set the target down.")); return false; }
    return true;
}
void USovCarryTargetComponent::CancelCarry()
{
    if (bReleasing || (!bCarried && !bPlacementUnresolved && !CarrierWindow.IsValid() && !TargetWindow.IsValid() && !Carrier.IsValid())) { return; }
    FVector Center;
    if (FindSafeRelease(Center) && ReleaseAt(Center,true)) { return; }
    TGuardValue<bool> Releasing(bReleasing,true); ++LeaseEpoch;
    const auto SavedCarrier=Carrier; const auto SavedCarrierASC=CarrierASC; const auto SavedTargetASC=TargetASC;
    const auto SavedCarrierWindow=CarrierWindow; const auto SavedTargetWindow=TargetWindow;
    // Keep only pending placement data. No attachment or gameplay-effect lease remains live.
    CarrierWindow.Invalidate(); TargetWindow.Invalidate(); CarrierASC.Reset();
    bCarried=false; bPlacementUnresolved=true; InteractionPriority=PreviousInteractionPriority;
    if (SavedCarrier.IsValid()) { RemoveTickPrerequisiteComponent(SavedCarrier->GetCharacterMovement()); }
    if (IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed())
    { GetOwner()->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); if (IsValid(GetOwner())) { GetOwner()->SetActorEnableCollision(false); } }
    if (SavedCarrierASC.IsValid() && SavedCarrierWindow.IsValid()) { SavedCarrierASC->RemoveActiveGameplayEffect(SavedCarrierWindow); }
    if (SavedTargetASC.IsValid() && SavedTargetWindow.IsValid()) { SavedTargetASC->RemoveActiveGameplayEffect(SavedTargetWindow); }
    if (IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed())
    {
        OnCarryChanged.Broadcast(false,false);
        OnRecoveryRequired.Broadcast(NSLOCTEXT("SovCarry","NoSafeRelease","The target cannot be safely placed. Clear its original position or load the checkpoint."));
    }
}

void USovCarryTargetComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    if ((!bCarried && !bPlacementUnresolved) || bMutating || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
    TGuardValue<bool> Mutation(bMutating,true);
    if (bPlacementUnresolved) { FVector Center; if (FindSafeRelease(Center)) { ReleaseAt(Center,true); } return; }
    if (!OwnsCarrier() || !CarrierASC->GetActiveGameplayEffect(CarrierWindow)
        || GetWorld()->GetTimeSeconds()-StartedAt>=MaximumCarrySeconds
        || (TargetASC.IsValid() && TargetASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute())<=0.f)
        || (TargetASC.IsValid() && !TargetASC->GetActiveGameplayEffect(TargetWindow))
        || !SweepBody(PreviousBodyCenter,GetOwner()->GetActorLocation())) { CancelCarry(); return; }
    PreviousBodyCenter=GetOwner()->GetActorLocation();
}
void USovCarryTargetComponent::Serialize(FArchive& Ar)
{ Super::Serialize(Ar); if (Ar.ArIsSaveGame && Ar.IsSaving() && (bCarried || bMutating || bReleasing || bPlacementUnresolved)) { Ar.SetError(); } }
void USovCarryTargetComponent::Load_Implementation()
{ if (bCarried || bPlacementUnresolved) { CancelCarry(); } OnCarryChanged.Broadcast(false,bRescued); }
void USovCarryTargetComponent::EndPlay(EEndPlayReason::Type Reason)
{ if (bCarried || bPlacementUnresolved || CarrierWindow.IsValid()) { CancelCarry(); } Super::EndPlay(Reason); }
