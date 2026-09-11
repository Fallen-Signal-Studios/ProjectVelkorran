// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AI/Activities/NPCActivity.h"
#include "SovNPCGoalKeyLifetimeTestFixtures.generated.h"

/** Runs the real native scoring/selection; no BT or combat is needed for key lifetime. */
UCLASS()
class USovNPCGoalKeyLifetimeTestActivity : public UNPCActivity
{
    GENERATED_BODY()
public:
    void Support(UClass* GoalClass) { SupportedGoalType = GoalClass; }
    float ScoreOne(const UNPCGoalItem* Goal) { return ScoreGoalItem(Goal); }
protected:
    virtual bool RunActivity() override { return true; }
    virtual bool EndActivity() override { return true; }
};

UCLASS()
class USovNPCGoalKeyLifetimeTestGoal : public UNPCGoalItem
{
    GENERATED_BODY()
public:
    mutable int32 KeyReads = 0;
    mutable int32 ScoreReads = 0;
    int32 Removals = 0;
    TFunction<void()> DuringRemoval;
    TFunction<void()> DuringInitialize;
    mutable TFunction<void()> DuringKeyRead;
    virtual void Initialize_Implementation() override
    {
        const TFunction<void()> Callback = DuringInitialize;
        if (Callback) { Callback(); }
    }
    virtual UObject* GetGoalKey_Implementation() const override
    {
        ++KeyReads;
        const TFunction<void()> Callback = DuringKeyRead;
        if (Callback) { Callback(); }
        return GoalKey;
    }
    virtual float GetGoalScore_Implementation() const override { ++ScoreReads; return DefaultScore; }
    virtual void OnRemoved_Implementation() override
    {
        ++Removals;
        const TFunction<void()> Callback = DuringRemoval;
        if (Callback) { Callback(); }
    }
};
