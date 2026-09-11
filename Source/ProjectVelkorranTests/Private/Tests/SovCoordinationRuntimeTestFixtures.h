// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Characters/SovNPCCharacterBase.h"
#include "Character/NarrativeCharacterVisual.h"
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
	void PublishTestVisual(class ANarrativeCharacterVisual* Visual)
	{ CharVisual = Visual; CharacterVisualInitialized.Broadcast(this); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	virtual void GetActorEyesViewPoint(FVector& Location, FRotator& Rotation) const override;
};

UCLASS(Transient)
class USovCoordinationCollisionProbe : public UActorComponent
{
	GENERATED_BODY()
public:
	TFunction<void()> OnEnabled;
	virtual void OnActorEnableCollisionChanged() override
	{
		Super::OnActorEnableCollisionChanged();
		if (GetOwner()->GetActorEnableCollision() && OnEnabled)
		{ auto Callback = MoveTemp(OnEnabled); OnEnabled = nullptr; Callback(); }
	}
};
