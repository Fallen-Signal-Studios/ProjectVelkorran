// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "AI/Activities/NPCGoalItem.h"
#include "Navigation/PathFollowingComponent.h"
#include "SovCoActionActivity.generated.h"

class USovCompanionComponent;
class ASovCoActionAnchor;

UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovCoActionGoal : public UNPCGoalItem
{
	GENERATED_BODY()
public:
	USovCoActionGoal(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(Transient) TObjectPtr<USovCompanionComponent> Companion;
	UPROPERTY(Transient) TObjectPtr<ASovCoActionAnchor> Anchor;
	FGuid RequestId;
	virtual float GetGoalScore_Implementation() const override;
	virtual bool ShouldCleanup_Implementation() const override;
};

/** Native path following inside the existing Narrative activity slot. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovCoActionActivity : public UNPCActivity
{
	GENERATED_BODY()
public:
	USovCoActionActivity(const FObjectInitializer& ObjectInitializer);
	bool RestartOwnedMove();
protected:
	virtual bool RunActivity() override;
	virtual bool EndActivity() override;
	virtual void StopBehaviorTree() override;
private:
	void OnPathFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void ReleaseOwnedMove();
	FAIRequestID MoveRequestId;
	FDelegateHandle PathFinishedHandle;
	bool bResolved = false;
};
