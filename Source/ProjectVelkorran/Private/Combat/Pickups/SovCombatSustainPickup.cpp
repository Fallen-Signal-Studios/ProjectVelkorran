// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Combat/Pickups/SovCombatSustainPickup.h"

#include "Characters/SovPlayerCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

ASovCombatSustainPickup::ASovCombatSustainPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);

	GroundCollisionSphere =
		CreateDefaultSubobject<USphereComponent>(TEXT("GroundCollisionSphere"));
	SetRootComponent(GroundCollisionSphere);
	GroundCollisionSphere->InitSphereRadius(GroundCollisionRadius);
	GroundCollisionSphere->SetCollisionObjectType(ECC_PhysicsBody);
	GroundCollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GroundCollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	GroundCollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	GroundCollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	GroundCollisionSphere->SetGenerateOverlapEvents(false);
	GroundCollisionSphere->SetCanEverAffectNavigation(false);
	GroundCollisionSphere->SetEnableGravity(true);
	GroundCollisionSphere->SetLinearDamping(GroundLinearDamping);
	GroundCollisionSphere->SetAngularDamping(GroundAngularDamping);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(GroundCollisionSphere);
	PickupSphere->InitSphereRadius(PickupRadius);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	PickupSphere->SetCanEverAffectNavigation(false);
	PickupSphere->OnComponentBeginOverlap.AddUniqueDynamic(
		this,
		&ThisClass::HandlePickupOverlap);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(GroundCollisionSphere);

	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(VisualRoot);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetGenerateOverlapEvents(false);
	PickupMesh->SetCanEverAffectNavigation(false);

	IdleNiagaraComponent =
		CreateDefaultSubobject<UNiagaraComponent>(TEXT("IdleNiagara"));
	IdleNiagaraComponent->SetupAttachment(VisualRoot);
	IdleNiagaraComponent->SetAutoActivate(true);
	IdleNiagaraComponent->SetCanEverAffectNavigation(false);

	IdleAudioComponent =
		CreateDefaultSubobject<UAudioComponent>(TEXT("IdleAudio"));
	IdleAudioComponent->SetupAttachment(VisualRoot);
	IdleAudioComponent->SetAutoActivate(true);
}

void ASovCombatSustainPickup::BeginPlay()
{
	Super::BeginPlay();

	GroundCollisionSphere->SetSphereRadius(
		FMath::Max(GroundCollisionRadius, 1.0f));
	GroundCollisionSphere->SetLinearDamping(
		FMath::Max(GroundLinearDamping, 0.0f));
	GroundCollisionSphere->SetAngularDamping(
		FMath::Max(GroundAngularDamping, 0.0f));
	GroundCollisionSphere->SetEnableGravity(true);
	GroundCollisionSphere->SetSimulatePhysics(true);
	GroundCollisionSphere->WakeAllRigidBodies();

	PickupSphere->SetSphereRadius(FMath::Max(PickupRadius, 1.0f));
	SetActorTickEnabled(
		GetNetMode() != NM_DedicatedServer
		&& FMath::Abs(RotationRateDegrees) > KINDA_SMALL_NUMBER);

	if (HasAuthority() && !bClaimed)
	{
		SetLifeSpan(FMath::Max(PickupLifetimeSeconds, 0.1f));
	}

	if (bClaimed)
	{
		DisableIdlePresentation();
	}
}

void ASovCombatSustainPickup::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bClaimed || !IsValid(VisualRoot))
	{
		return;
	}

	if (FMath::Abs(RotationRateDegrees) > KINDA_SMALL_NUMBER)
	{
		VisualRoot->AddLocalRotation(
			FRotator(0.0f, RotationRateDegrees * DeltaSeconds, 0.0f));
	}
}

void ASovCombatSustainPickup::InitializePickupLifetime(
	const float InLifetimeSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	PickupLifetimeSeconds = FMath::Max(InLifetimeSeconds, 0.1f);
	if (HasActorBegunPlay() && !bClaimed)
	{
		SetLifeSpan(PickupLifetimeSeconds);
	}
}

bool ASovCombatSustainPickup::TryGrantTo(
	ASovPlayerCharacterBase* CollectingPlayer)
{
	return false;
}

void ASovCombatSustainPickup::HandlePickupOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!HasAuthority() || bClaimed)
	{
		return;
	}

	ASovPlayerCharacterBase* CollectingPlayer =
		Cast<ASovPlayerCharacterBase>(OtherActor);
	UNarrativeAbilitySystemComponent* PlayerAbilitySystem =
		IsValid(CollectingPlayer)
			? CollectingPlayer->GetNarrativeAbilitySystemComponent()
			: nullptr;
	if (!IsValid(CollectingPlayer)
		|| CollectingPlayer->GetPlayerController() == nullptr
		|| !IsValid(PlayerAbilitySystem)
		|| PlayerAbilitySystem->IsDead()
		|| !TryGrantTo(CollectingPlayer))
	{
		return;
	}

	bClaimed = true;
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForceNetUpdate();
	MulticastPlayCollectionPresentation();
	SetLifeSpan(FMath::Max(CollectionCleanupDelay, 0.05f));
}

void ASovCombatSustainPickup::OnRep_Claimed()
{
	if (bClaimed)
	{
		DisableIdlePresentation();
	}
}

void ASovCombatSustainPickup::MulticastPlayCollectionPresentation_Implementation()
{
	DisableIdlePresentation();

	if (IsValid(CollectionNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			CollectionNiagaraSystem,
			GetActorLocation(),
			GetActorRotation(),
			CollectionNiagaraScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}

	if (IsValid(CollectionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			CollectionSound,
			GetActorLocation(),
			CollectionSoundVolume,
			CollectionSoundPitch);
	}

	BP_OnPickupCollected();
}

void ASovCombatSustainPickup::DisableIdlePresentation()
{
	SetActorTickEnabled(false);

	if (IsValid(PickupSphere))
	{
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (IsValid(GroundCollisionSphere))
	{
		GroundCollisionSphere->SetSimulatePhysics(false);
		GroundCollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (IsValid(PickupMesh))
	{
		PickupMesh->SetVisibility(false, true);
	}
	if (IsValid(IdleNiagaraComponent))
	{
		IdleNiagaraComponent->DeactivateImmediate();
	}
	if (IsValid(IdleAudioComponent))
	{
		IdleAudioComponent->Stop();
	}
}

void ASovCombatSustainPickup::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASovCombatSustainPickup, bClaimed);
}
