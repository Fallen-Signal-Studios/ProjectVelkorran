// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FQuestEditorCommands : public TCommands<FQuestEditorCommands>
{
public:

	FQuestEditorCommands();

	//Show the quest details
	TSharedPtr<FUICommandInfo> ShowQuestDetails;

	//Open a chrome tab with the tutorials
	TSharedPtr<FUICommandInfo> ViewTutorial;

	//Quickly add a compatible node. If we're at a state, add an action. If we're at an action, add a state
	TSharedPtr<FUICommandInfo> QuickAddNode;

	// jumps to the current active node
	TSharedPtr<FUICommandInfo> JumpToActiveNode;

	// When A Dialogue Asset class is selected for this quest, this will switch to that dialogue.
	TSharedPtr<FUICommandInfo> OpenQuestDialogue;

	virtual void RegisterCommands() override;

};
