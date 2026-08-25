// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SovDismembermentTypes.generated.h"

class AActor;
class ASovDetachedLimbActor;
class UMaterialInterface;
class UNiagaraSystem;

/** Stable replicated regions. Values are bit positions in the severed-region mask. */
UENUM(BlueprintType)
enum class ESovDismembermentRegion : uint8
{
	None = 0 UMETA(Hidden),
	Head,
	LeftUpperArm,
	RightUpperArm,
	LeftForearm,
	RightForearm,
	LeftHand,
	RightHand,
	LeftUpperLeg,
	RightUpperLeg,
	LeftLowerLeg,
	RightLowerLeg,
	LeftFoot,
	RightFoot,
	CustomA,
	CustomB,
	MAX UMETA(Hidden)
};

/** What HideBoneByName should do with physics bodies below the hidden bone. */
UENUM(BlueprintType)
enum class ESovDismembermentPhysicsBodyOperation : uint8
{
	None,
	Disable UMETA(DisplayName = "Disable Collision"),
	Terminate UMETA(DisplayName = "Terminate Permanently")
};

/** One independently authorable way a resolved hit may sever a region. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovDismembermentRule
{
	GENERATED_BODY()

	/** Empty means no channel restriction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule", meta = (Categories = "Sov.Damage.Channel"))
	FGameplayTagContainer RequiredDamageChannels;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule")
	bool bRequireAllDamageChannels = false;

	/** Optional Heavy, Unblockable, counter, or future attack classification requirements. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule")
	FGameplayTagContainer RequiredAttackClassifications;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule")
	bool bRequireAllAttackClassifications = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule")
	bool bRequireFatalHit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule")
	bool bRequirePoiseBreak = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule", meta = (ClampMin = "0.0"))
	float MinimumAppliedHealthDamage = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dismemberment Rule", meta = (ClampMin = "0.0"))
	float MinimumHealthOverkillDamage = 0.f;
};

/** Skeleton mapping and cosmetic payload for one severable body region. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovDismembermentRegionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	ESovDismembermentRegion Region = ESovDismembermentRegion::None;

	/** A hit on one of these bones or any descendant resolves to this region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	TArray<FName> HitBoneRoots;

	/** Hiding this bone removes its complete child branch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FName BoneToHide = NAME_None;

	/** Parent bone or authored socket that should carry the persistent stump actor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	FName StumpAttachBone = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Region")
	ESovDismembermentPhysicsBodyOperation PhysicsBodyOperation =
		ESovDismembermentPhysicsBodyOperation::Terminate;

	/** Additional Narrative appearance slots to hide, useful for rigid armor pieces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "Narrative.Equipment.Slot"))
	TArray<FGameplayTag> PresentationSlotsToHide;

	/** Hides Narrative face, helmet, facial hair, and groom components. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	bool bHideHeadPresentation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Detached Limb")
	TSubclassOf<ASovDetachedLimbActor> DetachedLimbClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Detached Limb")
	FTransform DetachedLimbSpawnOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Detached Limb", meta = (ClampMin = "0.0"))
	float DetachedLimbImpulseMultiplier = 1.f;

	/** Optional persistent actor containing authored cap geometry and wound materials. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump")
	TSubclassOf<AActor> StumpActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump")
	FTransform StumpSpawnOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	TObjectPtr<UNiagaraSystem> SeverSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	TObjectPtr<UMaterialInterface> BloodDecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	FVector BloodDecalSize = FVector(4.f, 24.f, 24.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore", meta = (ClampMin = "0.0"))
	float BloodDecalLifeSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore", meta = (ClampMin = "0.0"))
	float BloodDecalFadeSeconds = 2.f;
};
