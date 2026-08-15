// Copyright Narrative Tools 2025.

#include "DialogueDebugger.h"

#include "DialogueEditorCommands.h"
#include "DialogueGraph.h"
#include "DialogueGraphEditor.h"
#include "DialogueGraphNode.h"
#include "Tales/Dialogue.h"
#include "Tales/NarrativeFunctionLibrary.h"

FDialogueDebugger::FDialogueDebugger()
	: bIsPIEActive(false), DialogueBlueprintAsset(nullptr), ActiveDialogue(nullptr), ActiveNode(nullptr)
{
	FEditorDelegates::BeginPIE.AddRaw(this, &FDialogueDebugger::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FDialogueDebugger::OnEndPIE);
	//FEditorDelegates::PausePIE.AddRaw(this, &FDialogueDebugger::OnPausePIE);
}

FDialogueDebugger::~FDialogueDebugger()
{
	ResetActiveNode();
	TalesComponentInstance.Reset();
	EditorOwner.Reset();
	DialogueBlueprintAsset.Reset();
	ActiveDialogue.Reset();
	ActiveNode.Reset();
}

void FDialogueDebugger::Tick(float DeltaTime)
{
#if WITH_EDITORONLY_DATA
	if (ActiveDialogue.IsValid() && IsDebuggerReady())
	{
		UDialogueNode* AssetCurrentNode = ActiveDialogue->GetCurrentNode();
		const FName NewNodeID = AssetCurrentNode->GetID();
		if (ActiveNode.IsValid())
		{
			if (ActiveNode != AssetCurrentNode)
			{
				const FName CurrentNodeID = ActiveNode->GetID();
				SetNodeActive(CurrentNodeID, false);
			
				SetNodeActive(NewNodeID, true);
				ActiveNode = AssetCurrentNode;
			}
		}
		else
		{
			SetNodeActive(NewNodeID, true);
			ActiveNode = AssetCurrentNode;
		}
	}
#endif
}

bool FDialogueDebugger::IsTickable() const
{
	return IsDebuggerReady();
}

void FDialogueDebugger::Setup(UDialogueBlueprint* DialogueBlueprint, TSharedRef<FDialogueGraphEditor> OwnerEditor)
{
	EditorOwner = OwnerEditor;
	DialogueBlueprintAsset = DialogueBlueprint;

	// resolve tales comp instance when opening during PIE
	if (!IsPIENotSimulating())
	{
		TalesComponentInstance = UNarrativeFunctionLibrary::GetTalesComponent(GEditor->PlayWorld);

		// resolve live current dialogue object
		if (UDialogue* CurrentDialogue = TalesComponentInstance.IsValid()? TalesComponentInstance->CurrentDialogue : nullptr)
		{
			if (CurrentDialogue->GetClass() == DialogueBlueprintAsset->GeneratedClass)
			{
				ActiveDialogue = TalesComponentInstance->CurrentDialogue;
			}
		}
		
		bIsPIEActive = true;
	}	
}

void FDialogueDebugger::BindCommands(TSharedRef<FUICommandList> EditorCommandList)
{
	EditorCommandList->MapAction(FDialogueEditorCommands::Get().JumpToActiveNode,
		FExecuteAction::CreateSP(this, &FDialogueDebugger::DebuggerJumpToNode),
		FCanExecuteAction::CreateSP(this, &FDialogueDebugger::IsDebuggerReady));
}

void FDialogueDebugger::RegisterDebuggerToolbar(UToolMenu* ToolMenu)
{
	FToolMenuSection* Section = ToolMenu->FindSection("Debugging");
	if (!Section)
	{
		return;
	}

	// button
	FToolMenuEntry NarrativeToolbarEntry = FToolMenuEntry::InitToolBarButton(
		FDialogueEditorCommands::Get().JumpToActiveNode,
		FText::GetEmpty(),
		INVTEXT("Jump to the current active node"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Profiler.Misc.SortDescending")
	);

	Section->AddEntry(NarrativeToolbarEntry);
}

bool FDialogueDebugger::IsPIENotSimulating()
{
	return !GEditor->bIsSimulatingInEditor && (GEditor->PlayWorld == nullptr);
}

void FDialogueDebugger::OnBeginPIE(const bool bIsSimulating)
{
	bIsPIEActive = true;

	if (ActiveNode.IsValid())
	{
		SetNodeActive(ActiveNode->GetID(), false);
	}

	FDialogueDelegates::OnDialogueStarted.RemoveAll(this);
	FDialogueDelegates::OnDialogueEnd.RemoveAll(this);
	
	FDialogueDelegates::OnDialogueStarted.AddRaw(this, &FDialogueDebugger::OnDialogueStarted);
	FDialogueDelegates::OnDialogueEnd.AddRaw(this, &FDialogueDebugger::OnDialogueEnd);
}

void FDialogueDebugger::OnEndPIE(const bool bIsSimulating)
{
	bIsPIEActive = false;
	FDialogueDelegates::OnDialogueStarted.RemoveAll(this);
	FDialogueDelegates::OnDialogueEnd.RemoveAll(this);

	ResetActiveNode();
}

void FDialogueDebugger::OnDialogueStarted(UTalesComponent* TalesComponent, UDialogue* Dialogue, const FName& OptionalStartFromID)
{
	UDialogueBlueprint* DialogueBP = DialogueBlueprintAsset.Get();
	if (!IsValid(DialogueBP) || !Dialogue)
	{
		// safe back out
		ResetActiveNode();
		return; 
	}
	
	if (Dialogue->GetClass() == DialogueBP->GeneratedClass)
	{
		if (!TalesComponentInstance.IsValid())
		{
			TalesComponentInstance = TalesComponent;
		}
		
		ActiveDialogue = Dialogue;
	}
}

void FDialogueDebugger::OnDialogueEnd(UTalesComponent* TalesComponent, UDialogue* Dialogue)
{
	if (TalesComponentInstance.IsValid() && Dialogue->GetClass() == DialogueBlueprintAsset->GeneratedClass)
	{
		ResetActiveNode();
		
		ActiveDialogue.Reset();
	}
}

void FDialogueDebugger::DebuggerJumpToNode()
{
	if (IsDebuggerReady() && EditorOwner.IsValid() && DialogueBlueprintAsset.IsValid() && ActiveNode.IsValid())
	{
		UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(DialogueBlueprintAsset->DialogueGraph);

		TArray<UDialogueGraphNode*> States;
		DialogueGraph->GetNodesOfClass<UDialogueGraphNode>(States);
		
		const FName StateNodeID = ActiveNode->GetID();
		for (UDialogueGraphNode* State : States)
		{
			if (State->DialogueNode->GetID() == StateNodeID)
			{
				EditorOwner.Pin()->JumpToNode(State);
			}
		}		
	}
}

void FDialogueDebugger::SetNodeActive(const FName& NodeNodeID, const bool Active)
{	
	if (!DialogueBlueprintAsset.IsValid())
	{
		return;
	}

	UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(DialogueBlueprintAsset->DialogueGraph);
	
	TArray<UDialogueGraphNode*> Nodes;
	DialogueGraph->GetNodesOfClass<UDialogueGraphNode>(Nodes);

	for (UDialogueGraphNode* Node : Nodes)
	{
		if (Node->DialogueNode->GetID() == NodeNodeID)
		{
			Node->bActiveNode = Active;
			return;
		}
	}	
}

void FDialogueDebugger::ResetActiveNode()
{
	if (ActiveNode.IsValid())
	{
		SetNodeActive(ActiveNode->GetID(), false);
	}
}

