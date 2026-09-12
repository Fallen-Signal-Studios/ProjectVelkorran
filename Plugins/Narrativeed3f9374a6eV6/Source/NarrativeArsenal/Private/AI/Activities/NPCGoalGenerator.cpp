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

namespace
{
	/**
	 * The name Narrative's authored generators already use for "re-run my predicate over the
	 * actors I currently perceive". GoalGenerator_Attack defines it as a public, zero-parameter
	 * Blueprint function and calls it from two places: once at the end of
	 * InitializeGoalGenerator, and again whenever the game state reports a faction ATTITUDE
	 * change. The pass itself walks GetCurrentlyPerceivedActors and routes each one back
	 * through the generator's own attack predicate.
	 *
	 * Binding to it by name keeps the hostility predicate in exactly one place. The alternative
	 * - reimplementing the predicate in native, or adding a second reconsideration path - is
	 * what this is written to avoid.
	 */
	const FName AuthoredRefreshPerceivedActorsName(TEXT("RefreshPerceivedActors"));
}

void UNPCGoalGenerator::ReevaluatePerceivedActors_Implementation()
{
	UFunction* AuthoredRefresh = FindFunction(AuthoredRefreshPerceivedActorsName);

	// Require the exact shape we can safely invoke. A generator that happens to use the name
	// for something else - anything taking parameters or returning a value - is left alone
	// rather than called with an empty frame.
	if (!AuthoredRefresh || AuthoredRefresh->NumParms != 0)
	{
		return;
	}

	ProcessEvent(AuthoredRefresh, nullptr);
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