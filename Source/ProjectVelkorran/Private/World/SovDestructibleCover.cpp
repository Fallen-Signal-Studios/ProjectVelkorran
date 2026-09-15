// Copyright Fallen Signal Studios. All Rights Reserved.
#include "World/SovDestructibleCover.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

ASovDestructibleCover::ASovDestructibleCover()
{
    bReplicates = true;
    PrimaryActorTick.bCanEverTick = false;
    Obstruction = CreateDefaultSubobject<UBoxComponent>(TEXT("Obstruction"));
    SetRootComponent(Obstruction);
    Obstruction->SetCollisionProfileName(TEXT("BlockAll"));
    Obstruction->SetCanEverAffectNavigation(true);
    IntactVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IntactVisual"));
    IntactVisual->SetupAttachment(Obstruction);
    IntactVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    IntactVisual->SetCanEverAffectNavigation(false);
}

void ASovDestructibleCover::BeginPlay()
{
    Super::BeginPlay();
    ReconcileObstruction();
}

float ASovDestructibleCover::TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
    // Missing identity/collection is a content error, never permission to erase cover.
    if (!HasAuthority() || !CanBeDamaged() || !bDestructionEnabled || bBroken || !PlacementGuid.IsValid()
        || !FracturedAsset || !FMath::IsFinite(Amount) || Amount <= 0.f
        || !FMath::IsFinite(RemainingHealth) || RemainingHealth <= 0.f) { return 0.f; }
    const float Applied = FMath::Min(Amount, RemainingHealth);
    RemainingHealth -= Applied;
    if (RemainingHealth <= 0.f)
    {
        if (Event.IsOfType(FPointDamageEvent::ClassID))
        {
            const FVector Direction = static_cast<const FPointDamageEvent&>(Event).ShotDirection;
            if (!Direction.ContainsNaN() && !Direction.IsNearlyZero()) { ImpactDirection = Direction.GetSafeNormal(); }
        }
        bBroken = true;
        ReconcileObstruction();
        SpawnDebris();
    }
    ForceNetUpdate();
    return Applied;
}

void ASovDestructibleCover::ReconcileObstruction()
{
    IntactVisual->SetVisibility(!bBroken, true);
    Obstruction->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    // The component setter updates the navigation octree. Runtime nav generation
    // must also be enabled in each qualified campaign map.
    Obstruction->SetCanEverAffectNavigation(!bBroken);
}

void ASovDestructibleCover::SpawnDebris()
{
    if (Debris || !FracturedAsset || GetNetMode() == NM_DedicatedServer) { return; }
    Debris = NewObject<UGeometryCollectionComponent>(this);
    Debris->SetupAttachment(Obstruction);
    Debris->SetRelativeTransform(IntactVisual->GetRelativeTransform());
    Debris->SetRestCollection(FracturedAsset);
    // Detailed source meshes may contain thousands of disconnected fittings.
    // Such content must be regrouped, never spawned as thousands of rigid bodies.
    const int32 Transforms = Debris->GetInitialLocalRestTransforms().Num();
    if (Transforms < 2 || Transforms > 65) { Debris = nullptr; return; }
    Debris->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Debris->SetCollisionObjectType(ECC_PhysicsBody);
    Debris->SetCollisionResponseToAllChannels(ECR_Ignore);
    Debris->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Debris->SetCanEverAffectNavigation(false);
    Debris->RegisterComponent();
    // OnRegister rebuilds the dynamic collection. Create the solver proxy only
    // after that, otherwise it can retain the pre-registration collection.
    Debris->SetSimulatePhysics(true);
    // Wait for creation of the Chaos proxy; no ticking actor is needed afterwards.
    GetWorldTimerManager().SetTimer(FractureTimer, this, &ThisClass::FractureDebris, .1f, false);
    GetWorldTimerManager().SetTimer(CleanupTimer, this, &ThisClass::ClearDebris,
        FMath::Clamp(FMath::IsFinite(DebrisLifetime) ? DebrisLifetime : 6.f, .2f, 10.f), false);
    if (BreakEffect) { UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakEffect, IntactVisual->Bounds.Origin); }
    if (BreakSound) { UGameplayStatics::PlaySoundAtLocation(this, BreakSound, IntactVisual->Bounds.Origin); }
}

void ASovDestructibleCover::FractureDebris()
{
    if (Debris && bBroken)
    {
        Debris->ApplyExternalStrain(Debris->GetRootIndex(), Debris->Bounds.Origin,
            Debris->Bounds.SphereRadius * 2.f, 0, 1.f, FMath::Max(0.f, BreakStrain));
        GetWorldTimerManager().SetTimer(FractureTimer, this, &ThisClass::ImpulseDebris, .1f, false);
    }
}

void ASovDestructibleCover::ImpulseDebris()
{
    if (!Debris || !bBroken || !Debris->IsRootBroken()) { return; }
    const auto& Transforms = Debris->GetComponentSpaceTransforms3f();
    for (int32 Index = 0; Index < Transforms.Num(); ++Index)
    {
        if (Index == Debris->GetRootIndex()) { continue; }
        const FVector Center = Debris->GetComponentTransform().TransformPosition(FVector(Transforms[Index].GetLocation()));
        const FVector Outward = (Center - Obstruction->Bounds.Origin).GetSafeNormal();
        Debris->ApplyLinearVelocity(Index, ImpactDirection * 150.f + Outward * 80.f + FVector(0, 0, 100));
    }
}

void ASovDestructibleCover::ClearDebris()
{
    GetWorldTimerManager().ClearTimer(FractureTimer);
    GetWorldTimerManager().ClearTimer(CleanupTimer);
    if (Debris) { Debris->DestroyComponent(); Debris = nullptr; }
}

void ASovDestructibleCover::Load_Implementation()
{
    // Restore the authored state, not transient fragment transforms or effects.
    ClearDebris();
    ReconcileObstruction();
    ForceNetUpdate();
}

void ASovDestructibleCover::OnRep_Broken()
{
    ReconcileObstruction();
    // Late relevancy and checkpoint restoration must not replay a break effect.
    if (!bBroken) { ClearDebris(); }
}

void ASovDestructibleCover::EndPlay(EEndPlayReason::Type Reason)
{
    ClearDebris();
    Super::EndPlay(Reason);
}

void ASovDestructibleCover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASovDestructibleCover, RemainingHealth);
    DOREPLIFETIME(ASovDestructibleCover, bBroken);
}
