// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "SovAurelionCrossfireQuery.generated.h"

/** Foot-space envelope of the authored combat floor, not the encounter entry trigger. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovAurelionCrossfireBounds
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Crossfire") FName EncounterId;
    UPROPERTY(EditAnywhere, Category="Crossfire") FVector Minimum = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, Category="Crossfire") FVector Maximum = FVector::ZeroVector;
    bool IsValid() const;
    bool Contains(const FVector& Foot, float Radius) const;
};

/** Internal cached EQS context: weak identities and observed position, never actor ownership. */
UCLASS()
class PROJECTVELKORRAN_API UEnvQueryItemType_SovCrossfireLease : public UEnvQueryItemType
{
    GENERATED_BODY()
public:
    UEnvQueryItemType_SovCrossfireLease(const FObjectInitializer& Initializer = FObjectInitializer::Get());
};

UCLASS()
class PROJECTVELKORRAN_API UEnvQueryContext_SovCrossfireLease : public UEnvQueryContext
{
    GENERATED_BODY()
public:
    UEnvQueryContext_SovCrossfireLease(const FObjectInitializer& Initializer = FObjectInitializer::Get());
    virtual void ProvideContext(FEnvQueryInstance& Query, FEnvQueryContextData& Data) const override;
};

/** Eight projected lateral samples. No tick, MoveTo, focus, perception or blackboard writes. */
UCLASS(meta=(DisplayName="Aurelion linked Security Drone lateral points"))
class PROJECTVELKORRAN_API UEnvQueryGenerator_SovCrossfire : public UEnvQueryGenerator
{
    GENERATED_BODY()
public:
    UEnvQueryGenerator_SovCrossfire(const FObjectInitializer& Initializer = FObjectInitializer::Get());
    UPROPERTY(EditAnywhere, Category="Crossfire") FSovAurelionCrossfireBounds Bounds;
    virtual void GenerateItems(FEnvQueryInstance& Query) const override;
};

/** One bounded native batch: complete paths, real capsule clearance, actual weapon-channel ray,
 * and useful separation from the closest active linked shooting lane. */
UCLASS(meta=(DisplayName="Aurelion clear linked crossfire lane"))
class PROJECTVELKORRAN_API UEnvQueryTest_SovCrossfire : public UEnvQueryTest
{
    GENERATED_BODY()
public:
    UEnvQueryTest_SovCrossfire(const FObjectInitializer& Initializer = FObjectInitializer::Get());
    UPROPERTY(EditAnywhere, Category="Crossfire") FSovAurelionCrossfireBounds Bounds;
    virtual void RunTest(FEnvQueryInstance& Query) const override;
};
