// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovCompanionCommandActivity.h"
#include "Engine/World.h"
#include "TimerManager.h"
USovCompanionCommandGoal::USovCompanionCommandGoal(const FObjectInitializer& Initializer) : Super(Initializer)
{ bSaveGoal = false; GoalLifetime = -1.f; DefaultScore = 5000.f; }
float USovCompanionCommandGoal::GetGoalScore_Implementation() const
{ return Companion && Companion->IsCommandCurrent(this) ? DefaultScore : -1.f; }
bool USovCompanionCommandGoal::ShouldCleanup_Implementation() const
{ return !Companion || !Companion->IsCommandCurrent(this); }
USovCompanionCommandActivity::USovCompanionCommandActivity(const FObjectInitializer& Initializer) : Super(Initializer)
{ SupportedGoalType = USovCompanionCommandGoal::StaticClass(); bIsInterruptable = true; }
bool USovCompanionCommandActivity::RunActivity()
{
	const auto* Goal = Cast<USovCompanionCommandGoal>(ActivityGoal);
	if (!Goal || !Goal->Companion || !Goal->Companion->IsCommandCurrent(Goal)) { return false; }
	LastActivateTime = GetWorld()->GetTimeSeconds();
	GetWorld()->GetTimerManager().SetTimer(TickHandle, this, &ThisClass::TickCommand, .2f, true, 0.f); return true;
}
void USovCompanionCommandActivity::TickCommand()
{ if (auto* Goal = Cast<USovCompanionCommandGoal>(ActivityGoal)) { if (Goal->Companion) { Goal->Companion->TickContextCommand(Goal); } } }
void USovCompanionCommandActivity::StopBehaviorTree() { if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(TickHandle); } }
bool USovCompanionCommandActivity::EndActivity()
{
	StopBehaviorTree();
	if (auto* Goal = Cast<USovCompanionCommandGoal>(ActivityGoal)) { if (Goal->Companion) { Goal->Companion->NotifyCommandInterrupted(Goal); } }
	return true;
}
