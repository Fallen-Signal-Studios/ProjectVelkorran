// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Melee/SovGameplayAbility_Melee.h"
#include "SovMeleeRuntimeTestFixtures.generated.h"
/** Content-free socket transform provider; all physics sweeps, ASC damage and ledger code remain production. */
UCLASS(Transient,NotBlueprintable)
class USovMeleeRuntimeTestMesh : public USkeletalMeshComponent
{
    GENERATED_BODY()
public:
    virtual bool DoesSocketExist(FName Name) const override;
    virtual FTransform GetSocketTransform(FName Name,ERelativeTransformSpace Space=RTS_World) const override;
};
UCLASS(Transient,NotBlueprintable)
class USovMeleeRuntimeTestAbility : public USovGameplayAbility_Melee
{
    GENERATED_BODY()
public:
    USovMeleeRuntimeTestAbility();
    virtual USkeletalMeshComponent* ResolveMeleeTraceMesh_Implementation() const override;
};
