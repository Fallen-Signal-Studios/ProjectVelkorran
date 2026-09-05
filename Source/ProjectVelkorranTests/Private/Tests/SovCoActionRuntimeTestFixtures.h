// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AI/Activities/NPCActivityComponent.h"
#include "Tests/SovBotAttackTestFixtures.h"
#include "Tests/SovCampaignRuntimeTestFixtures.h"
#include "SovCoActionRuntimeTestFixtures.generated.h"

/** Excludes asset initialization, while retaining real possession and mission identity. */
UCLASS(Transient, NotBlueprintable)
class ASovCoActionTestPlayer : public ASovCampaignRuntimeTestPawn
{
	GENERATED_BODY()
public:
	void InitializeForCoAction() { bCharacterReady = true; }
	virtual bool IsAlive() const override { return true; }
};

UCLASS(Transient, NotBlueprintable)
class ASovCoActionTestPlayerController : public ASovCampaignRuntimeTestController
{
	GENERATED_BODY()
public:
	virtual void GetPlayerViewPoint(FVector& Location, FRotator& Rotation) const override
	{ Location = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector; Rotation = FRotator::ZeroRotator; }
};

/** BeginPlay's owner cache without starting world-wide Narrative schedules. */
UCLASS(Transient, NotBlueprintable)
class USovCoActionTestActivities : public UNPCActivityComponent
{
	GENERATED_BODY()
public:
	void InitializeForCoAction() { OwnerController = CastChecked<ANarrativeNPCController>(GetOwner()); Activate(); }
};

UCLASS(Transient, NotBlueprintable)
class ASovCoActionTestNPCController : public ANarrativeNPCController
{
	GENERATED_BODY()
public:
	ASovCoActionTestNPCController(const FObjectInitializer& Initializer)
		: Super(Initializer.SetDefaultSubobjectClass<USovCoActionTestActivities>(TEXT("NPCActivityComponent"))) {}
	bool bCaptureCommandMoves = false;
	int32 CapturedMoveCount = 0;
	TWeakObjectPtr<AActor> CapturedMoveTarget;
	float CapturedAcceptanceRadius = 0.f;
	virtual FPathFollowingRequestResult MoveTo(const FAIMoveRequest& Request, FNavPathSharedPtr* OutPath = nullptr) override
	{
		if (!bCaptureCommandMoves) { return Super::MoveTo(Request, OutPath); }
		++CapturedMoveCount; CapturedMoveTarget = Request.GetGoalActor(); CapturedAcceptanceRadius = Request.GetAcceptanceRadius();
		FPathFollowingRequestResult Result; Result.Code = EPathFollowingRequestResult::Failed; return Result;
	}
};

UCLASS(Transient, NotBlueprintable)
class ASovCoActionTestNPC : public ASovBotTestCharacter
{
	GENERATED_BODY()
public:
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
};
