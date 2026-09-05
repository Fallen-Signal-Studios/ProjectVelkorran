// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Companions/SovCompanionComponent.h"
#include "SovCompanionCommandActivity.generated.h"

UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovCompanionCommandGoal : public UNPCGoalItem
{
	GENERATED_BODY()
public:
	USovCompanionCommandGoal(const FObjectInitializer& Initializer);
	UPROPERTY(Transient) TObjectPtr<USovCompanionComponent> Companion;
	UPROPERTY(Transient) TObjectPtr<AActor> Target;
	ESovCompanionCommand Command = ESovCompanionCommand::Regroup;
	FVector HoldLocation = FVector::ZeroVector;
	FGuid RequestId;
	virtual float GetGoalScore_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};

/** Native authored follow/defend/combat decisions inside Narrative's one existing activity slot. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovCompanionCommandActivity : public UNPCActivity
{
	GENERATED_BODY()
public:
	USovCompanionCommandActivity(const FObjectInitializer& Initializer);
protected:
	virtual bool RunActivity() override;
	virtual bool EndActivity() override;
	virtual void StopBehaviorTree() override;
private:
	void TickCommand();
	FTimerHandle TickHandle;
};

/** Separate native activity identity for a curated, non-controlled protagonist kit. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovProtagonistCompanionActivity : public USovCompanionCommandActivity
{
	GENERATED_BODY()
public:
	USovProtagonistCompanionActivity(const FObjectInitializer& Initializer) : Super(Initializer) {}
};
