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
	SetReplicateMovement(false);
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
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
	VisualRoot->SetupAttachment(PickupSphere);

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

	PickupSphere->SetSphereRadius(FMath::Max(PickupRadius, 1.0f));
	InitialVisualRelativeLocation = VisualRoot->GetRelativeLocation();
	SetActorTickEnabled(
		GetNetMode() != NM_DedicatedServer
		&& (HoverAmplitude > KINDA_SMALL_NUMBER
			|| FMath::Abs(RotationRateDegrees) > KINDA_SMALL_NUMBER));

	if (HasAuthority())
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

	if (HoverAmplitude > KINDA_SMALL_NUMBER)
	{
		const float HoverRadians =
			GetGameTimeSinceCreation() * HoverFrequency * 2.0f * UE_PI;
		const float HoverOffset = FMath::Sin(HoverRadians) * HoverAmplitude;
		VisualRoot->SetRelativeLocation(
			InitialVisualRelativeLocation + FVector(0.0f, 0.0f, HoverOffset));
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
