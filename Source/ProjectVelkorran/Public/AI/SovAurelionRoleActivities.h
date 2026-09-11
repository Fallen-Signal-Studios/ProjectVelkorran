// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "AI/Activities/NPCActivity.h"
#include "BehaviorTree/BTTaskNode.h"
#include "AITypes.h"
#include "AI/SovAurelionEnemyRoles.h"
#include "SovAurelionRoleActivities.generated.h"

/** Goal-free one-time wall entry, selected by Narrative only when its authored physical route is admissible. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovAurelionWallTraversalActivity : public UNPCActivity
{
    GENERATED_BODY()
public:
    USovAurelionWallTraversalActivity(const FObjectInitializer& Initializer);
protected:
    virtual float ScoreActivity_Implementation(const FNPCGoalContainer& Goals, UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals) override;
};

/** Backline link support participates in the existing activity scheduler, not an actor tick. */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API USovAurelionWeaverSupportActivity : public UNPCActivity
{
    GENERATED_BODY()
public:
    USovAurelionWeaverSupportActivity(const FObjectInitializer& Initializer);
protected:
    virtual float ScoreActivity_Implementation(const FNPCGoalContainer& Goals, UNPCGoalItem*& OutBestGoal, TArray<UNPCGoalItem*>& OutInvalidGoals) override;
};

UCLASS(meta=(DisplayName="Aurelion: Traverse Authored Wall"))
class PROJECTVELKORRAN_API UBTTask_SovAurelionTraverseWall : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_SovAurelionTraverseWall(const FObjectInitializer& Initializer);
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
private:
    void ReleaseLease();
    TWeakObjectPtr<USovAurelionWallTraversalComponent> Traversal;
    uint64 Lease = 0;
};

UCLASS(meta=(DisplayName="Aurelion: Maintain Weaver Support"))
class PROJECTVELKORRAN_API UBTTask_SovAurelionWeaverSupport : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_SovAurelionWeaverSupport(const FObjectInitializer& Initializer);
protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;
private:
    friend struct FSovAurelionWeaverSupportTestAccess;
    // Search only at the support tree's bounded decision cadence, never every frame.
    bool FindRetreat(ASovAurelionWeaver* NPC, FVector& Destination, AActor*& ThreatActor) const;
    bool OwnsSupport() const;
    void ReleaseMove();
    TWeakObjectPtr<ASovAurelionWeaver> Weaver;
    TWeakObjectPtr<ANarrativeNPCController> Controller;
    TWeakObjectPtr<class UNarrativeAbilitySystemComponent> Abilities;
    TWeakObjectPtr<class UPathFollowingComponent> Paths;
    TWeakObjectPtr<class ASovEncounterDirector> Encounter;
    TWeakObjectPtr<AActor> Threat;
    FGuid Attempt;
    FGuid AnchorInstances[2];
    ESovCommandLinkState AnchorStates[2] = {ESovCommandLinkState::Inactive, ESovCommandLinkState::Inactive};
    FAIRequestID MoveId = FAIRequestID::InvalidRequest;
    uint64 ActorInfoEpoch = 0;
    uint64 ExecutionSerial = 0;
    FVector MoveGoal = FVector::ZeroVector;
    FVector ProgressLocation = FVector::ZeroVector;
    double MoveDeadline = 0.;
    double ProgressDeadline = 0.;
    double NextDecisionAt = 0.;
    double NextShareAt = 0.;
};
