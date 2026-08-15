// Copyright Narrative Tools 2022. 

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FDialogueEditorCommands : public TCommands<FDialogueEditorCommands>
{
public:

	FDialogueEditorCommands();

	//Show the quest details
	TSharedPtr<FUICommandInfo> ShowDialogueDetails;

	//Open a chrome tab with the tutorials
	TSharedPtr<FUICommandInfo> ViewTutorial;

	//Quickly add a compatible node. If we're at a state, add an action. If we're at an action, add a state
	TSharedPtr<FUICommandInfo> QuickAddNode;

	// jumps to the current active node
	TSharedPtr<FUICommandInfo> JumpToActiveNode;
	
	// When A quest has this Dialogue Asset class is selected, this will switch to that quest.
	TSharedPtr<FUICommandInfo> OpenLinkedQuest;

	virtual void RegisterCommands() override;

};
