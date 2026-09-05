// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Cinematics/SovCinematicPolicy.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Framework/SovPlayerController.h"
#include "Save/SovSaveSubsystem.h"
#include "Recovery/SovRecoveryExclusionVolume.h"
#include "World/SovWorldTransitActor.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WorldPartitionStreamingSourceComponent.h"
#include "Containers/Ticker.h"
#include "Items/WeaponItem.h"
#include "NarrativeGameplayTags.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneSubSection.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "WorldPartition/WorldPartitionStreamingSource.h"

USovCampaignCinematicComponent::USovCampaignCinematicComponent()
{ PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.bTickEvenWhenPaused = true; }

void USovCampaignCinematicComponent::OnRegister()
{
    Super::OnRegister();
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        if (auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner()))
        { Actor->PlaybackSettings.bAutoPlay = false; Actor->NarrativeSequenceParams.bAutoPlay = false; }
    }
}

void USovCampaignCinematicComponent::OnUnregister()
{
    Abort(TEXT("Cinematic component was unregistered.")); ReleaseOwnership();
    Super::OnUnregister();
}

bool USovCampaignCinematicComponent::ValidateConfiguration(FString& OutError) const
{
    const auto Fail = [&OutError]() { OutError = TEXT("Cinematic requires a Narrative sequence owner, mission/beat, bounded preload manifest and unique participant contracts."); return false; };
    if (!Cast<ANarrativeLevelSequenceActor>(GetOwner()) || MissionId.IsNone() || BeatId.IsNone() || Sequence.IsNull()
        || Participants.IsEmpty() || Participants.Num() > 16 || PreloadAssets.Num() > 128 || RequiredStreamingLevels.Num() > 32
        || RequiredPartitionRegions.Num() > 16 || TransitPostconditions.Num() > 16
        || !FMath::IsFinite(LoadingTimeoutSeconds) || LoadingTimeoutSeconds < 1.f || LoadingTimeoutSeconds > 60.f
        || !FMath::IsFinite(RequestRange) || RequestRange <= 0.f || RequestRange > 2000.f) { return Fail(); }
    TSet<FName> Bindings, ActorIds, Levels; int32 PlayerCount = 0;
    for (const auto& Participant : Participants)
    {
        if (Participant.BindingTag.IsNone() || Bindings.Contains(Participant.BindingTag)
            || Participant.ExitWield > ESovCinematicExitWield::DrawRequiredWeapon) { return Fail(); }
        Bindings.Add(Participant.BindingTag);
        if (Participant.bControlledProtagonist) { ++PlayerCount; if (!Participant.ActorTag.IsNone()) { return Fail(); } }
        else { if (Participant.ActorTag.IsNone() || ActorIds.Contains(Participant.ActorTag)) { return Fail(); } ActorIds.Add(Participant.ActorTag); }
        if (Participant.RequiredWeapon && !Participant.EquipmentSlot.IsValid()) { return Fail(); }
        if (Participant.ExitWield == ESovCinematicExitWield::DrawRequiredWeapon && (!Participant.RequiredWeapon || !Participant.WieldSlot.IsValid())) { return Fail(); }
        if (Participant.bApplyExitTransform && (!Participant.ExitTransform.IsValid()
            || !Participant.ExitTransform.GetScale3D().Equals(FVector::OneVector))) { return Fail(); }
    }
    if (PlayerCount != 1) { return Fail(); }
    for (const auto& Asset : PreloadAssets) { if (!Asset.IsValid()) { return Fail(); } }
    for (FName Level : RequiredStreamingLevels) { if (Level.IsNone() || Levels.Contains(Level)) { return Fail(); } Levels.Add(Level); }
    TSet<FName> RegionIds, TransitIds; TSet<FSoftObjectPath> TransitPaths;
    for (const auto& Region : RequiredPartitionRegions)
    {
        if (Region.RegionId.IsNone() || RegionIds.Contains(Region.RegionId)
            || !SovCinematicPolicy::ValidPartitionRegion(Region.Center.X, Region.Center.Y, Region.Center.Z,
                Region.Radius, Region.RequiredActors.Num())) { return Fail(); }
        RegionIds.Add(Region.RegionId); TSet<FSoftObjectPath> Actors;
        for (const auto& Actor : Region.RequiredActors)
        {
            const auto Path = Actor.ToSoftObjectPath();
            if (!Path.IsValid() || Actors.Contains(Path)) { return Fail(); } Actors.Add(Path);
        }
    }
    for (const auto& Contract : TransitPostconditions)
    {
        const auto Path = Contract.Transit.ToSoftObjectPath();
        if (!Path.IsValid() || Contract.ExpectedTransitId.IsNone() || TransitIds.Contains(Contract.ExpectedTransitId)
            || TransitPaths.Contains(Path) || (!Contract.bSetPower && !Contract.bSetLock) || Contract.LockReason.ToString().Len() > 256) { return Fail(); }
        TransitIds.Add(Contract.ExpectedTransitId); TransitPaths.Add(Path);
    }
    OutError.Reset(); return true;
}

bool USovCampaignCinematicComponent::AcquirePartitionSources(FString& OutError)
{
    ReleasePartitionSources();
    if (RequiredPartitionRegions.IsEmpty()) { return true; }
    if (!GetWorld() || !GetWorld()->IsPartitionedWorld())
    { OutError = TEXT("Cinematic declares partition cells but this world has no World Partition."); return false; }
    const uint64 Epoch = RequestEpoch;
    for (const auto& Region : RequiredPartitionRegions)
    {
        FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // The source has an independent transform, so camera/sequence-owner movement cannot move the preload area.
        AActor* Anchor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
        if (!IsValid(Anchor) || RequestEpoch != Epoch || bEndingPlay)
        {
            if (IsValid(Anchor)) { Anchor->Destroy(); }
            ReleasePartitionSources(); OutError = TEXT("Cinematic streaming source allocation was interrupted."); return false;
        }
        FSovCinematicPartitionLease& Lease = PartitionLeases.AddDefaulted_GetRef(); Lease.Contract = Region; Lease.Anchor = Anchor;
        USceneComponent* Root = NewObject<USceneComponent>(Anchor); Anchor->AddInstanceComponent(Root); Anchor->SetRootComponent(Root); Root->RegisterComponent();
        Anchor->SetActorLocation(Region.Center); Anchor->SetActorHiddenInGame(true); Anchor->SetActorEnableCollision(false);
        auto* Source = NewObject<UWorldPartitionStreamingSourceComponent>(Anchor); Lease.Source = Source;
        Source->DisableStreamingSource(); Source->TargetState = EStreamingSourceTargetState::Activated;
        // No grid filter: a typo must never turn an empty cell query into readiness.
        Source->Priority = EStreamingSourcePriority::High;
        FStreamingSourceShape Shape; Shape.bUseGridLoadingRange = false; Shape.bIsSector = false;
        Shape.Radius = Region.Radius; Shape.Location = FVector::ZeroVector; Shape.Rotation = FRotator::ZeroRotator;
        Source->Shapes.Add(Shape); Anchor->AddInstanceComponent(Source); Source->RegisterComponent(); Source->EnableStreamingSource();
        if (RequestEpoch != Epoch || bEndingPlay || !IsValid(Anchor) || !IsValid(Source) || !Source->IsRegistered())
        { ReleasePartitionSources(); OutError = TEXT("Cinematic partition source registration failed."); return false; }
    }
    ConsecutivePartitionReadyTicks = 0; return true;
}

bool USovCampaignCinematicComponent::ArePartitionRegionsReady() const
{
    if (PartitionLeases.IsEmpty()) { return RequiredPartitionRegions.IsEmpty(); }
    if (!GetWorld() || !GetWorld()->IsPartitionedWorld() || PartitionLeases.Num() != RequiredPartitionRegions.Num()) { return false; }
    for (const auto& Lease : PartitionLeases)
    {
        if (!IsValid(Lease.Anchor) || Lease.Anchor->IsActorBeingDestroyed() || !IsValid(Lease.Source)
            || !Lease.Source->IsRegistered() || !Lease.Source->IsStreamingSourceEnabled()
            || !Lease.Anchor->GetActorLocation().Equals(Lease.Contract.Center, .1f) || !Lease.Source->IsStreamingCompleted()) { return false; }
        for (const auto& Reference : Lease.Contract.RequiredActors)
        {
            const AActor* Actor = Reference.Get();
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld()
                || !Actor->GetLevel() || Actor->GetLevel() == GetWorld()->PersistentLevel || !Actor->GetLevel()->bIsVisible
                || !Actor->IsActorInitialized() || (GetWorld()->HasBegunPlay() && !Actor->HasActorBegunPlay())
                || !SovCinematicPolicy::WitnessInsideRegion(FVector::DistSquared(Actor->GetActorLocation(), Lease.Contract.Center), Lease.Contract.Radius)) { return false; }
        }
    }
    return true;
}

void USovCampaignCinematicComponent::ReleasePartitionSources()
{
    // Detach ownership before callbacks. Unregister only these source components, never a player's source or global cells.
    TArray<FSovCinematicPartitionLease> Retiring = MoveTemp(PartitionLeases); PartitionLeases.Reset();
    ConsecutivePartitionReadyTicks = 0;
    for (auto& Lease : Retiring)
    {
        if (IsValid(Lease.Source)) { Lease.Source->DisableStreamingSource(); Lease.Source->DestroyComponent(); }
        if (IsValid(Lease.Anchor) && !Lease.Anchor->IsActorBeingDestroyed()) { Lease.Anchor->Destroy(); }
    }
}

bool USovCampaignCinematicComponent::ResolveTransitPostconditions(FString& OutError)
{
    TransitSnapshots.Reset();
    for (const auto& Contract : TransitPostconditions)
    {
        auto* Transit = Contract.Transit.Get();
        if (!IsValid(Transit) || Transit->IsActorBeingDestroyed())
        { OutError = TEXT("A required cinematic door or lift is missing."); return false; }
        FSovCinematicTransitSnapshot Entry; Entry.Contract = Contract; Entry.Transit = Transit;
        Entry.bPowered = Transit->bPowered; Entry.LockReason = Transit->LockReason; Entry.Endpoint = static_cast<uint8>(Transit->GetTransitState());
        Entry.OriginalPowerRevision = Transit->GetPowerRevision(); Entry.OriginalLockRevision = Transit->GetLockRevision();
        TransitSnapshots.Add(Entry);
    }
    return ValidateTransitPostconditions(false, OutError);
}

bool USovCampaignCinematicComponent::ValidateTransitPostconditions(bool bApplied, FString& OutError) const
{
    if (TransitSnapshots.Num() != TransitPostconditions.Num()) { OutError = TEXT("Cinematic transit snapshot is incomplete."); return false; }
    for (const auto& Entry : TransitSnapshots)
    {
        auto* Transit = Entry.Transit.Get(); const auto& Contract = Entry.Contract;
        if (!IsValid(Transit) || Transit->IsActorBeingDestroyed() || Transit->GetWorld() != GetWorld() || !Transit->HasAuthority()
            || Transit != Contract.Transit.Get() || Transit->TransitId != Contract.ExpectedTransitId
            || static_cast<uint8>(Transit->GetTransitState()) != Entry.Endpoint
            || (Transit->GetTransitState() != ESovWorldTransitState::AtOrigin && Transit->GetTransitState() != ESovWorldTransitState::AtDestination)
            || !FMath::IsFinite(Transit->StructuralHealth) || Transit->StructuralHealth <= 0.f)
        { OutError = TEXT("Cinematic transit target must be the same living mechanism at its stable endpoint."); return false; }
        for (TActorIterator<ASovWorldTransitActor> It(GetWorld()); It; ++It)
        { if (*It != Transit && !It->IsActorBeingDestroyed() && It->TransitId == Contract.ExpectedTransitId) { OutError = TEXT("Cinematic transit identity is ambiguous."); return false; } }
        const bool ExpectedPower = bApplied && Contract.bSetPower ? Contract.bPowered : Entry.bPowered;
        const FText& ExpectedLock = bApplied && Contract.bSetLock ? Contract.LockReason : Entry.LockReason;
        if (Transit->bPowered != ExpectedPower || !Transit->LockReason.EqualTo(ExpectedLock)
            || Transit->GetPowerRevision() != (bApplied && Contract.bSetPower ? Entry.AppliedPowerRevision : Entry.OriginalPowerRevision)
            || Transit->GetLockRevision() != (bApplied && Contract.bSetLock ? Entry.AppliedLockRevision : Entry.OriginalLockRevision))
        { OutError = TEXT("Cinematic transit state changed outside its native postcondition."); return false; }
    }
    return true;
}

bool USovCampaignCinematicComponent::ApplyTransitPostconditions(FString& OutError)
{
    if (!ValidateTransitPostconditions(false, OutError)) { return false; }
    const uint64 Epoch = RequestEpoch;
    for (auto& Entry : TransitSnapshots)
    {
        auto* Transit = Entry.Transit.Get();
        if (!IsValid(Transit) || Transit->IsActorBeingDestroyed() || Transit->bPowered != Entry.bPowered
            || !Transit->LockReason.EqualTo(Entry.LockReason) || static_cast<uint8>(Transit->GetTransitState()) != Entry.Endpoint)
        { OutError = TEXT("Another callback changed an unapplied cinematic transit postcondition."); return false; }
        if (Transit->GetPowerRevision() != Entry.OriginalPowerRevision || Transit->GetLockRevision() != Entry.OriginalLockRevision)
        { OutError = TEXT("Another native setter wrote the transit postcondition."); return false; }
        if (Entry.Contract.bSetPower)
        { Entry.bPowerApplied = true; Entry.AppliedPowerRevision = Transit->GetPowerRevision() + 1; Transit->SetPower(Entry.Contract.bPowered); }
        if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || !IsContextCurrent() || !IsValid(Transit) || Transit->IsActorBeingDestroyed()) { return false; }
        if (!Transit->LockReason.EqualTo(Entry.LockReason) || Transit->GetLockRevision() != Entry.OriginalLockRevision
            || Transit->GetPowerRevision() != (Entry.Contract.bSetPower ? Entry.AppliedPowerRevision : Entry.OriginalPowerRevision)
            || Transit->TransitId != Entry.Contract.ExpectedTransitId || Transit->GetWorld() != GetWorld()
            || static_cast<uint8>(Transit->GetTransitState()) != Entry.Endpoint || !FMath::IsFinite(Transit->StructuralHealth) || Transit->StructuralHealth <= 0.f)
        { OutError = TEXT("Transit power callback changed the mechanism or its native setter ownership."); return false; }
        if (Entry.Contract.bSetLock)
        { Entry.bLockApplied = true; Entry.AppliedLockRevision = Transit->GetLockRevision() + 1; Transit->SetLockReason(Entry.Contract.LockReason); }
        if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || !IsContextCurrent()) { return false; }
    }
    return ValidateTransitPostconditions(true, OutError);
}

void USovCampaignCinematicComponent::RestoreTransitPostconditions()
{
    const uint64 Epoch = RequestEpoch;
    for (auto& Entry : TransitSnapshots)
    {
        if (RequestEpoch != Epoch || !OwnsPlaybackGeneration()) { return; }
        auto* Transit = Entry.Transit.Get();
        const auto TargetStillCurrent = [&]()
        {
            return RequestEpoch == Epoch && OwnsPlaybackGeneration() && IsValid(Transit) && !Transit->IsActorBeingDestroyed()
                && Transit->HasAuthority() && Transit->GetWorld() == GetWorld() && Transit == Entry.Contract.Transit.Get()
                && Transit->TransitId == Entry.Contract.ExpectedTransitId && static_cast<uint8>(Transit->GetTransitState()) == Entry.Endpoint
                && FMath::IsFinite(Transit->StructuralHealth) && Transit->StructuralHealth > 0.f;
        };
        if (!TargetStillCurrent()) { continue; }
        // Native setter revisions protect even a later owner's same-value write.
        if (Entry.bPowerApplied && Transit->GetPowerRevision() == Entry.AppliedPowerRevision && Transit->bPowered == Entry.Contract.bPowered)
        { Entry.bPowerApplied = false; Transit->SetPower(Entry.bPowered); }
        if (!TargetStillCurrent()) { continue; }
        if (Entry.bLockApplied && Transit->GetLockRevision() == Entry.AppliedLockRevision && Transit->LockReason.EqualTo(Entry.Contract.LockReason))
        { Entry.bLockApplied = false; Transit->SetLockReason(Entry.LockReason); }
    }
}

bool USovCampaignCinematicComponent::IsContextCurrent() const
{
    auto* PC = Controller.Get(); auto* Pawn = Cast<ASovPlayerCharacterBase>(PlayerPawn.Get()); auto* ASC = PlayerASC.Get();
    const auto* State = PC ? PC->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
    return !bEndingPlay && IsRegistered() && IsComponentTickEnabled() && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed() && GetOwner()->HasAuthority()
        && PC && PC->GetPawn() == Pawn && Pawn && Pawn->IsCharacterReady() && Pawn->IsAlive()
        && ASC && ASC->GetAvatarActor() == Pawn && UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn) == ASC
        && State && State->IsStateValid() && State->GetActiveMission() && State->GetActiveMission()->MissionId == MissionId
        && State->GetActiveProtagonist() == Pawn->GetProtagonistIdentityTag()
        && PC->GetCampaignTransitionState() == ESovCampaignTransitionState::Idle;
}

bool USovCampaignCinematicComponent::ResolveParticipants(FString& OutError)
{
    Snapshot.Reset(); TSet<ANarrativeCharacter*> Used;
    for (const auto& Contract : Participants)
    {
        ANarrativeCharacter* Character = Contract.bControlledProtagonist ? PlayerPawn.Get() : nullptr;
        if (!Contract.bControlledProtagonist)
        {
            for (TActorIterator<ANarrativeCharacter> It(GetWorld()); It; ++It)
            {
                if (It->IsActorBeingDestroyed() || !It->ActorHasTag(Contract.ActorTag)) { continue; }
                if (Character) { OutError = TEXT("Cinematic participant actor identity is ambiguous."); return false; }
                Character = *It;
            }
        }
        if (!Character || Used.Contains(Character)) { OutError = TEXT("A required cinematic participant is missing or used twice."); return false; }
        Used.Add(Character);
        FSovCinematicParticipantSnapshot Entry;
        Entry.Character = Character; Entry.ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
        Entry.Transform = Character->GetActorTransform(); Entry.Wield = Character->GetWeaponWieldState();
        if (Contract.RequiredWeapon && Character->GetEquipmentComponent())
        { Entry.RequiredWeapon = Character->GetEquipmentComponent()->GetEquippedWeaponAtSlot(Contract.EquipmentSlot); }
        Snapshot.Add(Entry);
    }
    return ValidateParticipants(true, OutError);
}

bool USovCampaignCinematicComponent::ValidateParticipants(bool bCheckExit, FString& OutError) const
{
    if (Snapshot.Num() != Participants.Num()) { OutError = TEXT("Cinematic participant snapshot is incomplete."); return false; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCinematicExit), false);
    for (const auto& Entry : Snapshot)
    {
        if (Entry.Character.IsValid())
        {
            Query.AddIgnoredActor(Entry.Character.Get()); TArray<AActor*> Attached;
            Entry.Character->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
        }
    }
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        const auto& Contract = Participants[Index]; const auto& Entry = Snapshot[Index];
        auto* Character = Entry.Character.Get(); auto* ASC = Entry.ASC.Get();
        if (!Character || Character->IsActorBeingDestroyed() || Character->GetWorld() != GetWorld()
            || !ASC || ASC->GetAvatarActor() != Character || UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character) != ASC
            || (Contract.bRequireLiving && !Character->IsAlive()) || !ASC->HasAllMatchingGameplayTags(Contract.RequiredState)
            || ASC->HasAnyMatchingGameplayTags(Contract.BlockedState))
        { OutError = TEXT("A cinematic participant is missing, dead, replaced or in an incompatible damage state."); return false; }
        if (const auto* Player = Cast<ASovPlayerCharacterBase>(Character); Player && !Player->IsCharacterReady())
        { OutError = TEXT("The cinematic protagonist is not ready."); return false; }
        if (const auto* NPC = Cast<ASovNPCCharacterBase>(Character); NPC && !NPC->IsEncounterSnapshotReady())
        { OutError = TEXT("A cinematic NPC has not finished its native initialization."); return false; }
        if (Contract.RequiredWeapon)
        {
            auto* Item = Entry.RequiredWeapon.Get();
            if (!Item || Item->OwningInventory != Character->GetInventoryComponent() || !Item->IsA(Contract.RequiredWeapon) || !Item->IsEquipped()
                || !Character->GetEquipmentComponent() || Character->GetEquipmentComponent()->GetEquippedWeaponAtSlot(Contract.EquipmentSlot) != Item
                || (Contract.ExitWield == ESovCinematicExitWield::DrawRequiredWeapon
                    && (Item->GetWeaponWieldAttachConfig(Contract.WieldSlot).SocketName.IsNone()
                        || !Item->GetWeaponWieldAttachConfig(Contract.WieldSlot).Offset.IsValid())))
            { OutError = TEXT("The required real equipped weapon or its authored hand attachment is unavailable."); return false; }
        }
        if (bCheckExit && Contract.bApplyExitTransform)
        {
            const auto* Capsule = Character->GetCapsuleComponent();
            if (!Capsule || ASovRecoveryExclusionVolume::ExcludesCapsule(GetWorld(), Contract.ExitTransform.GetLocation(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight())
                || GetWorld()->OverlapBlockingTestByChannel(Contract.ExitTransform.GetLocation(), Contract.ExitTransform.GetRotation(),
                ECC_Pawn, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query))
            { OutError = TEXT("Cinematic exit capsule overlaps blocking geometry."); return false; }
            const FVector Feet = Contract.ExitTransform.GetLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
            auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()); FNavLocation Projected;
            if (!Navigation || !Navigation->ProjectPointToNavigation(Feet, Projected, FVector(80,80,120)) || FVector::DistSquared(Feet, Projected.Location) > FMath::Square(100.f))
            { OutError = TEXT("Cinematic exit lacks a safe navigation surface."); return false; }

        }
    }
    if (bCheckExit)
    {
        for (int32 A = 0; A < Participants.Num(); ++A)
        {
            for (int32 B = A + 1; B < Participants.Num(); ++B)
            {
                if (!Participants[A].bApplyExitTransform && !Participants[B].bApplyExitTransform) { continue; }
                const auto* CA = Snapshot[A].Character->GetCapsuleComponent(); const auto* CB = Snapshot[B].Character->GetCapsuleComponent();
                const FVector PA = Participants[A].bApplyExitTransform ? Participants[A].ExitTransform.GetLocation() : Snapshot[A].Character->GetActorLocation();
                const FVector PB = Participants[B].bApplyExitTransform ? Participants[B].ExitTransform.GetLocation() : Snapshot[B].Character->GetActorLocation();
                if (!CA || !CB || SovCinematicPolicy::CapsulesOverlap(FVector::DistSquared2D(PA, PB), PA.Z - PB.Z,
                    CA->GetScaledCapsuleRadius() + CB->GetScaledCapsuleRadius(), CA->GetScaledCapsuleHalfHeight() + CB->GetScaledCapsuleHalfHeight()))
                { OutError = TEXT("Cinematic final capsule placements overlap one another."); return false; }
            }
        }
    }
    return true;
}

bool USovCampaignCinematicComponent::ValidatePresentationSequence(ULevelSequence* Asset, FString& OutError) const
{
    TArray<UMovieSceneSequence*> Pending; Pending.Add(Asset); TSet<UMovieSceneSequence*> Seen;
    for (int32 Index = 0; Index < Pending.Num(); ++Index)
    {
        auto* Current = Pending[Index]; if (Seen.Contains(Current)) { continue; } Seen.Add(Current);
        if (!Current || !Current->GetMovieScene() || Seen.Num() > 128) { OutError = TEXT("Cinematic sequence hierarchy is missing or exceeds its bounded contract."); return false; }
        auto* Scene = Current->GetMovieScene(); TArray<UMovieSceneTrack*> Tracks;
        for (auto* Track : Scene->GetTracks()) { Tracks.Add(Track); }
        for (const auto& Binding : Scene->GetBindings()) { for (auto* Track : Binding.GetTracks()) { Tracks.Add(Track); } }
        for (auto* Track : Tracks)
        {
            if (!Track || Track->IsA<UMovieSceneEventTrack>())
            { OutError = TEXT("Managed campaign scenes cannot rely on arbitrary Sequencer event tracks. Required world writes belong to native postconditions or the committed beat."); return false; }
            for (auto* Section : Track->GetAllSections())
            { if (auto* Sub = Cast<UMovieSceneSubSection>(Section)) { Pending.Add(Sub->GetSequence()); } }
        }
    }
    auto* Scene = Asset->GetMovieScene(); const auto Range = Scene->GetPlaybackRange();
    if (!Range.HasLowerBound() || !Range.HasUpperBound() || Range.GetUpperBoundValue() <= Range.GetLowerBoundValue())
    { OutError = TEXT("Cinematic requires a finite, nonempty playback range."); return false; }
    OutError.Reset(); return true;
}

bool USovCampaignCinematicComponent::RequestPlay(ASovPlayerController* Player, FString& OutError)
{
    if (!IsRegistered() || !IsComponentTickEnabled()) { OutError = TEXT("Cinematic component must be registered with ticking enabled."); return false; }
    // A checkpoint restore can make this beat incomplete again while the same placed actor remains alive.
    if (bRequestStarting || bEndingPlay || bFinishing || (Phase != ESovCinematicPhase::Idle && Phase != ESovCinematicPhase::Failed && Phase != ESovCinematicPhase::Completed)
        || !IsValid(Player) || !Player->HasAuthority() || Player->GetNetMode() != NM_Standalone || !ValidateConfiguration(OutError)) { return false; }
    TGuardValue<bool> Starting(bRequestStarting, true);
    const uint64 Epoch = ++RequestEpoch;
    Snapshot.Reset(); TransitSnapshots.Reset();
    Controller = Player; PlayerPawn = Cast<ANarrativeCharacter>(Player->GetPawn());
    PlayerASC = PlayerPawn.IsValid() ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerPawn.Get()) : nullptr;
    if (!IsContextCurrent() || FVector::DistSquared(PlayerPawn->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(RequestRange))
    { OutError = TEXT("Cinematic requires the ready current protagonist at its physical entry."); return false; }
    auto* State = Player->FindComponentByClass<USovCampaignStateComponent>(); const auto* Beat = State->GetActiveMission()->FindBeat(BeatId);
    if (!Beat || !Beat->bRequiresCinematicProof || Beat->CinematicId.IsNone() || Beat->bInteractiveChoice || State->IsBeatComplete(MissionId, BeatId))
    { OutError = TEXT("Cinematic must own an incomplete native-proof presentation beat; interactive choices stay in their own validated dialogue beat."); return false; }
    for (FName Prior : Beat->PrerequisiteBeats) { if (!State->IsBeatComplete(MissionId, Prior)) { OutError = TEXT("Cinematic prerequisites are incomplete."); return false; } }
    if (!State->HasKnowledge(State->GetActiveProtagonist(), Beat->RequiredKnowledge)) { OutError = TEXT("The protagonist has not learned this scene's required context."); return false; }
    for (const auto& Required : Beat->RequiredState) { if (State->GetStateValue(Required.Key) != Required.Value) { OutError = TEXT("Cinematic campaign-state prerequisites do not match."); return false; } }
    auto* Actor = CastChecked<ANarrativeLevelSequenceActor>(GetOwner());
    if (!Actor->CanAcceptPlayback() || !Actor->GetSequencePlayer() || Actor->GetSequencePlayer()->IsPlaying() || Actor->GetSequencePlayer()->IsPaused())
    { OutError = TEXT("The Narrative sequence actor is already owned by another playback."); return false; }
    bPlaybackSuperseded = false;
    ReservedPlaybackGeneration = Actor->GetPlaybackGeneration();
    ExpectedPlaybackGeneration = 0;
    auto* Saves = GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
    if (!Saves || Saves->WriteCheckpoint(ESovSaveBoundary::CanonGate, BeatId, OutError) != ESovSaveResult::Success) { return false; }
    if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || !IsContextCurrent()) { return false; }
    StreamingLevels.Reset();
    for (FName Level : RequiredStreamingLevels)
    {
        auto* Streaming = UGameplayStatics::GetStreamingLevel(this, Level);
        if (!Streaming) { OutError = TEXT("A declared cinematic streaming level is absent from this world."); return false; }
        StreamingLevels.Add(Streaming);
    }
    if (!AcquirePartitionSources(OutError)) { return false; }
    for (auto& Streaming : StreamingLevels) { Streaming->SetShouldBeLoaded(true); Streaming->SetShouldBeVisible(true); }
    if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || !IsContextCurrent()) { ReleasePartitionSources(); return false; }
    ExpectedPlaybackGeneration = 0; SessionId = FGuid::NewGuid(); LoadingStartedSeconds = FPlatformTime::Seconds(); PlayedSeconds = 0.0; bFullViewEligible = true;
    OriginalViewTarget = Player->GetViewTarget(); OriginalControlRotation = Player->GetControlRotation();
    bOwnInput = true; Player->SetIgnoreMoveInput(true); Player->SetIgnoreLookInput(true);
    bOwnSequenceTag = true;
    PlayerASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
    if (RequestEpoch != Epoch || !IsContextCurrent()) { if (RequestEpoch == Epoch) { Abort(TEXT("Cinematic ownership changed during setup.")); } return false; }
    ChangePhase(ESovCinematicPhase::Loading);
    if (RequestEpoch != Epoch || Phase != ESovCinematicPhase::Loading || !IsContextCurrent())
    { if (RequestEpoch == Epoch) { Abort(TEXT("Cinematic ownership changed during loading notification.")); } return false; }
    // Core ticker survives world pause and accidental component tick disabling; it never drives Sequencer itself.
    PreparationWatchdog = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
        [this, Epoch](float) { return CheckPreparationWatchdog(Epoch); }), .1f);
    TArray<FSoftObjectPath> Paths = PreloadAssets; Paths.AddUnique(Sequence.ToSoftObjectPath());
    const auto RequestedLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
    if (RequestEpoch != Epoch || !IsContextCurrent())
    {
        if (RequestedLoad.IsValid() && !RequestedLoad->HasLoadCompleted()) { RequestedLoad->CancelHandle(); }
        if (RequestEpoch == Epoch) { Abort(TEXT("Cinematic ownership changed during asset request.")); } return false;
    }
    LoadHandle = RequestedLoad;
    if (!LoadHandle.IsValid()) { Abort(TEXT("Cinematic asset preload could not start.")); return false; }
    OutError.Reset(); return true;
}

bool USovCampaignCinematicComponent::CheckPreparationWatchdog(uint64 Epoch)
{
    if (RequestEpoch != Epoch || bEndingPlay || (Phase != ESovCinematicPhase::Loading && Phase != ESovCinematicPhase::Playing && Phase != ESovCinematicPhase::Paused)) { return false; }
    if (bFinishing) { return true; }
    if (!IsRegistered() || !IsComponentTickEnabled()
        || (Phase == ESovCinematicPhase::Loading && SovCinematicPolicy::LoadingExpired(FPlatformTime::Seconds() - LoadingStartedSeconds, LoadingTimeoutSeconds)))
    { Abort(TEXT("Cinematic preparation timed out or its component stopped ticking.")); return false; }
    return true;
}

void USovCampaignCinematicComponent::StartPreparedPlayback()
{
    FString Error; auto* Asset = Sequence.Get(); auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner());
    if (!IsContextCurrent() || !OwnsPlaybackGeneration() || !Actor || !Asset || !ArePartitionRegionsReady()
        || !ValidatePresentationSequence(Asset, Error) || !ResolveParticipants(Error) || !ResolveTransitPostconditions(Error)) { Abort(Error); return; }
    for (const auto& Path : PreloadAssets) { if (!Path.ResolveObject()) { Abort(TEXT("A required cinematic presentation asset failed to load.")); return; } }
    auto* Scene = Asset->GetMovieScene(); const auto Range = Scene->GetPlaybackRange();
    DurationSeconds = Scene->GetTickResolution().AsSeconds(FFrameTime(Range.GetUpperBoundValue() - Range.GetLowerBoundValue()));
    if (!FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0 || DurationSeconds > 3600.0) { Abort(TEXT("Cinematic duration is outside the supported finite range.")); return; }
    auto Settings = Actor->NarrativeSequenceParams;
    Settings.bAutoPlay = false; Settings.bCanSkip = false; Settings.bPauseAtEnd = false; Settings.PlayRate = 1.f;
    Settings.LoopCount.Value = 0; Settings.StartTime = 0.f; Settings.bRandomStartTime = false;
    Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;
    Settings.RequiredParticipantBindingTags.Reset();
    for (const auto& Participant : Participants) { Settings.RequiredParticipantBindingTags.Add(Participant.BindingTag); }
    Actor->OwnerControllers = { Controller.Get() };
    const uint64 Epoch = RequestEpoch;
    ExpectedPlaybackGeneration = Actor->GetPlaybackGeneration() + 1;
    Actor->UpdateSequence(Asset, Settings);
    if (RequestEpoch != Epoch || !IsContextCurrent() || !IsValid(Actor) || Actor->GetPlaybackGeneration() != ExpectedPlaybackGeneration)
    { if (RequestEpoch == Epoch) { Abort(TEXT("Cinematic sequence changed during preparation.")); } return; }
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        Actor->SetBindingByTag(Participants[Index].BindingTag, {Snapshot[Index].Character.Get()});
        if (RequestEpoch != Epoch || !IsContextCurrent() || !OwnsPlaybackGeneration())
        { if (RequestEpoch == Epoch) { Abort(TEXT("Cinematic bindings changed during preparation.")); } return; }
    }
    auto* Player = Actor->GetSequencePlayer();
    EndPositionSeconds = Player->GetEndTime().AsSeconds();
    Player->OnPlay.AddUniqueDynamic(this, &USovCampaignCinematicComponent::HandleStarted);
    Player->OnFinished.AddUniqueDynamic(this, &USovCampaignCinematicComponent::HandleFinished);
    Player->OnStop.AddUniqueDynamic(this, &USovCampaignCinematicComponent::HandleStopped);
    Actor->OnPlaybackFailed.AddUniqueDynamic(this, &USovCampaignCinematicComponent::HandleFailed);
    Player->Play();
}

bool USovCampaignCinematicComponent::ValidateBindings() const
{
    const auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner());
    if (!Actor || !Actor->GetSequencePlayer() || Actor->GetSequencePlayer()->GetSequence() != Sequence.Get() || !Sequence.Get()) { return false; }
    const auto Tagged = Sequence.Get()->GetMovieScene()->AllTaggedBindings();
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        const auto* IDs = Tagged.Find(Participants[Index].BindingTag);
        if (!IDs || IDs->IDs.IsEmpty()) { return false; }
        for (const auto& Binding : IDs->IDs)
        {
            bool bFound = false;
            for (const auto& Object : Actor->GetSequencePlayer()->FindBoundObjects(Binding.GetGuid(), MovieSceneSequenceID::Root))
            {
                if (!Object.IsValid() || Object.Get() != Snapshot[Index].Character.Get()) { return false; }
                bFound = true;
            }
            if (!bFound) { return false; }
        }
    }
    return true;
}

void USovCampaignCinematicComponent::HandleStarted()
{
    if (bFinishing || (Phase != ESovCinematicPhase::Loading && Phase != ESovCinematicPhase::Paused))
    {
        bPlaybackSuperseded = true;
        if (!bFinishing) { Abort(TEXT("An external playback restarted the managed cinematic.")); }
        return;
    }
    if (Phase == ESovCinematicPhase::Paused && OwnsPlaybackGeneration()) { return; }
    auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner()); FString Error;
    if (!Actor || Actor->GetPlaybackGeneration() != ExpectedPlaybackGeneration || !IsContextCurrent() || !ValidateParticipants(false, Error)) { Abort(Error); return; }
    if (!ValidateBindings()) { Abort(TEXT("Narrative resolved a different cinematic participant binding.")); return; }
    LastPositionSeconds = Actor->GetSequencePlayer()->GetCurrentTime().AsSeconds();
    LastSampleWorldSeconds = GetWorld()->GetTimeSeconds();
    ChangePhase(ESovCinematicPhase::Playing);
}

bool USovCampaignCinematicComponent::SetCinematicPaused(bool bPause)
{
    auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner());
    if (!IsContextCurrent() || !Actor || !Actor->GetSequencePlayer() || Actor->GetPlaybackGeneration() != ExpectedPlaybackGeneration
        || bFinishing || (Phase != ESovCinematicPhase::Playing && Phase != ESovCinematicPhase::Paused)) { return false; }
    const uint64 Epoch = RequestEpoch;
    if (bPause && Phase == ESovCinematicPhase::Playing) { ObservePlaybackProgress(false); Actor->GetSequencePlayer()->Pause(); }
    else if (!bPause && Phase == ESovCinematicPhase::Paused) { LastSampleWorldSeconds = GetWorld()->GetTimeSeconds(); Actor->GetSequencePlayer()->Play(); }
    if (RequestEpoch != Epoch || !IsContextCurrent() || Actor->GetPlaybackGeneration() != ExpectedPlaybackGeneration) { return false; }
    ChangePhase(bPause ? ESovCinematicPhase::Paused : ESovCinematicPhase::Playing); return true;
}

bool USovCampaignCinematicComponent::ObservePlaybackProgress(bool bTerminal)
{
    const auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner());
    if (!Actor || !Actor->GetSequencePlayer() || !GetWorld() || !OwnsPlaybackGeneration() || !ValidateBindings()) { bFullViewEligible = false; return false; }
    const double Now = GetWorld()->GetTimeSeconds(); const double Position = Actor->GetSequencePlayer()->GetCurrentTime().AsSeconds();
    bFullViewEligible &= SovCinematicPolicy::ValidProgress(LastPositionSeconds, Position, Now - LastSampleWorldSeconds, Actor->GetSequencePlayer()->GetPlayRate());
    if (bFullViewEligible) { PlayedSeconds += FMath::Max(0.0, Position - LastPositionSeconds); }
    LastPositionSeconds = Position; LastSampleWorldSeconds = Now;
    return bFullViewEligible && (!bTerminal || SovCinematicPolicy::CompleteView(PlayedSeconds, DurationSeconds, Position, EndPositionSeconds));
}

bool USovCampaignCinematicComponent::RequestSkip(FString& OutError)
{
    auto* State = Controller.IsValid() ? Controller->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
    if (!State || !State->CanSkipCinematic(BeatId) || !IsContextCurrent() || (Phase != ESovCinematicPhase::Playing && Phase != ESovCinematicPhase::Paused))
    { OutError = TEXT("Skip requires a prior complete viewing of this non-interactive scene."); return false; }
    return Commit(true, OutError);
}

void USovCampaignCinematicComponent::HandleFinished()
{
    if (bFinishing || (Phase != ESovCinematicPhase::Playing && Phase != ESovCinematicPhase::Paused)) { return; }
    if (!ObservePlaybackProgress(true)) { Abort(TEXT("An interrupted or jumped presentation cannot count as its first full viewing.")); return; }
    FString Error; Commit(false, Error);
}
void USovCampaignCinematicComponent::HandleStopped()
{
    if (bFinishing || !GetWorld()) { return; }
    const uint64 Epoch = RequestEpoch;
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Epoch]()
    { if (RequestEpoch == Epoch && (Phase == ESovCinematicPhase::Playing || Phase == ESovCinematicPhase::Paused)) { Abort(TEXT("Cinematic playback stopped before completion.")); } }));
}
void USovCampaignCinematicComponent::HandleFailed() { if (!bFinishing) { Abort(TEXT("Required cinematic participant became unavailable.")); } }

bool USovCampaignCinematicComponent::HasCommitReceipt(const USovCampaignStateComponent* State, FName RequestedBeat, bool bSkipped) const
{
    return State && OwnsPlaybackGeneration() && Cast<ANarrativeLevelSequenceActor>(GetOwner())
        && CastChecked<ANarrativeLevelSequenceActor>(GetOwner())->GetPlaybackGeneration() == ExpectedPlaybackGeneration
        && bReceiptAvailable && bFinishing && Phase == ESovCinematicPhase::Committing && SessionId.IsValid()
        && bReceiptSkipped == bSkipped && RequestedBeat == BeatId && IsContextCurrent() && Controller.Get() == State->GetOwner();
}
bool USovCampaignCinematicComponent::ConsumeCommitReceipt(const USovCampaignStateComponent* State, FName RequestedBeat, bool bSkipped)
{ if (!HasCommitReceipt(State, RequestedBeat, bSkipped)) { return false; } bReceiptAvailable = false; return true; }

bool USovCampaignCinematicComponent::Commit(bool bSkipped, FString& OutError)
{
    auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner());
    if (bFinishing || !Actor || !OwnsPlaybackGeneration() || !ValidateBindings() || !IsContextCurrent() || !ArePartitionRegionsReady()
        || !ValidateParticipants(true, OutError) || !ValidateTransitPostconditions(false, OutError)) { Abort(OutError); return false; }
    TGuardValue<bool> Finishing(bFinishing, true); const uint64 Epoch = RequestEpoch; Phase = ESovCinematicPhase::Committing;
    Actor->GetSequencePlayer()->Stop(); // Restore evaluated tracks; never jump across arbitrary skipped events.
    if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || Actor->GetSequencePlayer()->IsPlaying() || Actor->GetSequencePlayer()->IsPaused()
        || !IsContextCurrent() || !ValidateParticipants(true, OutError)) { RestoreParticipants(); ReleaseOwnership(); ChangePhase(ESovCinematicPhase::Failed, OutError); return false; }
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        const auto& Contract = Participants[Index]; auto* Character = Snapshot[Index].Character.Get();
        if (Contract.bApplyExitTransform && !Character->SetActorTransform(Contract.ExitTransform, false, nullptr, ETeleportType::TeleportPhysics))
        { OutError = TEXT("Cinematic exit transform could not be applied."); RestoreParticipants(); ReleaseOwnership(); ChangePhase(ESovCinematicPhase::Failed, OutError); return false; }
        if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || !IsContextCurrent() || !IsValid(Character) || Character->IsActorBeingDestroyed()
            || UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character) != Snapshot[Index].ASC.Get())
        { RestoreParticipants(); ReleaseOwnership(); if (!bEndingPlay) { ChangePhase(ESovCinematicPhase::Failed, TEXT("Participant changed during cinematic exit.")); } return false; }
        if (Contract.ExitWield != ESovCinematicExitWield::Keep)
        {
            FWeaponWieldState Wield;
            if (Contract.ExitWield == ESovCinematicExitWield::DrawRequiredWeapon) { Wield.EquipSlots.AddTag(Contract.EquipmentSlot); Wield.WieldSlots.AddTag(Contract.WieldSlot); }
            Character->SetWieldState(Wield);
        }
        if (RequestEpoch != Epoch || !OwnsPlaybackGeneration() || !IsContextCurrent() || !ValidateParticipants(false, OutError))
        { RestoreParticipants(); ReleaseOwnership(); if (!bEndingPlay) { ChangePhase(ESovCinematicPhase::Failed, OutError); } return false; }
        const auto Wield = Character->GetWeaponWieldState();
        const bool bWieldMatches = Contract.ExitWield == ESovCinematicExitWield::Keep
            || (Contract.ExitWield == ESovCinematicExitWield::Holster && Wield.EquipSlots.IsEmpty() && Wield.WieldSlots.IsEmpty())
            || (Contract.ExitWield == ESovCinematicExitWield::DrawRequiredWeapon && Wield.EquipSlots.Num() == 1 && Wield.WieldSlots.Num() == 1
                && Wield.EquipSlots.HasTagExact(Contract.EquipmentSlot) && Wield.WieldSlots.HasTagExact(Contract.WieldSlot)
                && Wield.EquipWeapons.Num() == 1 && Wield.EquipWeapons[0] == Snapshot[Index].RequiredWeapon.Get());
        if (!bWieldMatches || (Contract.bApplyExitTransform && !Character->GetActorTransform().Equals(Contract.ExitTransform, .1f)))
        { OutError = TEXT("Cinematic exit postconditions did not survive their native callbacks."); RestoreParticipants(); ReleaseOwnership(); ChangePhase(ESovCinematicPhase::Failed, OutError); return false; }
    }
    if (!OwnsPlaybackGeneration() || !ArePartitionRegionsReady() || !ApplyTransitPostconditions(OutError)
        || !ValidateParticipants(true, OutError) || !ValidateExitPostconditions(OutError))
    { RestoreParticipants(); ReleaseOwnership(); if (!bEndingPlay) { ChangePhase(ESovCinematicPhase::Failed, OutError); } return false; }
    bReceiptAvailable = true; bReceiptSkipped = bSkipped;
    auto* State = Controller->FindComponentByClass<USovCampaignStateComponent>();
    const auto Result = State->CompleteCinematic(this, bSkipped);
    bReceiptAvailable = false;
    if (Result != ESovCampaignResult::Applied)
    {
        OutError = TEXT("Cinematic postconditions could not commit their campaign beat.");
        RestoreParticipants(); ReleaseOwnership(); ChangePhase(ESovCinematicPhase::Failed, OutError); return false;
    }
    if (RequestEpoch == Epoch) { ReleaseOwnership(); }
    if (!bEndingPlay && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()) { ChangePhase(ESovCinematicPhase::Completed); }
    OutError.Reset(); return true;
}

bool USovCampaignCinematicComponent::OwnsPlaybackGeneration() const
{
    const auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner());
    return !bPlaybackSuperseded && Actor && Actor->GetPlaybackGeneration() == (ExpectedPlaybackGeneration ? ExpectedPlaybackGeneration : ReservedPlaybackGeneration)
        && (ExpectedPlaybackGeneration != 0 || (Actor->GetSequencePlayer() && !Actor->GetSequencePlayer()->IsPlaying() && !Actor->GetSequencePlayer()->IsPaused()));
}

bool USovCampaignCinematicComponent::ValidateExitPostconditions(FString& OutError) const
{
    if (!ValidateTransitPostconditions(true, OutError)) { return false; }
    for (int32 Index = 0; Index < Participants.Num(); ++Index)
    {
        const auto& Contract = Participants[Index]; const auto* Character = Snapshot[Index].Character.Get();
        if (!Character || Character->IsActorBeingDestroyed()) { return false; }
        const auto Wield = Character->GetWeaponWieldState();
        const bool bWieldMatches = Contract.ExitWield == ESovCinematicExitWield::Keep
            || (Contract.ExitWield == ESovCinematicExitWield::Holster && Wield.EquipSlots.IsEmpty() && Wield.WieldSlots.IsEmpty())
            || (Contract.ExitWield == ESovCinematicExitWield::DrawRequiredWeapon && Wield.EquipSlots.Num() == 1 && Wield.WieldSlots.Num() == 1
                && Wield.EquipSlots.HasTagExact(Contract.EquipmentSlot) && Wield.WieldSlots.HasTagExact(Contract.WieldSlot)
                && Wield.EquipWeapons.Num() == 1 && Wield.EquipWeapons[0] == Snapshot[Index].RequiredWeapon.Get());
        if (!bWieldMatches || (Contract.bApplyExitTransform && !Character->GetActorTransform().Equals(Contract.ExitTransform, .1f)))
        { OutError = TEXT("Cinematic final postconditions were changed by another participant callback."); return false; }
    }
    return true;
}

void USovCampaignCinematicComponent::RestoreParticipants()
{
    RestoreTransitPostconditions();
    for (const auto& Entry : Snapshot)
    {
        if (!OwnsPlaybackGeneration()) { return; }
        auto* Character = Entry.Character.Get();
        if (!Character || Character->IsActorBeingDestroyed() || UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character) != Entry.ASC.Get()) { continue; }
        if (!Character->GetActorTransform().Equals(Entry.Transform, .1f)) { Character->SetActorTransform(Entry.Transform, false, nullptr, ETeleportType::TeleportPhysics); }
        if (!OwnsPlaybackGeneration() || !IsValid(Character) || Character->IsActorBeingDestroyed() || UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character) != Entry.ASC.Get()) { continue; }
        const auto Current = Character->GetWeaponWieldState();
        if (Current.EquipSlots != Entry.Wield.EquipSlots || Current.WieldSlots != Entry.Wield.WieldSlots) { Character->SetWieldState(Entry.Wield); }
    }
}

void USovCampaignCinematicComponent::ReleaseOwnership()
{
    ++RequestEpoch; bReceiptAvailable = false;
    if (PreparationWatchdog.IsValid()) { FTSTicker::RemoveTicker(PreparationWatchdog); PreparationWatchdog.Reset(); }
    if (auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner()))
    {
        if (auto* Player = Actor->GetSequencePlayer())
        {
            Player->OnPlay.RemoveDynamic(this, &USovCampaignCinematicComponent::HandleStarted);
            Player->OnFinished.RemoveDynamic(this, &USovCampaignCinematicComponent::HandleFinished);
            Player->OnStop.RemoveDynamic(this, &USovCampaignCinematicComponent::HandleStopped);
        }
        Actor->OnPlaybackFailed.RemoveDynamic(this, &USovCampaignCinematicComponent::HandleFailed);
    }
    if (bOwnSequenceTag) { bOwnSequenceTag = false; if (auto* ASC = PlayerASC.Get()) { ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled); } }
    if (bOwnInput)
    {
        bOwnInput = false;
        if (auto* PC = Controller.Get())
        {
            PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false);
            if (OwnsPlaybackGeneration() && PC->GetPawn() == PlayerPawn.Get())
            { PC->SetViewTarget(OriginalViewTarget.IsValid() ? OriginalViewTarget.Get() : PlayerPawn.Get()); PC->SetControlRotation(OriginalControlRotation); }
        }
    }
    if (LoadHandle.IsValid() && !LoadHandle->HasLoadCompleted()) { LoadHandle->CancelHandle(); }
    LoadHandle.Reset(); StreamingLevels.Reset(); ReleasePartitionSources();
}

void USovCampaignCinematicComponent::Abort(const FString& Reason)
{
    if (bFinishing || ((Phase == ESovCinematicPhase::Idle || Phase == ESovCinematicPhase::Failed || Phase == ESovCinematicPhase::Completed)
        && !bOwnInput && !bOwnSequenceTag && PartitionLeases.IsEmpty())) { return; }
    TGuardValue<bool> Finishing(bFinishing, true);
    if (auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner()); OwnsPlaybackGeneration() && ExpectedPlaybackGeneration != 0 && Actor && Actor->GetSequencePlayer()) { Actor->GetSequencePlayer()->Stop(); }
    RestoreParticipants(); ReleaseOwnership(); ChangePhase(ESovCinematicPhase::Failed, Reason.IsEmpty() ? TEXT("Cinematic preflight failed.") : Reason);
}

void USovCampaignCinematicComponent::ChangePhase(ESovCinematicPhase NewPhase, const FString& Reason)
{ Phase = NewPhase; OnPhaseChanged.Broadcast(Phase, Reason); }

void USovCampaignCinematicComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(Delta, TickType, TickFunction);
    if (bFinishing || bEndingPlay || !FMath::IsFinite(Delta) || Delta < 0.f) { return; }
    if (Phase == ESovCinematicPhase::Loading)
    {
        if (!IsContextCurrent() || !OwnsPlaybackGeneration()
            || SovCinematicPolicy::LoadingExpired(FPlatformTime::Seconds() - LoadingStartedSeconds, LoadingTimeoutSeconds))
        { Abort(TEXT("Cinematic preparation timed out or lost its protagonist; required assets, cells and activation actors must be available.")); return; }
        bool bLevelsReady = true; for (const auto& Level : StreamingLevels) { bLevelsReady &= Level && Level->IsLevelLoaded() && Level->IsLevelVisible(); }
        ConsecutivePartitionReadyTicks = static_cast<uint8>(SovCinematicPolicy::AdvanceReadyObservations(ConsecutivePartitionReadyTicks, ArePartitionRegionsReady()));
        if (LoadHandle.IsValid() && LoadHandle->HasLoadCompleted() && bLevelsReady && ConsecutivePartitionReadyTicks >= 2 && ExpectedPlaybackGeneration == 0) { StartPreparedPlayback(); }
    }
    else if (Phase == ESovCinematicPhase::Playing || Phase == ESovCinematicPhase::Paused)
    {
        auto* Actor = Cast<ANarrativeLevelSequenceActor>(GetOwner()); FString Error;
        if (!IsContextCurrent() || !Actor || !OwnsPlaybackGeneration() || !ArePartitionRegionsReady()
            || !ValidateBindings() || !ValidateParticipants(false, Error) || !ValidateTransitPostconditions(false, Error))
        { bFullViewEligible = false; Abort(Error.IsEmpty() ? TEXT("Required cinematic cells or activation actors became unavailable.") : Error); return; }
        if (Phase == ESovCinematicPhase::Playing && Actor->GetSequencePlayer()->IsPlaying())
        {
            ObservePlaybackProgress(false);
        }
    }
}

void USovCampaignCinematicComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true; Abort(TEXT("Cinematic owner left the world.")); ReleaseOwnership();
    Super::EndPlay(EndPlayReason);
}
