// Copyright Fallen Signal Studios. All Rights Reserved.

#include "Components/SovDismembermentComponent.h"

#include "Character/NarrativeCharacterVisual.h"
#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dismemberment/SovDetachedLimbActor.h"
#include "Dismemberment/SovDismembermentProfile.h"
#include "Engine/World.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Sovereign/SovGameplayTags.h"
#include "TimerManager.h"
#include "UnrealFramework/NarrativeCharacter.h"

namespace SovDismemberment
{
	void DisablePhysicsBodiesBelowBone(
		USkeletalMeshComponent* Mesh,
		const FName RootBone)
	{
		if (!IsValid(Mesh) || RootBone.IsNone())
		{
			return;
		}

		TArray<FName> BoneNames;
		Mesh->GetBoneNames(BoneNames);
		for (const FName BoneName : BoneNames)
		{
			if (BoneName == RootBone || Mesh->BoneIsChildOf(BoneName, RootBone))
			{
				if (FBodyInstance* BodyInstance = Mesh->GetBodyInstance(BoneName))
				{
					BodyInstance->SetCollisionEnabled(
						ECollisionEnabled::NoCollision,
						true);
				}
			}
		}
	}
}

USovDismembermentComponent::USovDismembermentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	const auto AddEpicHumanRegion = [this](
		const ESovDismembermentRegion Region,
		const FName BoneToHide,
		const FName StumpAttachBone,
		const bool bHideHeadPresentation)
	{
		FSovDismembermentRegionDefinition Definition;
		Definition.Region = Region;
		Definition.HitBoneRoots.Add(BoneToHide);
		Definition.BoneToHide = BoneToHide;
		Definition.StumpAttachBone = StumpAttachBone;
		Definition.bHideHeadPresentation = bHideHeadPresentation;
		FallbackRegions.Add(MoveTemp(Definition));
	};

	AddEpicHumanRegion(
		ESovDismembermentRegion::Head,
		TEXT("head"),
		TEXT("neck_01"),
		true);
	AddEpicHumanRegion(
		ESovDismembermentRegion::LeftForearm,
		TEXT("lowerarm_l"),
		TEXT("upperarm_l"),
		false);
	AddEpicHumanRegion(
		ESovDismembermentRegion::RightForearm,
		TEXT("lowerarm_r"),
		TEXT("upperarm_r"),
		false);
	AddEpicHumanRegion(
		ESovDismembermentRegion::LeftLowerLeg,
		TEXT("calf_l"),
		TEXT("thigh_l"),
		false);
	AddEpicHumanRegion(
		ESovDismembermentRegion::RightLowerLeg,
		TEXT("calf_r"),
		TEXT("thigh_r"),
		false);
}

void USovDismembermentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		Character->OnASCInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleAbilitySystemInitialized);
		Character->CharacterVisualInitialized.AddUniqueDynamic(
			this,
			&ThisClass::HandleCharacterVisualInitialized);
		BindCharacterVisual(Character->GetCharacterVisual());
	}

	TryInitializeFromOwner();
	ScheduleVisualRefresh();
}

void USovDismembermentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VisualRefreshTimerHandle);
	}
	bVisualRefreshScheduled = false;

	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		Character->OnASCInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleAbilitySystemInitialized);
		Character->CharacterVisualInitialized.RemoveDynamic(
			this,
			&ThisClass::HandleCharacterVisualInitialized);
	}

	BindCharacterVisual(nullptr);
	InitializeWithAbilitySystem(nullptr);

	for (const TPair<ESovDismembermentRegion, TObjectPtr<AActor>>& Pair : SpawnedStumpActors)
	{
		if (IsValid(Pair.Value.Get()))
		{
			Pair.Value.Get()->Destroy();
		}
	}
	SpawnedStumpActors.Reset();

	Super::EndPlay(EndPlayReason);
}

void USovDismembermentComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovDismembermentComponent, SeveredRegionMask);
}

bool USovDismembermentComponent::IsInitialized() const
{
	return IsValid(AbilitySystemComponent.Get());
}

void USovDismembermentComponent::InitializeWithAbilitySystem(
	UNarrativeAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		return;
	}

	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->OnDamageResolvedAsTarget.RemoveDynamic(
			this,
			&ThisClass::HandleDamageResolved);
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	if (IsValid(AbilitySystemComponent))
	{
		AbilitySystemComponent->OnDamageResolvedAsTarget.AddUniqueDynamic(
			this,
			&ThisClass::HandleDamageResolved);
	}
}

void USovDismembermentComponent::TryInitializeFromOwner()
{
	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		InitializeWithAbilitySystem(Character->GetNarrativeAbilitySystemComponent());
	}
}

void USovDismembermentComponent::HandleAbilitySystemInitialized()
{
	TryInitializeFromOwner();
}

void USovDismembermentComponent::HandleCharacterVisualInitialized(
	ANarrativeCharacter* Character)
{
	if (Character == GetOwner())
	{
		BindCharacterVisual(Character->GetCharacterVisual());
		ScheduleVisualRefresh();
	}
}

void USovDismembermentComponent::BindCharacterVisual(
	ANarrativeCharacterVisual* NewCharacterVisual)
{
	if (BoundCharacterVisual == NewCharacterVisual)
	{
		return;
	}

	if (IsValid(BoundCharacterVisual))
	{
		BoundCharacterVisual->OnBaseAppearanceApplied.RemoveDynamic(
			this,
			&ThisClass::HandleBaseAppearanceApplied);
		BoundCharacterVisual->OnAppearancePartChanged.RemoveDynamic(
			this,
			&ThisClass::HandleAppearancePartChanged);
	}

	BoundCharacterVisual = NewCharacterVisual;
	if (IsValid(BoundCharacterVisual))
	{
		BoundCharacterVisual->OnBaseAppearanceApplied.AddUniqueDynamic(
			this,
			&ThisClass::HandleBaseAppearanceApplied);
		BoundCharacterVisual->OnAppearancePartChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleAppearancePartChanged);
	}
}

void USovDismembermentComponent::HandleBaseAppearanceApplied()
{
	ScheduleVisualRefresh();
}

void USovDismembermentComponent::HandleAppearancePartChanged(
	const FGameplayTag AppearanceSlot)
{
	static_cast<void>(AppearanceSlot);
	ScheduleVisualRefresh();
}

void USovDismembermentComponent::ScheduleVisualRefresh()
{
	if (bVisualRefreshScheduled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bVisualRefreshScheduled = true;
		VisualRefreshTimerHandle = World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(
				this,
				&ThisClass::HandleDeferredVisualRefresh));
	}
	else
	{
		RefreshDismembermentVisuals();
	}
}

void USovDismembermentComponent::HandleDeferredVisualRefresh()
{
	bVisualRefreshScheduled = false;
	RefreshDismembermentVisuals();
}

void USovDismembermentComponent::RefreshDismembermentVisuals()
{
	if (ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		BindCharacterVisual(Character->GetCharacterVisual());
	}

	for (const FSovDismembermentRegionDefinition& Definition : GetActiveRegionDefinitions())
	{
		if (IsRegionSevered(Definition.Region))
		{
			const FTransform SeverTransform = ResolveSeverTransform(Definition);
			ApplyRegionVisualState(Definition, SeverTransform);
		}
	}
}

void USovDismembermentComponent::HandleDamageResolved(
	const FSovDamageResult& Result)
{
	if (bDismembermentEnabled
		&& IsValid(GetOwner())
		&& GetOwner()->HasAuthority())
	{
		TrySeverFromDamageResult(Result);
	}
}

bool USovDismembermentComponent::TrySeverFromDamageResult(
	const FSovDamageResult& Result)
{
	if (Result.TargetActor.Get() != GetOwner()
		|| Result.HitZone.IsNone()
		|| Result.bGuarded
		|| Result.bPerfectDefense
		|| !DoesDamageMeetAnyRule(Result))
	{
		return false;
	}

	const FSovDismembermentRegionDefinition* Definition =
		FindRegionDefinitionForBone(Result.HitZone);
	if (!Definition || IsRegionSevered(Definition->Region))
	{
		return false;
	}

	const FTransform SeverTransform = ResolveSeverTransform(*Definition);
	FVector ImpactLocation = SeverTransform.GetLocation();
	FVector ImpactNormal = FVector::UpVector;
	FVector ImpulseDirection = GetOwner()->GetActorForwardVector();

	if (const FHitResult* Hit = Result.EffectContext.GetHitResult())
	{
		ImpactLocation = Hit->ImpactPoint;
		ImpactNormal = Hit->ImpactNormal.GetSafeNormal();
		ImpulseDirection = (Hit->TraceEnd - Hit->TraceStart).GetSafeNormal();
	}

	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = (ImpactLocation - GetOwner()->GetActorLocation()).GetSafeNormal();
	}
	if (ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector::UpVector;
	}
	if (ImpulseDirection.IsNearlyZero())
	{
		ImpulseDirection = -ImpactNormal;
	}

	const float MinimumImpulse = FMath::Max(MinimumDetachedLimbImpulse, 0.f);
	const float MaximumImpulse = FMath::Max(MaximumDetachedLimbImpulse, MinimumImpulse);
	const float ImpulseMagnitude = FMath::Clamp(
		Result.ResolvedDamage * FMath::Max(DetachedLimbImpulsePerDamage, 0.f),
		MinimumImpulse,
		MaximumImpulse);

	return CommitSever(
		*Definition,
		Result.HitZone,
		ImpactLocation,
		ImpactNormal,
		ImpulseDirection * ImpulseMagnitude);
}

bool USovDismembermentComponent::DoesDamageMeetAnyRule(
	const FSovDamageResult& Result) const
{
	const TArray<FSovDismembermentRule>& Rules = GetActiveRules();
	if (!Rules.IsEmpty())
	{
		return Rules.ContainsByPredicate(
			[this, &Result](const FSovDismembermentRule& Rule)
			{
				return DoesDamageMeetRule(Result, Rule);
			});
	}

	// Safe out-of-box policy: only lethal, unguarded Edge damage can sever.
	const FSovGameplayTags& Tags = FSovGameplayTags::Get();
	return Result.bFatal
		&& Result.DamageChannels.HasTagExact(Tags.Damage_Channel_Edge)
		&& Result.AppliedHealthDamage + KINDA_SMALL_NUMBER
			>= FMath::Max(DefaultMinimumAppliedHealthDamage, 0.f)
		&& Result.HealthOverkillDamage + KINDA_SMALL_NUMBER
			>= FMath::Max(DefaultMinimumHealthOverkillDamage, 0.f);
}

bool USovDismembermentComponent::DoesDamageMeetRule(
	const FSovDamageResult& Result,
	const FSovDismembermentRule& Rule) const
{
	if ((Rule.bRequireFatalHit && !Result.bFatal)
		|| (Rule.bRequirePoiseBreak && !Result.bPoiseBroken)
		|| Result.AppliedHealthDamage + KINDA_SMALL_NUMBER
			< FMath::Max(Rule.MinimumAppliedHealthDamage, 0.f)
		|| Result.HealthOverkillDamage + KINDA_SMALL_NUMBER
			< FMath::Max(Rule.MinimumHealthOverkillDamage, 0.f))
	{
		return false;
	}

	if (!Rule.RequiredDamageChannels.IsEmpty())
	{
		const bool bChannelsMatch = Rule.bRequireAllDamageChannels
			? Result.DamageChannels.HasAll(Rule.RequiredDamageChannels)
			: Result.DamageChannels.HasAny(Rule.RequiredDamageChannels);
		if (!bChannelsMatch)
		{
			return false;
		}
	}

	if (!Rule.RequiredAttackClassifications.IsEmpty())
	{
		const bool bClassificationsMatch = Rule.bRequireAllAttackClassifications
			? Result.AttackClassifications.HasAll(Rule.RequiredAttackClassifications)
			: Result.AttackClassifications.HasAny(Rule.RequiredAttackClassifications);
		if (!bClassificationsMatch)
		{
			return false;
		}
	}

	return true;
}

bool USovDismembermentComponent::ForceSeverRegion(
	const ESovDismembermentRegion Region,
	const FName HitBone,
	const FVector ImpactLocation,
	const FVector ImpactNormal,
	const FVector DetachedLimbImpulse)
{
	if (!bDismembermentEnabled
		|| !IsValid(GetOwner())
		|| !GetOwner()->HasAuthority()
		|| IsRegionSevered(Region))
	{
		return false;
	}

	const FSovDismembermentRegionDefinition* Definition = FindRegionDefinition(Region);
	return Definition
		? CommitSever(
			*Definition,
			HitBone,
			ImpactLocation,
			ImpactNormal,
			DetachedLimbImpulse)
		: false;
}

bool USovDismembermentComponent::CommitSever(
	const FSovDismembermentRegionDefinition& Definition,
	const FName HitBone,
	const FVector& ImpactLocation,
	const FVector& ImpactNormal,
	const FVector& DetachedLimbImpulse)
{
	if (!IsValid(GetOwner())
		|| !GetOwner()->HasAuthority()
		|| !IsValidRegion(Definition.Region)
		|| IsRegionSevered(Definition.Region))
	{
		return false;
	}

	const FTransform SeverTransform = ResolveSeverTransform(Definition);
	const bool bUseSeverLocation = ImpactLocation.ContainsNaN()
		|| ImpactLocation.IsNearlyZero();
	const FVector SafeImpactLocation = bUseSeverLocation
		? SeverTransform.GetLocation()
		: ImpactLocation;
	const FName SafeHitBone = HitBone.IsNone() ? Definition.BoneToHide : HitBone;
	FVector SafeImpactNormal = ImpactNormal.GetSafeNormal();
	if (SafeImpactNormal.IsNearlyZero())
	{
		SafeImpactNormal = FVector::UpVector;
	}

	SeveredRegionMask |= GetRegionBit(Definition.Region);
	ApplyRegionVisualState(Definition, SeverTransform);
	OnDismembermentStateChanged.Broadcast(SeveredRegionMask);
	GetOwner()->ForceNetUpdate();

	const int32 CosmeticSeed = ++CosmeticEventCounter ^ GetOwner()->GetUniqueID();
	MulticastPlaySever(
		Definition.Region,
		SafeHitBone,
		SafeImpactLocation,
		SafeImpactNormal,
		SeverTransform.GetLocation(),
		SeverTransform.Rotator(),
		DetachedLimbImpulse,
		CosmeticSeed);
	return true;
}

void USovDismembermentComponent::MulticastPlaySever_Implementation(
	const ESovDismembermentRegion Region,
	const FName HitBone,
	const FVector ImpactLocation,
	const FVector ImpactNormal,
	const FVector SeverLocation,
	const FRotator SeverRotation,
	const FVector DetachedLimbImpulse,
	const int32 CosmeticSeed)
{
	const FSovDismembermentRegionDefinition* Definition = FindRegionDefinition(Region);
	if (!Definition)
	{
		return;
	}

	const FTransform SeverTransform(SeverRotation, SeverLocation);
	ApplyRegionVisualState(*Definition, SeverTransform);
	PlaySeverCosmetics(
		*Definition,
		HitBone,
		ImpactLocation,
		ImpactNormal,
		SeverTransform,
		DetachedLimbImpulse,
		CosmeticSeed);
}

void USovDismembermentComponent::OnRep_SeveredRegionMask()
{
	OnDismembermentStateChanged.Broadcast(SeveredRegionMask);
	ScheduleVisualRefresh();
}

bool USovDismembermentComponent::IsRegionSevered(
	const ESovDismembermentRegion Region) const
{
	const int32 RegionBit = GetRegionBit(Region);
	return RegionBit != 0 && (SeveredRegionMask & RegionBit) != 0;
}

void USovDismembermentComponent::GetSeveredRegions(
	TArray<ESovDismembermentRegion>& OutRegions) const
{
	OutRegions.Reset();
	for (uint8 RegionValue = static_cast<uint8>(ESovDismembermentRegion::Head);
		RegionValue < static_cast<uint8>(ESovDismembermentRegion::MAX);
		++RegionValue)
	{
		const ESovDismembermentRegion Region =
			static_cast<ESovDismembermentRegion>(RegionValue);
		if (IsRegionSevered(Region))
		{
			OutRegions.Add(Region);
		}
	}
}

const TArray<FSovDismembermentRegionDefinition>&
USovDismembermentComponent::GetActiveRegionDefinitions() const
{
	return IsValid(DismembermentProfile) && !DismembermentProfile->Regions.IsEmpty()
		? DismembermentProfile->Regions
		: FallbackRegions;
}

const TArray<FSovDismembermentRule>&
USovDismembermentComponent::GetActiveRules() const
{
	return IsValid(DismembermentProfile) && !DismembermentProfile->SeverRules.IsEmpty()
		? DismembermentProfile->SeverRules
		: FallbackRules;
}

const FSovDismembermentRegionDefinition*
USovDismembermentComponent::FindRegionDefinition(
	const ESovDismembermentRegion Region) const
{
	return GetActiveRegionDefinitions().FindByPredicate(
		[Region](const FSovDismembermentRegionDefinition& Definition)
		{
			return Definition.Region == Region;
		});
}

const FSovDismembermentRegionDefinition*
USovDismembermentComponent::FindRegionDefinitionForBone(
	const FName HitBone) const
{
	if (HitBone.IsNone())
	{
		return nullptr;
	}

	const USkeletalMeshComponent* PrimaryMesh = ResolvePrimaryMesh();
	const auto GetBoneDistance = [PrimaryMesh, HitBone](const FName RootBone)
	{
		if (RootBone.IsNone())
		{
			return MAX_int32;
		}

		FName CurrentBone = HitBone;
		for (int32 Distance = 0; Distance < 256 && !CurrentBone.IsNone(); ++Distance)
		{
			if (CurrentBone == RootBone)
			{
				return Distance;
			}
			if (!IsValid(PrimaryMesh))
			{
				break;
			}
			CurrentBone = PrimaryMesh->GetParentBone(CurrentBone);
		}
		return MAX_int32;
	};

	const FSovDismembermentRegionDefinition* BestDefinition = nullptr;
	int32 BestDistance = MAX_int32;
	for (const FSovDismembermentRegionDefinition& Definition : GetActiveRegionDefinitions())
	{
		for (const FName RootBone : Definition.HitBoneRoots)
		{
			const int32 Distance = GetBoneDistance(RootBone);
			if (Distance < BestDistance)
			{
				BestDefinition = &Definition;
				BestDistance = Distance;
			}
		}
	}
	return BestDefinition;
}

USkeletalMeshComponent* USovDismembermentComponent::ResolvePrimaryMesh() const
{
	if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return nullptr;
}

FTransform USovDismembermentComponent::ResolveSeverTransform(
	const FSovDismembermentRegionDefinition& Definition) const
{
	if (const USkeletalMeshComponent* PrimaryMesh = ResolvePrimaryMesh())
	{
		if (!Definition.BoneToHide.IsNone()
			&& PrimaryMesh->GetBoneIndex(Definition.BoneToHide) != INDEX_NONE)
		{
			FTransform BoneTransform = PrimaryMesh->GetSocketTransform(
				Definition.BoneToHide,
				RTS_World);
			const FVector BoneScale = BoneTransform.GetScale3D();
			if (FMath::IsNearlyZero(BoneScale.X)
				|| FMath::IsNearlyZero(BoneScale.Y)
				|| FMath::IsNearlyZero(BoneScale.Z))
			{
				BoneTransform.SetScale3D(PrimaryMesh->GetComponentScale());
			}
			return BoneTransform;
		}
	}

	return IsValid(GetOwner()) ? GetOwner()->GetActorTransform() : FTransform::Identity;
}

void USovDismembermentComponent::GatherPresentationMeshes(
	TArray<USkeletalMeshComponent*>& OutMeshes) const
{
	OutMeshes.Reset();
	if (USkeletalMeshComponent* PrimaryMesh = ResolvePrimaryMesh())
	{
		OutMeshes.AddUnique(PrimaryMesh);
	}

	if (IsValid(GetOwner()))
	{
		TArray<USkeletalMeshComponent*> OwnerMeshes;
		GetOwner()->GetComponents(OwnerMeshes);
		for (USkeletalMeshComponent* Mesh : OwnerMeshes)
		{
			OutMeshes.AddUnique(Mesh);
		}
	}

	ANarrativeCharacterVisual* CharacterVisual = BoundCharacterVisual.Get();
	if (!IsValid(CharacterVisual))
	{
		if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
		{
			CharacterVisual = Character->GetCharacterVisual();
		}
	}
	if (IsValid(CharacterVisual))
	{
		TArray<USkeletalMeshComponent*> VisualMeshes;
		CharacterVisual->GetAllMeshes(VisualMeshes);
		for (USkeletalMeshComponent* Mesh : VisualMeshes)
		{
			OutMeshes.AddUnique(Mesh);
		}

		TArray<UMeshComponent*> LocalMeshes;
		CharacterVisual->GetAllLocalMeshes(LocalMeshes);
		for (UMeshComponent* LocalMesh : LocalMeshes)
		{
			if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(LocalMesh))
			{
				OutMeshes.AddUnique(SkeletalMesh);
			}
		}
	}

	OutMeshes.RemoveAll(
		[](const USkeletalMeshComponent* Mesh)
		{
			return !IsValid(Mesh);
		});
}

void USovDismembermentComponent::ApplyRegionVisualState(
	const FSovDismembermentRegionDefinition& Definition,
	const FTransform& SeverTransform)
{
	EPhysBodyOp PhysicsBodyOperation = PBO_None;
	bool bDisablePhysicsBodies = false;
	switch (Definition.PhysicsBodyOperation)
	{
	case ESovDismembermentPhysicsBodyOperation::Disable:
		bDisablePhysicsBodies = true;
		break;
	case ESovDismembermentPhysicsBodyOperation::Terminate:
		PhysicsBodyOperation = PBO_Term;
		break;
	default:
		break;
	}

	if (!Definition.BoneToHide.IsNone())
	{
		USkeletalMeshComponent* PrimaryMesh = ResolvePrimaryMesh();
		TArray<USkeletalMeshComponent*> Meshes;
		GatherPresentationMeshes(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			// Narrative's modular body and armor meshes normally follow the main
			// character mesh through Leader Pose. Hiding the bone directly on a
			// follower gives it a second, incomplete visibility transform and can
			// stretch weighted vertices toward component origin. Followers inherit
			// the collapsed bone transform from their leader, so only independently
			// posed meshes should be modified here.
			if (Mesh->LeaderPoseComponent.IsValid())
			{
				continue;
			}

			if (Mesh->GetBoneIndex(Definition.BoneToHide) != INDEX_NONE)
			{
				const bool bIsPrimaryMesh = Mesh == PrimaryMesh;
				Mesh->HideBoneByName(
					Definition.BoneToHide,
					bIsPrimaryMesh ? PhysicsBodyOperation : PBO_None);
				if (bDisablePhysicsBodies && bIsPrimaryMesh)
				{
					SovDismemberment::DisablePhysicsBodiesBelowBone(
						Mesh,
						Definition.BoneToHide);
				}
			}
		}
	}

	ANarrativeCharacterVisual* CharacterVisual = BoundCharacterVisual.Get();
	if (!IsValid(CharacterVisual))
	{
		if (const ANarrativeCharacter* Character = Cast<ANarrativeCharacter>(GetOwner()))
		{
			CharacterVisual = Character->GetCharacterVisual();
		}
	}

	if (IsValid(CharacterVisual))
	{
		for (const FGameplayTag& Slot : Definition.PresentationSlotsToHide)
		{
			if (UMeshComponent* Mesh = CharacterVisual->GetSkeletalMeshComponent(Slot))
			{
				Mesh->SetVisibility(false, true);
			}
			if (UMeshComponent* Mesh = CharacterVisual->GetStaticMeshComponent(Slot))
			{
				Mesh->SetVisibility(false, true);
			}
		}

		if (Definition.bHideHeadPresentation)
		{
			TArray<UMeshComponent*> HeadMeshes;
			CharacterVisual->GetHeadMeshes(HeadMeshes);
			for (UMeshComponent* HeadMesh : HeadMeshes)
			{
				if (IsValid(HeadMesh))
				{
					HeadMesh->SetVisibility(false, true);
				}
			}
		}
	}

	EnsureStumpActor(Definition, SeverTransform);
}

void USovDismembermentComponent::EnsureStumpActor(
	const FSovDismembermentRegionDefinition& Definition,
	const FTransform& SeverTransform)
{
	if (!Definition.StumpActorClass || !IsValid(GetWorld())
		|| GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (const TObjectPtr<AActor>* Existing = SpawnedStumpActors.Find(Definition.Region))
	{
		if (IsValid(Existing->Get()))
		{
			return;
		}
	}

	const FTransform SpawnTransform = Definition.StumpSpawnOffset * SeverTransform;
	AActor* StumpActor = GetWorld()->SpawnActorDeferred<AActor>(
		Definition.StumpActorClass,
		SpawnTransform,
		GetOwner(),
		Cast<APawn>(GetOwner()),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(StumpActor))
	{
		return;
	}

	StumpActor->SetReplicates(false);
	UGameplayStatics::FinishSpawningActor(StumpActor, SpawnTransform);
	StumpActor->SetReplicates(false);
	StumpActor->SetActorEnableCollision(false);

	if (USkeletalMeshComponent* PrimaryMesh = ResolvePrimaryMesh())
	{
		const bool bHasAttachPoint = !Definition.StumpAttachBone.IsNone()
			&& (PrimaryMesh->GetBoneIndex(Definition.StumpAttachBone) != INDEX_NONE
				|| PrimaryMesh->DoesSocketExist(Definition.StumpAttachBone));
		StumpActor->AttachToComponent(
			PrimaryMesh,
			FAttachmentTransformRules::KeepWorldTransform,
			bHasAttachPoint ? Definition.StumpAttachBone : NAME_None);
	}

	SpawnedStumpActors.Add(Definition.Region, StumpActor);
}

void USovDismembermentComponent::PlaySeverCosmetics(
	const FSovDismembermentRegionDefinition& Definition,
	const FName HitBone,
	const FVector& ImpactLocation,
	const FVector& ImpactNormal,
	const FTransform& SeverTransform,
	const FVector& DetachedLimbImpulse,
	const int32 CosmeticSeed)
{
	UWorld* World = GetWorld();
	if (IsValid(World) && World->GetNetMode() != NM_DedicatedServer)
	{
		FVector SafeNormal = ImpactNormal.GetSafeNormal();
		if (SafeNormal.IsNearlyZero())
		{
			SafeNormal = FVector::UpVector;
		}

		if (IsValid(Definition.SeverSystem))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				World,
				Definition.SeverSystem,
				ImpactLocation,
				SafeNormal.Rotation(),
				FVector::OneVector,
				true,
				true,
				ENCPoolMethod::AutoRelease,
				true);
		}

		if (IsValid(Definition.BloodDecalMaterial))
		{
			FRandomStream RandomStream(CosmeticSeed);
			FRotator DecalRotation = SafeNormal.Rotation();
			DecalRotation.Roll = RandomStream.FRandRange(-180.f, 180.f);
			if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(
				World,
				Definition.BloodDecalMaterial,
				Definition.BloodDecalSize,
				ImpactLocation + (SafeNormal * 1.5f),
				DecalRotation,
				Definition.BloodDecalLifeSeconds))
			{
				const float FadeSeconds = FMath::Clamp(
					Definition.BloodDecalFadeSeconds,
					0.f,
					Definition.BloodDecalLifeSeconds);
				if (FadeSeconds > KINDA_SMALL_NUMBER)
				{
					Decal->SetFadeOut(
						FMath::Max(Definition.BloodDecalLifeSeconds - FadeSeconds, 0.f),
						FadeSeconds,
						false);
				}
			}
		}

		if (Definition.DetachedLimbClass)
		{
			const FTransform LimbTransform =
				Definition.DetachedLimbSpawnOffset * SeverTransform;
			ASovDetachedLimbActor* DetachedLimb =
				World->SpawnActorDeferred<ASovDetachedLimbActor>(
					Definition.DetachedLimbClass,
					LimbTransform,
					GetOwner(),
					Cast<APawn>(GetOwner()),
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (IsValid(DetachedLimb))
			{
				DetachedLimb->SetReplicates(false);
				DetachedLimb->InitializeDetachedLimb(
					DetachedLimbImpulse
						* FMath::Max(Definition.DetachedLimbImpulseMultiplier, 0.f),
					GetOwner());
				UGameplayStatics::FinishSpawningActor(DetachedLimb, LimbTransform);
				DetachedLimb->SetReplicates(false);
			}
		}
	}

	OnLimbSevered.Broadcast(Definition.Region, HitBone, SeverTransform.GetLocation());
}

int32 USovDismembermentComponent::GetRegionBit(
	const ESovDismembermentRegion Region)
{
	return IsValidRegion(Region)
		? (1 << static_cast<uint8>(Region))
		: 0;
}

bool USovDismembermentComponent::IsValidRegion(
	const ESovDismembermentRegion Region)
{
	const uint8 RegionValue = static_cast<uint8>(Region);
	return RegionValue > static_cast<uint8>(ESovDismembermentRegion::None)
		&& RegionValue < static_cast<uint8>(ESovDismembermentRegion::MAX)
		&& RegionValue < 31;
}
