// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Characters/SovNPCCharacterBase.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "SovCoordinationRuntimeTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class USovCoordinationTestASC : public USovBotTestASC
{
	GENERATED_BODY()
public:
	void SeedDead(bool bDead) { bIsDead = bDead; }
};

UCLASS(Transient, NotBlueprintable, NotPlaceable)
class ASovCoordinationTestNPC : public ASovNPCCharacterBase
{
	GENERATED_BODY()
public:
	ASovCoordinationTestNPC(const FObjectInitializer& Initializer);
	void InitializeTestCombat();
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;
};
