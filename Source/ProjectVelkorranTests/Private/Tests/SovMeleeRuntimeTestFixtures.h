// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "Melee/SovGameplayAbility_Melee.h"
#include "Tests/SovHandoffRuntimeTestFixtures.h"
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
/** One node whose primary edge is built from socket offsets and whose second, separate edge shares the ledger. */
UCLASS(Transient,NotBlueprintable)
class USovMeleeRuntimeTestSegmentAbility : public USovMeleeRuntimeTestAbility
{
    GENERATED_BODY()
public:
    USovMeleeRuntimeTestSegmentAbility();
};

/** A fully ready campaign player for the production player-only combo input gate. */
UCLASS(Transient, NotBlueprintable)
class ASovMeleeRuntimeTestPlayer : public ASovHandoffRuntimeTestPawn
{
    GENERATED_BODY()
public:
    ASovMeleeRuntimeTestPlayer(const FObjectInitializer& Initializer) : Super(Initializer) {}
    virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};