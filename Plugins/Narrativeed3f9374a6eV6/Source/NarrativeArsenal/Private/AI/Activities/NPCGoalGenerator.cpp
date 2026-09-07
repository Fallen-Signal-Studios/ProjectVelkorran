// Copyright Narrative Tools 2024. 


#include "AI/Activities/NPCGoalGenerator.h"
#include "AI/NarrativeAIStartupDiagnostics.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"

UNPCGoalGenerator::UNPCGoalGenerator(const FObjectInitializer& ObjectInitializer)
{

}

void UNPCGoalGenerator::Initialize(class ANarrativeNPCController* InOwnerController, class UNPCActivityComponent* InOwnerComp)
{
	check(InOwnerController && InOwnerComp);
	OwnerController = InOwnerController;
	OwnerActivityComponent = InOwnerComp;

	FNarrativeAIStartupDiagnostics::Record(this, TEXT("generator_initialize_enter"), GetPathNameSafe(InOwnerController));
	FNarrativeAIStartupDiagnostics::Snapshot(InOwnerController, true);
	InitializeGoalGenerator();
	FNarrativeAIStartupDiagnostics::Record(this, TEXT("generator_initialize_return"), GetPathNameSafe(InOwnerController));
	FNarrativeAIStartupDiagnostics::Snapshot(InOwnerController, true);
}

void UNPCGoalGenerator::InitializeGoalGenerator_Implementation()
{

}

UNPCGoalItem* UNPCGoalGenerator::AddGoalItem(class UNPCGoalItem* Goal, const bool bTriggerReselect)
{
	if (OwnerActivityComponent)
	{
		return OwnerActivityComponent->AddGoal(Goal, bTriggerReselect);
	}

	return nullptr; 
}

void UNPCGoalGenerator::RemoveGoalItem(class UNPCGoalItem* Goal)
{
	if (OwnerActivityComponent)
	{
		OwnerActivityComponent->RemoveGoal(Goal);
	}
}