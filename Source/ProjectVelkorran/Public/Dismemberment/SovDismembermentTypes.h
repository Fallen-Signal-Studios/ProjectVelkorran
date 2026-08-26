// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SovDismembermentTypes.generated.h"

class AActor;
class ASovDetachedLimbActor;
class UMaterialInterface;
class UNiagaraSystem;

/** One stateful Niagara effect attached to a severed region's surviving stump bone. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovDismembermentStumpNiagaraSlot
{
	GENERATED_BODY()

	/** Editor-facing label such as BloodSpray, Embers, or Smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump Niagara")
	FName SlotName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump Niagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem = nullptr;

	/** Empty uses the region's Stump Attach Bone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump Niagara")
	FName AttachBoneOverride = NAME_None;

	/** Applied at the sever transform before the effect is attached to the surviving bone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump Niagara")
	FTransform SpawnOffset = FTransform::Identity;

	/** Enable for finite systems. Leave disabled for persistent looping stump effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump Niagara")
	bool bAutoDestroy = false;
};

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

	/** Stateful effects reconstructed from the sever mask and attached to the stump. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stump", meta = (TitleProperty = "SlotName"))
	TArray<FSovDismembermentStumpNiagaraSlot> StumpNiagaraSlots;

	/** Transient effect played once when the sever first occurs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	TObjectPtr<UNiagaraSystem> SeverSystem = nullptr;

	/** Keeps the Niagara emitter origin on the surviving stump during animation and ragdoll. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	bool bAttachSeverSystemToStump = true;

	/** Applied to the sever transform when attached, or the impact transform when world-space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	FTransform SeverSystemSpawnOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	TObjectPtr<UMaterialInterface> BloodDecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore")
	FVector BloodDecalSize = FVector(4.f, 24.f, 24.f);

	/** Radius used to find nearby world geometry for a blood decal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore", meta = (ClampMin = "0.0", Units = "cm"))
	float BloodDecalSurfaceSearchDistance = 250.f;

	/** Prevents z-fighting after the decal is placed on the resolved surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore", meta = (ClampMin = "0.0", Units = "cm"))
	float BloodDecalSurfaceOffset = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore", meta = (ClampMin = "0.0"))
	float BloodDecalLifeSeconds = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gore", meta = (ClampMin = "0.0"))
	float BloodDecalFadeSeconds = 2.f;
};
