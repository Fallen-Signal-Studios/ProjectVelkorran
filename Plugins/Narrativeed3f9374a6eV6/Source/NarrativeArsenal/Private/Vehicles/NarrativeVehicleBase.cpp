// Copyright Narrative Tools 2025.


#include "Vehicles/NarrativeVehicleBase.h"

#include "ArsenalStatics.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "NavAreas/NavArea_Obstacle.h"
#include "NavModifierComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/AbilityConfiguration.h"
#include "Engine/CollisionProfile.h"
#include "NarrativeLogChannels.h"
#include "SaveSystemStatics.h"
#include "AbilitySystemBlueprintLibrary.h"


FName ANarrativeVehicleBase::VehicleMeshComponentName(TEXT("VehicleMesh"));

// Sets default values
ANarrativeVehicleBase::ANarrativeVehicleBase(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(VehicleMeshComponentName);
	Mesh->SetCollisionProfileName(UCollisionProfile::Vehicle_ProfileName);
	Mesh->BodyInstance.bSimulatePhysics = false;
	Mesh->BodyInstance.bNotifyRigidBodyCollision = true;
	Mesh->BodyInstance.bUseCCD = true;
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCollisionResponseToChannel(TraceChannel_NarrativeInteraction, ECR_Block);
	RootComponent = Mesh;

	ImpactMesh = CreateDefaultSubobject<USkeletalMeshComponent>("ImpactMesh");
	ImpactMesh->SetVisibility(false);
	ImpactMesh->SetGenerateOverlapEvents(true);
	//Forward and backwards should scale up a bit, sideways less so. 
	ImpactMesh->SetEnableAnimation(false);
	ImpactMesh->SetRelativeScale3D(FVector(1.1f, 1.02f, 1.1f));
	ImpactMesh->SetSkeletalMesh(Mesh->GetSkeletalMeshAsset());
	ImpactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); //Gets turned on when someone enters car. 
	ImpactMesh->SetCollisionObjectType(ECC_WorldDynamic);
	ImpactMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ImpactMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ImpactMesh->SetupAttachment(Mesh);
	
	VehicleNavModifier = CreateDefaultSubobject<UNavModifierComponent>("VehicleNavModifier");
	VehicleNavModifier->FailsafeExtent = FVector(250.f, 100.f, 100.f);
	VehicleNavModifier->AreaClass = UNavArea_Obstacle::StaticClass();
	
	// Create ability system component, and set it to be explicitly replicated - TODO need to lazy load ASC possibly? 
	AbilitySystemComponent = CreateDefaultSubobject<UNarrativeAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed mode means we only are replicated the GEs to ourself, not the GEs to simulated proxies. If another GDPlayerState (Hero) receives a GE,
	// we won't be told about it by the Server. Attributes, GameplayTags, and GameplayCues will still replicate to us.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create the attribute set, this replicates by default
	// Adding it as a subobject of the owning actor of an AbilitySystemComponent
	// automatically registers the AttributeSet with the AbilitySystemComponent
	AttributeSetBase = CreateDefaultSubobject<UNarrativeAttributeSetBase>(TEXT("AttributeSetBase"));

	/*We don't want vehicles to be spatially loaded as driving them off will cause them to unload.
	Instead, we would want a vehicle spawner that spatially loads and unloads vehicles, similar to NPCSpawners. */
#if WITH_EDITOR
	SetIsSpatiallyLoaded(false);
#endif 

	VehicleImpactSelfDamage = FVector2D(200.f, 600.f);
	VehicleImpactCharacterDamage = FVector2D(200.f, 600.f);

	//Make vehicle GUID none by default - vehicles have to set this and opt-in to being saved, we won't save vehicles by default. 
	VehicleSaveGUID = FGuid();

	//USaveSystemStatics::CreateSaveGuid(VehicleSaveGUID);
}

void ANarrativeVehicleBase::InitializeVehicleASC()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
		InitializeAttributes();
		AddStartupEffects();
		//AddDefaultAbilities();
	}
}

void ANarrativeVehicleBase::AddDefaultAbilities()
{

}

void ANarrativeVehicleBase::InitializeAttributes()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (AbilityConfiguration)
	{
		if (!AbilityConfiguration->DefaultAttributes)
		{
			UE_LOG(LogNarrativeVehicle, Error, TEXT("%s() Missing DefaultAttributes for %s. Please fill in the character's definition."), *FString(__FUNCTION__), *GetName());
			return;
		}

		// Can run on Server and Client
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		const int32 VehicleLevel = 1;

		FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(AbilityConfiguration->DefaultAttributes, GetVehicleLevel(), EffectContext);
		if (NewHandle.IsValid())
		{
			FActiveGameplayEffectHandle ActiveGEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), AbilitySystemComponent.Get());
		}

		AbilitySystemComponent->OnDeathStateChanged.AddUniqueDynamic(this, &ANarrativeVehicleBase::HandleDeath);
	}

}

void ANarrativeVehicleBase::AddStartupEffects()
{
	if (GetLocalRole() != ROLE_Authority || !AbilitySystemComponent || AbilitySystemComponent->bStartupEffectsApplied)
	{
		return;
	}

	if (AbilityConfiguration)
	{
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		for (TSubclassOf<UGameplayEffect> GameplayEffect : AbilityConfiguration->StartupEffects)
		{
			FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(GameplayEffect, GetVehicleLevel(), EffectContext);
			if (NewHandle.IsValid())
			{
				FActiveGameplayEffectHandle ActiveGEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), AbilitySystemComponent.Get());
			}
		}

		AbilitySystemComponent->bStartupEffectsApplied = true;
	}
}

void ANarrativeVehicleBase::HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	if (bIsDead)
	{
		TArray<AActor*> AttachedActors;
		GetAttachedActors(AttachedActors, true);

		for (auto& AttachedActor : AttachedActors)
		{
			if (ANarrativeCharacter* NChar = Cast<ANarrativeCharacter>(AttachedActor))
			{
				if (UNarrativeAbilitySystemComponent* NASC = NChar->GetNarrativeAbilitySystemComponent())
				{
					NASC->Instakill();
				}
			}
		}
	}
}

void ANarrativeVehicleBase::OnVehicleMeshHit_Implementation(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor && OtherActor->GetRootComponent()->GetMobility() == EComponentMobility::Static)
	{
		//For cars, ignore the Z velocity as hitting the ground shouldn't damage us 
		const float VelSpeed = GetVelocity().Size2D();

		if (VehicleImpactObjectDamageCurve && AbilitySystemComponent)
		{
			const float DesiredDamage = VehicleImpactObjectDamageCurve->GetFloatValue(NormalImpulse.Length());//FMath::GetMappedRangeValueClamped(VehicleImpactSelfDamage, FVector2D(GetMaxHealth() / 5.f, GetMaxHealth()), VelSpeed);

			if (DesiredDamage > 0.f)
			{
				AbilitySystemComponent->DealDamage(DesiredDamage);
			}
		}
	}
}

void ANarrativeVehicleBase::OnCollisionMeshOverlap_Implementation(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//Characters, props, etc all implement this interface to respond to vehicle impact meshes overlapping 
	if (OtherActor->Implements<UNarrativeImpactInterface>())
	{
		INarrativeImpactInterface::Execute_HandleVehicleImpact(OtherActor, this, OverlappedComponent, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	}
}

void ANarrativeVehicleBase::SetVehicleSaveGuid(const FGuid& NewGUID)
{
	VehicleSaveGUID = NewGUID;
}

class ANarrativeCharacter* ANarrativeVehicleBase::GetNarrativeCharacter() const
{
	if (INarrativeCharacterOwner* CharInterface = Cast<INarrativeCharacterOwner>(GetController()))
	{
		return CharInterface->GetNarrativeCharacter();
	}

	return nullptr; 
}

void ANarrativeVehicleBase::BeginPlay()
{
	Super::BeginPlay();

	//For some reason the nav modifier doesn't automatically register on begin play 
	if (VehicleNavModifier)
	{
		VehicleNavModifier->SetNavigationRelevancy(false);
		VehicleNavModifier->SetNavigationRelevancy(true);
	}
	
	if (ImpactMesh)
	{
		ImpactMesh->OnComponentBeginOverlap.AddUniqueDynamic(this, &ANarrativeVehicleBase::OnCollisionMeshOverlap);
	}

	if (Mesh)
	{
		Mesh->OnComponentHit.AddUniqueDynamic(this, &ANarrativeVehicleBase::OnVehicleMeshHit);
	}

	if (VehicleRandomSeed < 0)
	{
		SetRandomSeed(FMath::Rand32());
	}
	
	InitializeVehicleASC();
}

void ANarrativeVehicleBase::Destroyed()
{
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);

	for (auto& Child : ChildActors)
	{
		if (Child)
		{
			Child->Destroy();
		}
	}

	Super::Destroyed();
}

void ANarrativeVehicleBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	//Temp optimization - technically we want this being used when main mesh physics are woken, but this will catch 99% of cases. 
	if (ImpactMesh)
	{
		ImpactMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
}

void ANarrativeVehicleBase::UnPossessed()
{
	Super::UnPossessed();


	if (ImpactMesh)
	{
		ImpactMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

class UAbilitySystemComponent* ANarrativeVehicleBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ANarrativeVehicleBase::HandleVehicleImpact_Implementation(class ANarrativeVehicleBase* Vehicle, UPrimitiveComponent* OverlappedComponent, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	//Commented for now as was oversensitive and needs float curve setup instead of mapped range 
	////If we get hit by another vehicle, we should take damage
	//if (Vehicle)
	//{
	//	//Commtne
	//	//For cars, ignore the Z velocity as hitting the ground shouldn't damage us 
	//	const float VelSpeed = Vehicle->GetVelocity().Size2D();
	//	if (AbilitySystemComponent && VelSpeed > 300.f)
	//	{
	//		const float DesiredDamage = FMath::GetMappedRangeValueClamped(VehicleImpactSelfDamage, FVector2D(GetMaxHealth() / 5.f, GetMaxHealth()), VelSpeed);
	//		AbilitySystemComponent->DealDamage(DesiredDamage);
	//	}
	//}
}

void ANarrativeVehicleBase::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->GetOwnedGameplayTags(TagContainer);
	}
}

bool ANarrativeVehicleBase::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasMatchingGameplayTag(TagToCheck);
	}

	return false;
}

bool ANarrativeVehicleBase::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAllMatchingGameplayTags(TagContainer);
	}

	return false;
}

bool ANarrativeVehicleBase::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (const UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		return ASC->HasAnyMatchingGameplayTags(TagContainer);
	}

	return false;
}

ETeamAttitude::Type ANarrativeVehicleBase::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(GetController()))
	{
		return TeamAgent->GetTeamAttitudeTowards(Other);
	}

	return ETeamAttitude::Neutral;
}

FGameplayTagContainer ANarrativeVehicleBase::GetFactions() const
{
	if (INarrativeTeamAgentInterface* TeamAgent = Cast<INarrativeTeamAgentInterface>(GetController()))
	{
		return TeamAgent->GetFactions();
	}

	return FGameplayTagContainer();
}

FGuid ANarrativeVehicleBase::GetActorGUID_Implementation() const
{
	//Mass controlled vehicles should not save to disk
	if (UArsenalStatics::IsControlledByMass(this))
	{
		return FGuid();
	}

	//Whether player or NPC controlled, controlled vehicles shouldn't save to disk (we cannot save whilst driving )
	if (GetController())
	{
		return FGuid();
	}


	return VehicleSaveGUID;

}

float ANarrativeVehicleBase::GetHealth() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetHealth();
	}

	return 0.0f;
}

float ANarrativeVehicleBase::GetMaxHealth() const
{
	if (AttributeSetBase)
	{
		return AttributeSetBase->GetMaxHealth();
	}

	return 0.0f;
}

int32 ANarrativeVehicleBase::GetVehicleLevel() const
{
	return 1;
}

void ANarrativeVehicleBase::SetRandomSeed(const int32 NewSeed)
{
	VehicleRandomSeed = NewSeed;
	OnSeedSet.Broadcast(VehicleRandomSeed);
}

void ANarrativeVehicleBase::SetManagedByMass_Implementation(bool bManagedByMass)
{
	UChaosWheeledVehicleMovementComponent* VehicleComponent = FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
	UPrimitiveComponent* PrimitiveRoot = Cast<UPrimitiveComponent>(GetRootComponent());
	
	if (bManagedByMass)
	{
		VehicleComponent->SetHandbrakeInput(false);
		VehicleComponent->SetRequiresControllerForInputs(false);
		VehicleComponent->bReverseAsBrake = false; // If this is true, cars would reverse at traffic lights and when near vehicles
		PrimitiveRoot->SetSimulatePhysics(true);
	}
	else
	{
		// We dont want to disable physics/set handbreak as that may break some things when transitioning
		VehicleComponent->bReverseAsBrake = true;
	}
}

void ANarrativeVehicleBase::DealVehicleDamage(class UAbilitySystemComponent* DamageASC, const float DamageAmount, const FHitResult& Hit)
{
	if (Hit.IsValidBlockingHit())
	{

		if (ANarrativeCharacter* Driver = GetNarrativeCharacter())
		{
			//Route damage through the drivers NASC, so damage numbers/kill enemy tasks etc are processed correctly. 
			if (UNarrativeAbilitySystemComponent* DriverNASC = Driver->GetNarrativeAbilitySystemComponent())
			{
				FGameplayEffectSpecHandle Handle = DriverNASC->MakeOutgoingSpec(VehicleDamageEffect, 1.f, FGameplayEffectContextHandle());
				FGameplayAbilityTargetDataHandle TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Hit);

				Handle.Data->SetSetByCallerMagnitude(FNarrativeGameplayTags::Get().SetByCaller_Damage, DamageAmount);

				DriverNASC->ApplyGameplayEffectSpecToTargetData(Handle, TargetData);

				//Uncomment if you want to route damage through vehicles ASC rather than drivers for some reason 
				//AbilitySystemComponent->ApplyGameplayEffectSpecToTargetData(Handle, TargetData);
			}
		}

	}
}

