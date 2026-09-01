// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Characters/SovDroneNPCBase.h"

#include "AIController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "BrainComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Effects/SovGameplayEffect_ReformationDroneWeapons.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Presentation/SovReformationDroneSelfDestructPresentation.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

USovDroneDismembermentComponent::USovDroneDismembermentComponent()
{
	bDismembermentEnabled = false;
	bUseSKMannequinBoneMap = false;
	bSpawnDeathBloodPuddleOnDeath = false;
}

ASovDroneNPCBase::ASovDroneNPCBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<
		USovDroneDismembermentComponent>(TEXT("SovDismembermentComponent")))
{
	PrimaryActorTick.bCanEverTick = true;
	DeathExplosionDamageEffectClass =
		USovGameplayEffect_ReformationDroneDamage::StaticClass();
}

void ASovDroneNPCBase::BeginPlay()
{
	Super::BeginPlay();

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		InitialMeshRelativeLocation = MeshComponent->GetRelativeLocation();
		InitialMeshCollisionProfile = MeshComponent->GetCollisionProfileName();
		InitialMeshCollisionEnabled = MeshComponent->GetCollisionEnabled();
		bInitialMeshHiddenInGame = MeshComponent->bHiddenInGame;
	}

	if (UCapsuleComponent* DroneCapsule = GetCapsuleComponent())
	{
		InitialCapsuleCollisionProfile = DroneCapsule->GetCollisionProfileName();
		InitialCapsuleCollisionEnabled = DroneCapsule->GetCollisionEnabled();
	}
}

void ASovDroneNPCBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	USkeletalMeshComponent* MeshComponent = GetMesh();
	const UWorld* World = GetWorld();
	if (!bEnableVerticalHoverVariation
		|| bDeathExplosionTriggered
		|| !IsValid(MeshComponent)
		|| !IsValid(World)
		|| HoverVariationAmplitude <= KINDA_SMALL_NUMBER
		|| HoverVariationFrequency <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector HoverLocation = InitialMeshRelativeLocation;
	HoverLocation.Z += FMath::Sin(
		World->GetTimeSeconds() * UE_TWO_PI * HoverVariationFrequency)
		* HoverVariationAmplitude;
	MeshComponent->SetRelativeLocation(HoverLocation);
}

void ASovDroneNPCBase::SuppressNextDeathExplosion()
{
	if (HasAuthority() && !bDeathExplosionTriggered)
	{
		bSuppressNextDeathExplosion = true;
	}
}

void ASovDroneNPCBase::ClearDeathExplosionSuppression()
{
	if (HasAuthority() && !bDeathExplosionTriggered)
	{
		bSuppressNextDeathExplosion = false;
	}
}

void ASovDroneNPCBase::HandleDeath_Implementation(
	AActor* KilledActor,
	UNarrativeAbilitySystemComponent* KilledActorASC,
	const bool bIsDead)
{
	// The committed presentation is authoritative and survives the ability's
	// synchronous cancellation during Narrative death. Sample it before the base
	// implementation hides owned presentation actors.
	const bool bNativeSelfDestructCommitted =
		bIsDead && HasAuthority() && HasCommittedNativeSelfDestruct();

	Super::HandleDeath_Implementation(KilledActor, KilledActorASC, bIsDead);

	if (bIsDead)
	{
		if (HasAuthority()
			&& bEnableDeathExplosionOnDeath
			&& !bSuppressNextDeathExplosion
			&& !bNativeSelfDestructCommitted
			&& !bDeathExplosionTriggered)
		{
			TriggerDeathExplosion();
		}
		else
		{
			// This is also the dead-state latch used to stop cosmetic hover.
			bDeathExplosionTriggered = true;
		}
	}
	else
	{
		bDeathExplosionTriggered = false;
		bSuppressNextDeathExplosion = false;
		ApplyDeathShutdownState(false);
	}
}

void ASovDroneNPCBase::SetRagdoll(const bool bWantsRagdoll)
{
	if (!bWantsRagdoll)
	{
		Super::SetRagdoll(false);
		ApplyDeathShutdownState(false);
		return;
	}

	// Mechanical drones deliberately never enter Narrative's humanoid ragdoll path.
	ApplyDeathShutdownState(true);
}

void ASovDroneNPCBase::TriggerDeathExplosion()
{
	if (!HasAuthority() || bDeathExplosionTriggered)
	{
		return;
	}

	bDeathExplosionTriggered = true;
	const FVector ExplosionLocation = GetActorLocation();

	UWorld* World = GetWorld();
	UNarrativeAbilitySystemComponent* SourceASC =
		GetNarrativeAbilitySystemComponent();
	if (IsValid(World)
		&& IsValid(SourceASC)
		&& DeathExplosionDamageEffectClass.Get()
		&& DeathExplosionRadius > KINDA_SMALL_NUMBER
		&& DeathExplosionDamage > KINDA_SMALL_NUMBER)
	{
		FCollisionObjectQueryParams ObjectQuery;
		ObjectQuery.AddObjectTypesToQuery(ECC_Pawn);

		FCollisionQueryParams SphereQuery(
			SCENE_QUERY_STAT(SovDroneDeathExplosion),
			false,
			this);
		SphereQuery.AddIgnoredActor(this);

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(
			Overlaps,
			ExplosionLocation,
			FQuat::Identity,
			ObjectQuery,
			FCollisionShape::MakeSphere(DeathExplosionRadius),
			SphereQuery);

		TSet<UAbilitySystemComponent*> UniqueTargets;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			UAbilitySystemComponent* TargetASC =
				UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
					Overlap.GetActor());
			AActor* TargetActor = IsValid(TargetASC)
				? TargetASC->GetAvatarActor()
				: nullptr;
			if (!IsValid(TargetASC)
				|| TargetASC == SourceASC
				|| UniqueTargets.Contains(TargetASC)
				|| !IsLivingHostileTarget(TargetASC, TargetActor))
			{
				continue;
			}

			const FVector TargetLocation = TargetActor->GetActorLocation();
			FCollisionQueryParams VisibilityQuery(
				SCENE_QUERY_STAT(SovDroneDeathExplosionVisibility),
				true,
				this);
			VisibilityQuery.AddIgnoredActor(this);
			FHitResult BlockingHit;
			const bool bFoundBlockingHit = World->LineTraceSingleByChannel(
					BlockingHit,
					ExplosionLocation,
					TargetLocation,
					DeathExplosionDamagePreventionChannel.GetValue(),
					VisibilityQuery);
			AActor* BlockingActor = BlockingHit.GetActor();
			UAbilitySystemComponent* BlockingASC = IsValid(BlockingActor)
				? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
					BlockingActor)
				: nullptr;
			if (bFoundBlockingHit
				&& BlockingActor != TargetActor
				&& (!IsValid(BlockingActor)
					|| (!BlockingActor->IsOwnedBy(TargetActor)
						&& !TargetActor->IsOwnedBy(BlockingActor)
						&& BlockingASC != TargetASC)))
			{
				continue;
			}

			UniqueTargets.Add(TargetASC);
			const float DistanceAlpha = FMath::Clamp(
				FVector::Distance(ExplosionLocation, TargetLocation)
					/ DeathExplosionRadius,
				0.0f,
				1.0f);
			const float MinimumDamage = DeathExplosionDamage
				* FMath::Clamp(
					DeathExplosionMinimumDamageFraction,
					0.0f,
					1.0f);
			const float AppliedDamage = FMath::Lerp(
				DeathExplosionDamage,
				MinimumDamage,
				DistanceAlpha);

			FGameplayEffectContextHandle EffectContext =
				SourceASC->MakeEffectContext();
			EffectContext.AddInstigator(this, this);
			EffectContext.AddSourceObject(this);
			EffectContext.AddOrigin(ExplosionLocation);
			FGameplayEffectSpecHandle DamageSpecHandle =
				SourceASC->MakeOutgoingSpec(
					DeathExplosionDamageEffectClass,
					1.0f,
					EffectContext);
			if (FGameplayEffectSpec* DamageSpec = DamageSpecHandle.Data.Get())
			{
				DamageSpec->SetSetByCallerMagnitude(
					FNarrativeGameplayTags::Get().SetByCaller_Damage,
					AppliedDamage);
				SourceASC->ApplyGameplayEffectSpecToTarget(
					*DamageSpec,
					TargetASC);
			}
		}
	}

	MulticastPlayDeathExplosion(ExplosionLocation);
}

void ASovDroneNPCBase::ApplyDeathShutdownState(const bool bIsDead)
{
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (bIsDead)
		{
			if (UBrainComponent* BrainComponent = AIController->GetBrainComponent())
			{
				BrainComponent->StopLogic(TEXT("Drone destroyed"));
			}
		}
		else if (UBrainComponent* BrainComponent = AIController->GetBrainComponent())
		{
			BrainComponent->RestartLogic();
		}
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		if (bIsDead)
		{
			MovementComponent->DisableMovement();
		}
		else
		{
			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		MeshComponent->SetSimulatePhysics(false);
		MeshComponent->SetRelativeLocation(InitialMeshRelativeLocation);
		MeshComponent->SetHiddenInGame(
			bIsDead ? bHideMeshOnDeath : bInitialMeshHiddenInGame,
			true);
		if (bIsDead)
		{
			MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else
		{
			MeshComponent->SetCollisionProfileName(InitialMeshCollisionProfile);
			MeshComponent->SetCollisionEnabled(InitialMeshCollisionEnabled);
		}
	}

	if (UCapsuleComponent* DroneCapsule = GetCapsuleComponent())
	{
		if (bIsDead)
		{
			if (bKeepDeathInteractionTrace)
			{
				DroneCapsule->SetCollisionProfileName(InitialCapsuleCollisionProfile);
				DroneCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				DroneCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			}
			else
			{
				DroneCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
		else
		{
			DroneCapsule->SetCollisionProfileName(InitialCapsuleCollisionProfile);
			DroneCapsule->SetCollisionEnabled(InitialCapsuleCollisionEnabled);
		}
	}
}

bool ASovDroneNPCBase::HasCommittedNativeSelfDestruct() const
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	for (TActorIterator<ASovReformationDroneSelfDestructPresentation> It(World);
		It;
		++It)
	{
		const ASovReformationDroneSelfDestructPresentation* Presentation = *It;
		if (!IsValid(Presentation))
		{
			continue;
		}

		const FSovReformationDroneSelfDestructPresentationState State =
			Presentation->GetPresentationState();
		if ((Presentation->GetOwner() == this || State.SourceDrone == this)
			&& State.Phase
				== ESovReformationDroneSelfDestructPhase::Detonated)
		{
			return true;
		}
	}

	return false;
}

bool ASovDroneNPCBase::IsLivingHostileTarget(
	const UAbilitySystemComponent* TargetAbilitySystem,
	const AActor* TargetActor) const
{
	if (!IsValid(TargetAbilitySystem)
		|| !IsValid(TargetActor)
		|| !TargetAbilitySystem->GetSet<UNarrativeAttributeSetBase>())
	{
		return false;
	}

	if (const UNarrativeAbilitySystemComponent* NarrativeASC =
		Cast<UNarrativeAbilitySystemComponent>(TargetAbilitySystem);
		IsValid(NarrativeASC) && NarrativeASC->IsDead())
	{
		return false;
	}

	const FSovGameplayTags& SovTags = FSovGameplayTags::Get();
	if (TargetAbilitySystem->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_IsDead)
		|| TargetAbilitySystem->HasMatchingGameplayTag(SovTags.State_Fatal)
		|| TargetAbilitySystem->GetNumericAttribute(
			UNarrativeAttributeSetBase::GetHealthAttribute())
			<= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const INarrativeTeamAgentInterface* SourceTeam =
		Cast<const INarrativeTeamAgentInterface>(this);
	return SourceTeam
		&& SourceTeam->GetTeamAttitudeTowards(*TargetActor)
			== ETeamAttitude::Hostile;
}

void ASovDroneNPCBase::MulticastPlayDeathExplosion_Implementation(
	const FVector_NetQuantize ExplosionLocation)
{
	if (IsValid(DeathExplosionNiagaraSystem))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			DeathExplosionNiagaraSystem,
			ExplosionLocation,
			FRotator::ZeroRotator,
			DeathExplosionScale,
			true,
			true,
			ENCPoolMethod::AutoRelease,
			true);
	}

	if (IsValid(DeathExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			DeathExplosionSound,
			ExplosionLocation);
	}
}
