// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovEclipseAnimationAuthoringLibrary.generated.h"

class UAnimBlueprint;
class UAnimationAsset;
class UBlendSpace;
class USkeleton;
class USkeletalMesh;

/** Editor-only adaptation of copied Aurelion presentation assets; never saves or changes gameplay. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovEclipseAnimationAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static FString AdaptCopiedAnimation(UAnimBlueprint* Blueprint, UBlendSpace* BlendSpace,
        USkeleton* Skeleton, const TArray<UAnimationAsset*>& Sources, const TArray<UAnimationAsset*>& Replacements);
    UFUNCTION(BlueprintPure, Category="Aurelion|Editor")
    static TArray<FName> MeshBones(USkeletalMesh* Mesh);
};
