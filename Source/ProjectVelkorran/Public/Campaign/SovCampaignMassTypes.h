// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "MassEntityTypes.h"
#include "SovCampaignMassTypes.generated.h"
class USkeletalMesh;
class UStaticMesh;
class UMaterialInterface;
class UGameplayEffect;
class UGameplayAbility;
class UAnimSequence;

UENUM(BlueprintType)
enum class ESovCampaignRepresentationTier : uint8 { Actor, Mass, Presentation };

/** Authored locomotion assets for each captured skeletal component of a Tier C participant. */
USTRUCT(BlueprintType)
struct FSovCampaignMassAnimationProfile
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, TSoftObjectPtr<UAnimSequence>> ComponentAnimations;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="1200")) float ReferenceSpeed = 300.f;
};

/** Transient strong references belong to the director, including while no visual proxy exists. */
USTRUCT()
struct FSovCampaignMassAssetResidency
{
	GENERATED_BODY()
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> Assets;
};

/** Exact mesh-space pose, including modular appearance and visible equipment. No live actor references. */
USTRUCT()
struct FSovCampaignMassMesh
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(SaveGame) TSoftObjectPtr<UStaticMesh> StaticMesh;
	UPROPERTY(SaveGame) TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
	UPROPERTY(SaveGame) FTransform RelativeTransform;
	UPROPERTY(SaveGame) TArray<FName> Bones;
	UPROPERTY(SaveGame) TArray<FTransform> ComponentSpacePose;
	UPROPERTY(SaveGame) TArray<FTransform> LocalPose;
	UPROPERTY(SaveGame) FName ComponentName;
	UPROPERTY(SaveGame) bool bRequiresSnapshotBlend = false;
	UPROPERTY(SaveGame) TArray<FName> HiddenBones;
	UPROPERTY(SaveGame) TSoftObjectPtr<UAnimSequence> RouteAnimation;
	UPROPERTY(SaveGame) float AnimationReferenceSpeed = 300.f;
	UPROPERTY(SaveGame) float AnimationTime = 0.f;
};

USTRUCT()
struct FSovCampaignMassEffect
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TSubclassOf<UGameplayEffect> Class;
	UPROPERTY(SaveGame) float Level = 1.f;
	UPROPERTY(SaveGame) int32 Stacks = 1;
	UPROPERTY(SaveGame) TMap<FGameplayTag, float> TagMagnitudes;
	UPROPERTY(SaveGame) TMap<FName, float> NameMagnitudes;
	UPROPERTY(SaveGame) FGameplayTagContainer DynamicAssetTags;
	UPROPERTY(SaveGame) FGameplayTagContainer DynamicGrantedTags;
	UPROPERTY(SaveGame) FName OwnerComponent;
	UPROPERTY(SaveGame) FGuid OwnerItem;
	UPROPERTY(SaveGame) TArray<float> ModifierMagnitudes;
};

USTRUCT()
struct FSovCampaignMassAbility
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) TSubclassOf<UGameplayAbility> Class;
	UPROPERTY(SaveGame) int32 Level = 1;
	UPROPERTY(SaveGame) int32 InputID = INDEX_NONE;
	UPROPERTY(SaveGame) FGameplayTagContainer DynamicTags;
	UPROPERTY(SaveGame) FGuid OwnerItem;
};

/** Additional state on the director's existing NPC record, not a second checkpoint owner. */
USTRUCT()
struct FSovCampaignMassState
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FGameplayTagContainer Factions;
	UPROPERTY(SaveGame) TArray<FSovCampaignMassMesh> Meshes;
	UPROPERTY(SaveGame) TArray<FSovCampaignMassEffect> Effects;
	UPROPERTY(SaveGame) TArray<FSovCampaignMassAbility> Abilities;
	/** Full numeric bases retain non-resource attributes; resource currents remain explicit in the NPC record. */
	UPROPERTY(SaveGame) TMap<FString, float> AttributeBases;
	UPROPERTY(SaveGame) FGameplayTagContainer OwnedTags;
	UPROPERTY(SaveGame) TMap<FGameplayTag, int32> TagCounts;
};

/** Explicit corridor route. No navigation, perception, combat, damage or missions run on a proxy. */
USTRUCT()
struct PROJECTVELKORRAN_API FSovCampaignMassRouteFragment : public FMassFragment
{
	GENERATED_BODY()
	UPROPERTY() TArray<FVector> Points;
	UPROPERTY() int32 NextPoint = 0;
	UPROPERTY() float Speed = 0.f;
	UPROPERTY() bool bPresentationOnly = false;
};
