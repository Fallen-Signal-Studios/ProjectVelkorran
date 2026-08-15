// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include <Animation/AnimationPoseData.h>
#include <CollisionShape.h>
#include <BoneContainer.h>
#include <Animation/AnimNotifyQueue.h>
#include "WeaponAnimPose.generated.h"

// Copied from AnimPose.h but exposes certain functionality that was originally protected/private
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FWeaponAnimPose
{
	GENERATED_BODY()

	/** Returns whether or not the pose data was correctly initialized and populated */
	bool IsValid() const;
	
public:
	/** Initializes the various arrays, using and copying the provided bone container */
	void Init(const FBoneContainer& InBoneContainer);
	/** Populates an FCompactPose using the contained bone data */
	void GetPose(FCompactPose& InOutCompactPose) const;
	/** Generates the contained bone data using the provided Component and its AnimInstance */
	void SetPose(USkeletalMeshComponent* Component);
	/** Generates the contained bone data using the provided CompactPose */
	void SetPose(const FAnimationPoseData& PoseData);
	/** Copies the reference pose to animated pose data */
	void SetToRefPose();

	/** (Re-)Generates the world space transforms using populated local space data */
	void GenerateWorldSpaceTransforms();
	/** Resets all contained data, rendering the instance invalid */
	void Reset();

	/** Whether or not the contained data was initialized and can be used to store a pose */
	bool IsInitialized() const { return BoneNames.Num() != 0; }
	/** Whether or local space pose data has been populated */
	bool IsPopulated() const { return LocalSpacePoses.Num() != 0; }
	
public:
	UPROPERTY()
	TArray<FName> BoneNames;
	
	UPROPERTY()
	TArray<int32> BoneIndices;

	UPROPERTY()
	TArray<int32> ParentBoneIndices;

	UPROPERTY()
	TArray<FTransform> LocalSpacePoses;
	
	UPROPERTY()
	TArray<FTransform> WorldSpacePoses;

	UPROPERTY()
	TArray<FTransform> RefLocalSpacePoses;
	
	UPROPERTY()
	TArray<FTransform> RefWorldSpacePoses;

	UPROPERTY()
	TArray<FName> CurveNames;

	UPROPERTY()
	TArray<float> CurveValues;
	
	UPROPERTY()
	TArray<FName> SocketNames;

	UPROPERTY()
	TArray<FName> SocketParentBoneNames;
	
	UPROPERTY()
	TArray<FTransform> SocketTransforms;

	friend class UAnimPoseExtensions;
};

// Stores state data about a specific frame in an attack animation
USTRUCT(BlueprintType)
struct FDamageStateData
{
	GENERATED_BODY()

	FDamageStateData() = default;

	FDamageStateData(double InFrameTime)
	{
		FrameTime = InFrameTime;
	}

	double FrameTime = 0;
	bool bTriggered = false;

	FVector RelativeLocation = FVector::ZeroVector;
	FQuat RelativeRotation = FQuat::Identity;
};

// Container that stores damage state data and can be efficiently retrieved from a TMap
USTRUCT(BlueprintType)
struct FDamageStateDataContainer
{
	GENERATED_BODY()

	FDamageStateDataContainer() = default;

	FDamageStateDataContainer(const TArray<FDamageStateData>& InDamageStateData)
		: DamageStateDatas(InDamageStateData) {};
	
	TArray<FDamageStateData> DamageStateDatas;
};

// Wrapper class so we can send anim notify reference as a payload in gameplay event
UCLASS(BlueprintType)
class UAnimNotifyRefObject : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Notify Ref Object", meta = (ExposeOnSpawn))
	FAnimNotifyEventReference NotifyEventReference;
};

USTRUCT()
struct FWeaponCollisionData
{
	GENERATED_BODY()

	FWeaponCollisionData() = default;

	FWeaponCollisionData(const FVector& InRelativeLocation, const FQuat& InRelativeRotation, const FVector& InLastLocation, const FCollisionShape& InShape)
		: RelativeLocation(InRelativeLocation),
		RelativeRotation(InRelativeRotation),
		LastLocation(InLastLocation),
		Shape(InShape)
	{};
	
	FVector RelativeLocation = FVector::ZeroVector;
	FQuat RelativeRotation = FQuat::Identity;

	FVector LastLocation = FVector::ZeroVector;

	FCollisionShape Shape;
};
