// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SovAurelionEnemyAuthoringLibrary.generated.h"

class UBehaviorTree;
class UBlueprint;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct PROJECTVELKORRANEDITOR_API FSovAurelionEnemyAuthoringResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Aurelion|Editor") bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly, Category="Aurelion|Editor") TObjectPtr<UBehaviorTree> Tree;
    UPROPERTY(BlueprintReadOnly, Category="Aurelion|Editor") FString Error;
};

/** Fixed project-owned assets only. No save, PIE, plugin or source-template mutation. */
UCLASS()
class PROJECTVELKORRANEDITOR_API USovAurelionEnemyAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static bool CompileOwnedBlueprint(UBlueprint* Blueprint, UClass* ExpectedParent = nullptr);
    /** Creates a real task tree; a bounded Wait runs after both success and rejection. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static FSovAurelionEnemyAuthoringResult CreateRoleTree(bool bTraversal);
    /** Verifies the class, fixed owned package and role tree before setting the activity CDO. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static bool ConfigureRoleActivity(UBlueprint* Blueprint, bool bTraversal);
    /** Only a bone that exists on the provided real authored mesh can become Core's hit matcher. */
    UFUNCTION(BlueprintCallable, Category="Aurelion|Editor")
    static bool ConfigureEliteCore(UBlueprint* Blueprint, USkeletalMesh* Mesh, FName Bone);
};
