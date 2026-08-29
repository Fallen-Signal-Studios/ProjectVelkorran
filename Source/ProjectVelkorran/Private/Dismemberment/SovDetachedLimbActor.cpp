// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Dismemberment/SovDetachedLimbActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

ASovDetachedLimbActor::ASovDetachedLimbActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	LimbMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LimbMesh"));
	SetRootComponent(LimbMesh);
	LimbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASovDetachedLimbActor::InitializeDetachedLimb(
	const FVector InInitialImpulse,
	AActor* InSourceCharacter)
{
	InitialImpulse = InInitialImpulse;
	SourceCharacter = InSourceCharacter;
}

void ASovDetachedLimbActor::BeginPlay()
{
	Super::BeginPlay();

	if (CosmeticLifeSeconds > 0.f)
	{
		SetLifeSpan(CosmeticLifeSeconds);
	}

	if (IsValid(LimbMesh)
		&& bSimulatePhysics
		&& IsValid(LimbMesh->GetSkeletalMeshAsset())
		&& IsValid(LimbMesh->GetPhysicsAsset()))
	{
		LimbMesh->SetCollisionProfileName(CollisionProfileName);
		LimbMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		LimbMesh->SetSimulatePhysics(true);
		LimbMesh->AddImpulse(InitialImpulse, NAME_None, true);
	}

	ReceiveDetachedLimbInitialized(InitialImpulse, SourceCharacter.Get());
}
