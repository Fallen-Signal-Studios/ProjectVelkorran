// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovCombatSustainDropComponent.h"

#include "ArsenalStatics.h"
#include "Combat/Pickups/SovAmmoCombatSustainPickup.h"
#include "Combat/Pickups/SovEchoCombatSustainPickup.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Items/AmmoItem.h"
#include "UnrealFramework/NarrativeCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSovCombatSustainDrops, Log, All);

USovCombatSustainDropComponent::USovCombatSustainDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USovCombatSustainDropComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		Character->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleAbilitySystemInitialized);
	}

	TryInitializeFromOwner();
}

void USovCombatSustainDropComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		Character->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleAbilitySystemInitialized);
	}

	InitializeWithAbilitySystem(nullptr);
	Super::EndPlay(EndPlayReason);
}

void USovCombatSustainDropComponent::InitializeWithAbilitySystem(
	UNarrativeAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		return;
	}

	if (IsValid(AbilitySystemComponent.Get()))
	{
		AbilitySystemComponent->OnDamageResolvedAsTarget.RemoveDynamic(
			this,
			&ThisClass::HandleDamageResolved);
		AbilitySystemComponent->OnDeathStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	bDropsSpawnedForCurrentDeath = IsValid(AbilitySystemComponent.Get())
		&& AbilitySystemComponent->IsDead();

	if (IsValid(AbilitySystemComponent.Get()))
	{
		AbilitySystemComponent->OnDamageResolvedAsTarget.AddUniqueDynamic(
			this,
			&ThisClass::HandleDamageResolved);
		AbilitySystemComponent->OnDeathStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleDeathStateChanged);
	}
}

bool USovCombatSustainDropComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent.Get());
}

void USovCombatSustainDropComponent::HandleAbilitySystemInitialized()
{
	TryInitializeFromOwner();
}

void USovCombatSustainDropComponent::TryInitializeFromOwner()
{
	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		InitializeWithAbilitySystem(
			Character->GetNarrativeAbilitySystemComponent());
	}
}

void USovCombatSustainDropComponent::HandleDamageResolved(
	const FSovDamageResult& Result)
{
	if (!IsEligibleFatalDamage(Result))
	{
		return;
	}

	// Claim the death before spawning either pickup. A missing or invalid class
	// must not allow a repeated fatal notification to duplicate the other drop.
	bDropsSpawnedForCurrentDeath = true;
	SpawnConfiguredDrops();
}

void USovCombatSustainDropComponent::HandleDeathStateChanged(
	AActor* KilledActor,
	UNarrativeAbilitySystemComponent* KilledActorASC,
	const bool bIsDead)
{
	if (KilledActor != GetOwner()
		&& KilledActorASC != AbilitySystemComponent.Get())
	{
		return;
	}

	if (!bIsDead)
	{
		// Narrative NPCs can be revived or recycled by encounter logic. The next
		// authoritative fatal transaction is a new reward opportunity.
		bDropsSpawnedForCurrentDeath = false;
	}
}

bool USovCombatSustainDropComponent::IsEligiblePlayerSource(
	const AActor* SourceActor) const
{
	if (!IsValid(SourceActor))
	{
		return false;
	}

	if (const APawn* SourcePawn = Cast<APawn>(SourceActor))
	{
		return SourcePawn->IsPlayerControlled();
	}

	if (const AController* SourceController = Cast<AController>(SourceActor))
	{
		return SourceController->IsPlayerController();
	}

	if (const APawn* InstigatorPawn = SourceActor->GetInstigator())
	{
		if (InstigatorPawn->IsPlayerControlled())
		{
			return true;
		}
	}

	const AActor* CurrentOwner = SourceActor->GetOwner();
	for (int32 OwnerDepth = 0;
		IsValid(CurrentOwner) && OwnerDepth < 4;
		++OwnerDepth, CurrentOwner = CurrentOwner->GetOwner())
	{
		if (const APawn* OwnerPawn = Cast<APawn>(CurrentOwner))
		{
			return OwnerPawn->IsPlayerControlled();
		}
		if (const AController* OwnerController = Cast<AController>(CurrentOwner))
		{
			return OwnerController->IsPlayerController();
		}
	}

	return false;
}

bool USovCombatSustainDropComponent::IsEligibleFatalDamage(
	const FSovDamageResult& Result) const
{
	const AActor* Owner = GetOwner();
	const AActor* SourceActor = Result.SourceActor.Get();
	return bCombatSustainDropsEnabled
		&& !bDropsSpawnedForCurrentDeath
		&& IsValid(Owner)
		&& Owner->HasAuthority()
		&& Result.TargetActor.Get() == Owner
		&& Result.bFatal
		&& IsEligiblePlayerSource(SourceActor)
		&& UArsenalStatics::GetAttitude(SourceActor, Owner)
			== ETeamAttitude::Hostile;
}

void USovCombatSustainDropComponent::SpawnConfiguredDrops()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!IsValid(World) || !IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	if (bDropAmmo
		&& AmmoPickupClass
		&& AmmoItemClass
		&& AmmoAmount > 0)
	{
		const FTransform SpawnTransform = MakePickupSpawnTransform(
			AmmoSpawnOffset);
		ASovAmmoCombatSustainPickup* AmmoPickup =
			World->SpawnActorDeferred<ASovAmmoCombatSustainPickup>(
				AmmoPickupClass,
				SpawnTransform,
				Owner, // encounter cleanup can attribute this transient reward to its source
				nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (IsValid(AmmoPickup))
		{
			AmmoPickup->InitializeAmmo(AmmoItemClass, AmmoAmount);
			AmmoPickup->FinishSpawning(SpawnTransform);
		}
		else
		{
			UE_LOG(
				LogSovCombatSustainDrops,
				Warning,
				TEXT("%s failed to spawn configured ammo sustain pickup %s."),
				*GetNameSafe(Owner),
				*GetNameSafe(AmmoPickupClass.Get()));
		}
	}

	if (bDropEcho
		&& EchoPickupClass
		&& EchoAmount > KINDA_SMALL_NUMBER)
	{
		const FTransform SpawnTransform = MakePickupSpawnTransform(
			EchoSpawnOffset);
		ASovEchoCombatSustainPickup* EchoPickup =
			World->SpawnActorDeferred<ASovEchoCombatSustainPickup>(
				EchoPickupClass,
				SpawnTransform,
				Owner, // encounter cleanup can attribute this transient reward to its source
				nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (IsValid(EchoPickup))
		{
			EchoPickup->InitializeEcho(EchoAmount);
			EchoPickup->FinishSpawning(SpawnTransform);
		}
		else
		{
			UE_LOG(
				LogSovCombatSustainDrops,
				Warning,
				TEXT("%s failed to spawn configured Echo sustain pickup %s."),
				*GetNameSafe(Owner),
				*GetNameSafe(EchoPickupClass.Get()));
		}
	}
}

FTransform USovCombatSustainDropComponent::MakePickupSpawnTransform(
	const FVector& LocalOffset) const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FTransform::Identity;
	}

	const FVector WorldLocation = Owner->GetActorLocation()
		+ Owner->GetActorQuat().RotateVector(LocalOffset);
	return FTransform(Owner->GetActorQuat(), WorldLocation);
}
