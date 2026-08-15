//  Copyright Narrative Tools 2022.

#include "QuestNodeUserWidget.h"

#include "DialogueBlueprint.h"
#include "IDialogueEditor.h"
#include "Tales/Dialogue.h"
#include "Tales/Quest.h"

void UQuestNodeUserWidget::InitializeFromNode(class UQuestNode* InNode, class UQuest* InQuest)
{
	if (InNode)
	{
		Node = InNode;
		Quest = InQuest;

		OnNodeInitialized(InNode, InQuest);
	}
}

void UQuestNodeUserWidget::AddOrJumpToMatchingDialogueNode()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	UBlueprint* DialogueBP = UBlueprint::GetBlueprintFromClass(Quest->GetQuestDialogueClass());
	if (AssetEditorSubsystem && DialogueBP)
	{
		// find or open graph editor for bp
		IDialogueEditor* DialogueEditor = static_cast<IDialogueEditor*>(AssetEditorSubsystem->FindEditorForAsset(DialogueBP, true));
		if (!DialogueEditor)
		{
			AssetEditorSubsystem->OpenEditorForAsset(DialogueBP);
			DialogueEditor = static_cast<IDialogueEditor*>(AssetEditorSubsystem->FindEditorForAsset(DialogueBP, true));
		}
		
		if (DialogueEditor)
		{
			DialogueEditor->JumpOrAddDialogueNode(Node->GetID());
		}
	}
}

bool UQuestNodeUserWidget::DoesMatchingDialogueNodeExist()
{
	const FName NodeID = Node->GetID();
	if (UDialogueBlueprint* DialogueBP = Quest? Cast<UDialogueBlueprint>(UBlueprint::GetBlueprintFromClass(Quest->GetQuestDialogueClass())) : nullptr)
	{
		return DialogueBP->DialogueTemplate->GetNodes().FindByPredicate([&NodeID](UDialogueNode* DialogueNode)
		{
			return DialogueNode->GetID() == NodeID;
		}) != nullptr;
	}

	return false;
}
