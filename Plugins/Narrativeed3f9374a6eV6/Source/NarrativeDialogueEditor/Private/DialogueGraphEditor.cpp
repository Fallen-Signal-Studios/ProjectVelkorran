// Copyright Narrative Tools 2022. 

#include "DialogueGraphEditor.h"
#include "NarrativeDialogueEditorModule.h"
#include "ScopedTransaction.h"
#include "DialogueEditorTabFactories.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"
#include "DialogueGraphNode.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "EdGraphUtilities.h"
#include "DialogueGraph.h"
#include "DialogueGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "DialogueEditorModes.h"
#include "Widgets/Docking/SDockTab.h"
#include "DialogueEditorToolbar.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Tales/DialogueSM.h"
#include "Tales/DialogueAsset.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphNode.h"
#include "Tales/Dialogue.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h" 
#include "DialogueEditorCommands.h"
#include "DialogueEditorSettings.h"
#include <SBlueprintEditorToolbar.h>
#include <Kismet2/DebuggerCommands.h>
#include "SKismetInspector.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_CustomEvent.h"
#include "DialogueGraphNode_NPC.h"
#include "DialogueGraphNode_Player.h"
#include <Engine/World.h>

#include "DialogueDebugger.h"
#include "DialogueEditorStyle.h"
#include "DialogueNodeUserWidget.h"
#include "LevelEditor.h"
#include "SNodePanel.h"
#include "Tales/NarrativeDialogueSettings.h"
#include "Toolkits/ToolkitManager.h"

#define LOCTEXT_NAMESPACE "DialogueAssetEditor"

const FName FDialogueGraphEditor::DialogueEditorMode(TEXT("DialogueEditor"));

//TODO change this to the documentation page after documentation is ready for use
static const FString NarrativeHelpURL("http://www.google.com");

FDialogueGraphEditor::FDialogueGraphEditor()
{
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor != NULL)
	{
		Editor->RegisterForUndo(this);
	}
}

FDialogueGraphEditor::~FDialogueGraphEditor()
{
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);

	SetGraphEditable(true);
	
	UEditorEngine* Editor = (UEditorEngine*)GEngine;
	if (Editor)
	{
		Editor->UnregisterForUndo(this);
	}
}


void FDialogueGraphEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	if (NewSelection.Num() == 1)
	{
		for (auto& Obj : NewSelection)
		{
			//Want to edit the underlying dialogue object, not the graph node
			if (UDialogueGraphNode* GraphNode = Cast<UDialogueGraphNode>(Obj))
			{
				TSet<class UObject*> ModifiedSelection;
				ModifiedSelection.Add(GraphNode->DialogueNode);
				FBlueprintEditor::OnSelectedNodesChangedImpl(ModifiedSelection);
				return;
			}
		}
	}


	FBlueprintEditor::OnSelectedNodesChangedImpl(NewSelection);
}
//
//void FDialogueGraphEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
//{
//	SelectedNodesCount = NewSelection.Num();
//
//	if (SelectedNodesCount == 0)
//	{
//		DetailsView->SetObject(DialogueBlueprint->Dialogue);
//		return;
//	}
//
//	UDialogueGraphNode* SelectedNode = nullptr;
//
//	for (UObject* Selection : NewSelection)
//	{
//		if (UDialogueGraphNode* Node = Cast<UDialogueGraphNode>(Selection))
//		{
//			SelectedNode = Node;
//			break;
//		}
//	}
//
//	if (UDialogueGraph* MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph))
//	{
//		if (DetailsView.IsValid())
//		{
//			if (SelectedNode)
//			{
//				//Edit the underlying graph node object 
//				if (UDialogueGraphNode* Node = Cast<UDialogueGraphNode>(SelectedNode))
//				{
//					DetailsView->SetObject(Node->DialogueNode);
//				}
//			}
//		}
//		else
//		{
//			DetailsView->SetObject(nullptr);
//		}
//	}
//}

void FDialogueGraphEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);

	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FDialogueGraphEditor::OnWorldChange(UWorld* World, EMapChangeType MapChangeType)
{
	//Dialogue graph nodes will be referencing the UWorld, and if it changes this will break, so point them at transient package 
	if (World)
	{
		for (TObjectIterator<UUserWidget> Itr; Itr; ++Itr)
		{
			UUserWidget* Widget = *Itr;

			if (Widget->IsA<UDialogueNodeUserWidget>())
			{
				Widget->Rename(nullptr, GetTransientPackage());
			}
		}
	}
}

void FDialogueGraphEditor::SwitchToQuestAsset()
{
	if (!DialogueBlueprint)
	{
		return;
	}
	
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	UDialogue* Dialogue = Cast<UDialogue>(DialogueBlueprint->GeneratedClass.GetDefaultObject());
	if (AssetEditorSubsystem && Dialogue)
	{
		AssetEditorSubsystem->OpenEditorForAsset(UBlueprint::GetBlueprintFromClass(Dialogue->EditorLinkedQuest));
	}
}

bool FDialogueGraphEditor::CanSwitchToQuestAsset()
{
	if (!DialogueBlueprint)
	{
		return false;
	}
	
	UDialogue* Dialogue = Cast<UDialogue>(DialogueBlueprint->GeneratedClass.GetDefaultObject());
	return Dialogue? Dialogue->EditorLinkedQuest != nullptr : false;
}

void FDialogueGraphEditor::InitDialogueEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDialogueBlueprint* InDialogue)
{
	// dialogue assets cannot be edited during PIE
	FEditorDelegates::BeginPIE.AddRaw(this, &FDialogueGraphEditor::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FDialogueGraphEditor::OnEndPIE);
	
	DialogueBlueprint = InDialogue; 

	// create a debugger instance before binding commands
	Debugger = MakeShareable(new FDialogueDebugger);
	
	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	GetToolkitCommands()->Append(FPlayWorldCommands::GlobalPlayWorldActions.ToSharedRef());

	CreateDefaultCommands();
	RegisterMenus();
	
	CreateInternalWidgets();

	TSharedPtr<FDialogueGraphEditor> ThisPtr(SharedThis(this));

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(DialogueBlueprint);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, FNarrativeDialogueEditorModule::DialogueEditorAppId, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);

	TArray<UBlueprint*> EditedBlueprints;
	EditedBlueprints.Add(DialogueBlueprint);

	CommonInitialization(EditedBlueprints, false);

	AddApplicationMode(DialogueEditorMode, MakeShareable(new FDialogueEditorApplicationMode(SharedThis(this))));

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	SetCurrentMode(DialogueEditorMode);

	// setup debugger
	Debugger->Setup(DialogueBlueprint, SharedThis(this));

	if (UToolMenu* EditorToolbar = UToolMenus::Get()->FindMenu("AssetEditor.Dialogue Editor.Toolbar.DialogueEditor"))
	{
		Debugger->RegisterDebuggerToolbar(EditorToolbar);

		FToolMenuSection& Section = EditorToolbar->AddSection("EditQuestAsset", FText::GetEmpty(), {"Debugging", EToolMenuInsertType::After});
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FDialogueEditorCommands::Get().OpenLinkedQuest,
			LOCTEXT("DialogueGraphEditor_EditQuest", "Edit Quest"),
			LOCTEXT("DialogueGraphEditor_EditQuest_ToolTip", "When A quest has this Dialogue Asset class is selected, this will switch to that quest."),
			FSlateIcon(FDialogueEditorStyle::GetStyleSetName(), "DialogueEditor.QuestIcon"))
		);
	}

	PostLayoutBlueprintEditorInitialization();
	
	if (!FDialogueDebugger::IsPIENotSimulating())
	{
		SetGraphEditable(false);
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddRaw(this, &FDialogueGraphEditor::OnWorldChange);
}

FName FDialogueGraphEditor::GetToolkitFName() const
{
	return FName("Dialogue Editor");
}

FText FDialogueGraphEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "DialogueEditor");
}

FString FDialogueGraphEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DialogueEditor").ToString();
}

FLinearColor FDialogueGraphEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

FText FDialogueGraphEditor::GetToolkitToolTipText() const
{
	if (DialogueBlueprint)
	{
		return FAssetEditorToolkit::GetToolTipTextForObject(DialogueBlueprint);
	}
	return FText();
}

void FDialogueGraphEditor::CreateDialogueCommandList()
{

}


void FDialogueGraphEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	if (GraphEditorCommandsList.IsValid())
	{
		FDialogueEditorCommands::Register();

		GraphEditorCommandsList->MapAction(FDialogueEditorCommands::Get().QuickAddNode,
		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::QuickAddNode),
		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanQuickAddNode));

		ToolkitCommands->MapAction(
		FDialogueEditorCommands::Get().OpenLinkedQuest,
		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::SwitchToQuestAsset),
		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanSwitchToQuestAsset));
	}
}

void FDialogueGraphEditor::PasteNodesHere(class UEdGraph* DestinationGraph, const FVector2f& GraphLocation)
{
	if (UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(DestinationGraph))
	{
		TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
		if (!CurrentGraphEditor.IsValid())
		{
			return;
		}

		// Undo/Redo support
		const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());

		DialogueGraph->Modify();

		UDialogueGraphNode* SelectedParent = NULL;
		bool bHasMultipleNodesSelected = false;

		const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

		// Clear the selection set (newly pasted stuff will be selected)
		CurrentGraphEditor->ClearSelectionSet();

		// Grab the text to paste from the clipboard.
		FString TextToImport;
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);

		// Import the nodes
		TSet<UEdGraphNode*> PastedNodes;
		FEdGraphUtilities::ImportNodesFromText(DialogueGraph, TextToImport, /*out*/ PastedNodes);

		if (PastedNodes.Num())
		{
			// walk nodes to be pasted, and accumulate their position 
			FVector2D AvgNodePosition(0.0f, 0.0f);
			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* Node = *It;
				AvgNodePosition.X += Node->NodePosX;
				AvgNodePosition.Y += Node->NodePosY;
			}
			// average the accumulated node positions
			float InvNumNodes = 1.0f / static_cast<float>(PastedNodes.Num());
			AvgNodePosition.X *= InvNumNodes;
			AvgNodePosition.Y *= InvNumNodes;
			
			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* PasteNode = *It;
				UDialogueGraphNode* PasteDialogueNode = Cast<UDialogueGraphNode>(PasteNode);

				if (PasteNode && PasteDialogueNode)
				{
					// set new paste node position, accounting for selected nodes average position
					PasteNode->NodePosX = static_cast<int32>((PasteNode->NodePosX - AvgNodePosition.X) + GraphLocation.X);
					PasteNode->NodePosY = static_cast<int32>((PasteNode->NodePosY - AvgNodePosition.Y) + GraphLocation.Y);

					PasteNode->SnapToGrid(SNodePanel::GetSnapGridSize());

					// Give new node a different Guid from the old one
					PasteNode->CreateNewGuid();

					//New dialogue graph node will point to old dialouenode, duplicate a new one for our new node
					UDialogueNode* DupNode = DialogueGraph->DuplicateNode(PasteDialogueNode->DialogueNode);

					PasteDialogueNode->DialogueNode = DupNode;

					// Select the newly pasted stuff now that it has a new object 
					CurrentGraphEditor->SetNodeSelection(PasteNode, true);
				}
			}

			//Now that everything has been pasted, iterate a second time to rebuild the new nodes connections 
			for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
			{
				UEdGraphNode* PasteNode = *It;
				UDialogueGraphNode* PasteDialogueNode = Cast<UDialogueGraphNode>(PasteNode);

				//Dialogue nodes connections will still be outdated, update these to the new connections
				DialogueGraph->NodeAdded(PasteDialogueNode);
				DialogueGraph->PinRewired(PasteDialogueNode, PasteDialogueNode->GetOutputPin());
			}

		}
		else
		{
			/*
			We may be trying to paste from narrative dialogue markup.
			
			Format is:

			Gavin: Hey dude!
			Erno: Hey, how are you?
			Player: What are you guys doing here?

			Go through line by line and add these nodes in! 
			*/
			TArray<FString> PastedDialogueLines;
			TextToImport.ParseIntoArrayLines(PastedDialogueLines);

			DialogueGraph->Modify();

			FVector2D PasteLoc = FVector2D(GraphLocation.X, GraphLocation.Y);
			UDialogueGraphNode* LastNode = nullptr;

			UBlueprint* DialogueBP = GetBlueprintObj();
			UDialogue* Dialogue = nullptr;

			if (DialogueBP)
			{
				Dialogue = Cast<UDialogue>(DialogueBP->GeneratedClass->GetDefaultObject());
			}

			if (!Dialogue)
			{
				return;
			}

			if (const UDialogueGraphSchema* Schema = Cast<UDialogueGraphSchema>(DialogueGraph->GetSchema()))
			{
				for (auto& DialogueLine : PastedDialogueLines)
				{
					//on hit and run wiki any lines without colons wont be character lines
					if (!DialogueLine.Contains(":"))
					{
						continue;
					}

					int32 ColonIdx = -1;
					DialogueLine.FindChar(':', ColonIdx);

					FString SpeakerID = DialogueLine.LeftChop(DialogueLine.Len() - ColonIdx);
					SpeakerID.RemoveSpacesInline();

					FString LineText = DialogueLine.RightChop(ColonIdx);

					LineText.RemoveFromStart(":");


					FDialogueSchemaAction_NewNode AddNewNode;
					UDialogueGraphNode* Node;
					UClass* DialogueNodeClass = SpeakerID.Equals("Player", ESearchCase::IgnoreCase) ? UDialogueGraphNode_Player::StaticClass() : UDialogueGraphNode_NPC::StaticClass();

					//Dont do anything if we're already linked somewhere
					Node = NewObject<UDialogueGraphNode>(DialogueGraph, DialogueNodeClass);

					AddNewNode.NodeTemplate = Node;

					if (!LastNode)
					{
						AddNewNode.PerformAction(DialogueGraph, nullptr, PasteLoc);
					}
					else
					{
						AddNewNode.PerformAction(DialogueGraph, LastNode->GetOutputPin(), PasteLoc);
					}

					if (ColonIdx > -1)
					{
						if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(Node->DialogueNode))
						{
							if (Dialogue)
							{
								//If the speaker doesn't exist add it automatically
								bool bSpeakerFound = false;

								for (auto& Speaker : Dialogue->Speakers)
								{
									if (Speaker.SpeakerID.ToString().Equals(SpeakerID, ESearchCase::IgnoreCase))
									{
										bSpeakerFound = true;
									}
								}

								if (!bSpeakerFound)
								{
									FSpeakerInfo NewSpeaker;
									NewSpeaker.SpeakerID = FName(SpeakerID);
									Dialogue->Speakers.Add(NewSpeaker);
								}
							}
							NPCNode->SetSpeakerID(FName(SpeakerID));
						}
					}

					Node->DialogueNode->Line.Text = FText::FromString(LineText);
					Node->DialogueNode->GenerateIDFromText(); // This doesn't seem to get called automatically so call manually 

					// Update UI
					CurrentGraphEditor->NotifyGraphChanged();

					UObject* GraphOwner = DialogueGraph->GetOuter();
					if (GraphOwner)
					{
						GraphOwner->PostEditChange();
						GraphOwner->MarkPackageDirty();
					}

					if (const UNarrativeDialogueSettings* DialogueSettings = GetDefault<UNarrativeDialogueSettings>())
					{
						if (DialogueSettings->bEnableVerticalWiring)
						{
							PasteLoc += FVector2D(0.f, 250.f);
						}
						else
						{
							PasteLoc += FVector2D(550.f, 0.f);
						}
					}

					LastNode = Node;
				}
			}
		}

		// Update UI
		CurrentGraphEditor->NotifyGraphChanged();

		UObject* GraphOwner = DialogueGraph->GetOuter();
		if (GraphOwner)
		{
			GraphOwner->PostEditChange();
			GraphOwner->MarkPackageDirty();
		}
	}
	else
	{
		FBlueprintEditor::PasteNodesHere(DestinationGraph, GraphLocation);
	}
}

bool FDialogueGraphEditor::CanPasteNodes() const
{
	return true;
}

void FDialogueGraphEditor::DeleteSelectedNodes()
{
	TSharedPtr<SGraphEditor> CurrentGraphEditor = FocusedGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	if(UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(CurrentGraphEditor->GetCurrentGraph()))
	{
		if (!IsValid(CurrentGraphEditor->GetCurrentGraph()))
		{
			return;
		}

		const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
		CurrentGraphEditor->GetCurrentGraph()->Modify();

		//For now in dialogues we'll transact all nodes because they may be linked to the ones we're about to delete 
		//TODO only ones that are linked to nodes in selection set 
		for (auto& DNode : DialogueGraph->Nodes)
		{
			if(UDialogueGraphNode* DGNode = Cast<UDialogueGraphNode>(DNode))
			{
				if (DGNode->DialogueNode)
				{
					DGNode->DialogueNode->Modify();
				}

				DNode->Modify();
				
			}
		}

		const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
		CurrentGraphEditor->ClearSelectionSet();

		for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
		{
			if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
			{
				if (Node->CanUserDeleteNode())
				{
					Node->Modify();
					Node->DestroyNode();
				}
			}
		}
	}
	else
	{
		FBlueprintEditor::DeleteSelectedNodes();
	}


}

bool FDialogueGraphEditor::Dialogue_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding)
{
	return FBlueprintEditor::GetBoundsForSelectedNodes(Rect, Padding);
}

//void FDialogueGraphEditor::Dialogue_SelectAllNodes()
//{
//	if (TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin())
//	{
//		CurrentGraphEditor->SelectAllNodes();
//	}
//}
//
//bool FDialogueGraphEditor::Dialogue_CanSelectAllNodes() const
//{
//	return true;
//}
//
//void FDialogueGraphEditor::Dialogue_DeleteSelectedNodes()
//{
//	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
//	if (!CurrentGraphEditor.IsValid())
//	{
//		return;
//	}
//
//	const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
//	CurrentGraphEditor->GetCurrentGraph()->Modify();
//
//	const FGraphPanelSelectionSet SelectedNodes = CurrentGraphEditor->GetSelectedNodes();
//	CurrentGraphEditor->ClearSelectionSet();
//
//	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
//	{
//		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
//		{
//			if (Node->CanUserDeleteNode())
//			{
//				Node->Modify();
//				Node->DestroyNode();
//			}
//		}
//	}
//}
//
//bool FDialogueGraphEditor::Dialogue_CanDeleteNodes() const
//{
//	// If any of the nodes can be deleted then we should allow deleting
//	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
//	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
//	{
//		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
//		if (Node && Node->CanUserDeleteNode())
//		{
//			return true;
//		}
//	}
//
//	return false;
//}
//
//void FDialogueGraphEditor::Dialogue_DeleteSelectedDuplicatableNodes()
//{
//	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
//	if (!CurrentGraphEditor.IsValid())
//	{
//		return;
//	}
//	const FGraphPanelSelectionSet OldSelectedNodes = CurrentGraphEditor->GetSelectedNodes();
//	CurrentGraphEditor->ClearSelectionSet();
//
//	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
//	{
//		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
//		if (Node && Node->CanDuplicateNode())
//		{
//			CurrentGraphEditor->SetNodeSelection(Node, true);
//		}
//	}
//
//	// Delete the duplicatable nodes
//	DeleteSelectedNodes();
//
//	CurrentGraphEditor->ClearSelectionSet();
//
//	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
//	{
//		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
//		{
//			CurrentGraphEditor->SetNodeSelection(Node, true);
//		}
//	}
//}
//
//void FDialogueGraphEditor::Dialogue_CutSelectedNodes()
//{
//	CopySelectedNodes();
//	DeleteSelectedDuplicatableNodes();
//}
//
//bool FDialogueGraphEditor::Dialogue_CanCutNodes() const
//{
//	return CanCopyNodes() && CanDeleteNodes();
//}
//
//void FDialogueGraphEditor::Dialogue_CopySelectedNodes()
//{
//	// Export the selected nodes and place the text on the clipboard
//	FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
//
//	FString ExportedText;
//
//	for (FGraphPanelSelectionSet::TIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
//	{
//		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
//		UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
//		if (Node == nullptr)
//		{
//			SelectedIter.RemoveCurrent();
//			continue;
//		}
//
//		Node->PrepareForCopying();
//	}
//
//	FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
//	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
//
//}
//
//bool FDialogueGraphEditor::Dialogue_CanCopyNodes() const
//{
//	// If any of the nodes can be duplicated then we should allow copying
//	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
//	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
//	{
//		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
//		if (Node && Node->CanDuplicateNode())
//		{
//			return true;
//		}
//	}
//
//	return false;
//}
//
//bool FDialogueGraphEditor::Dialogue_CanPasteNodes() const
//{
//	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
//	if (!CurrentGraphEditor.IsValid())
//	{
//		return false;
//	}
//
//	FString ClipboardContent;
//	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
//
//	return true;
//	//return FEdGraphUtilities::CanImportNodesFromText(CurrentGraphEditor->GetCurrentGraph(), ClipboardContent);
//}
//
//void FDialogueGraphEditor::Dialogue_DuplicateNodes()
//{
//	CopySelectedNodes();
//	PasteNodes();
//}
//
//bool FDialogueGraphEditor::Dialogue_CanDuplicateNodes() const
//{
//	//Duplicating nodes is disabled for now
//	//return false;
//
//	return CanCopyNodes();
//}

//bool FDialogueGraphEditor::Dialogue_GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding)
//{
//	const bool bResult = FBlueprintEditor::GetBoundsForSelectedNodes(Rect, Padding);
//
//	return bResult;
//}

void FDialogueGraphEditor::ShowDialogueDetails()
{
	DetailsView->SetObject(DialogueBlueprint);
}

bool FDialogueGraphEditor::CanShowDialogueDetails() const
{
	return IsValid(DialogueBlueprint);
}

void FDialogueGraphEditor::OpenNarrativeTutorialsInBrowser()
{
	FPlatformProcess::LaunchURL(*NarrativeHelpURL, NULL, NULL);
}

bool FDialogueGraphEditor::CanOpenNarrativeTutorialsInBrowser() const
{
	return true;
}

void FDialogueGraphEditor::QuickAddNode()
{
	//Disabled for now until we can find out why creating actions cause a nullptr crash
	//return;

	TSharedPtr<SGraphEditor> CurrentGraphEditor = UpdateGraphEdPtr.Pin();
	if (!CurrentGraphEditor.IsValid())
	{
		return;
	}

	const FScopedTransaction Transaction(FDialogueEditorCommands::Get().QuickAddNode->GetDescription());
	UDialogueGraph* DialogueGraph = Cast<UDialogueGraph>(CurrentGraphEditor->GetCurrentGraph());

	DialogueGraph->Modify();

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	if (const UDialogueGraphSchema* Schema = Cast<UDialogueGraphSchema>(DialogueGraph->GetSchema()))
	{
		for (auto& SelectedNode : SelectedNodes)
		{
			FDialogueSchemaAction_NewNode AddNewNode;
			UDialogueGraphNode* Node;

			//We we're quick adding from an action, add a new state after the action
			if (UDialogueGraphNode_NPC* NPCNode = Cast<UDialogueGraphNode_NPC>(SelectedNode))
			{
				Node = NewObject<UDialogueGraphNode_NPC>(DialogueGraph, UDialogueGraphNode_NPC::StaticClass());
				AddNewNode.NodeTemplate = Node;

				FVector2D NewStateLocation = FVector2D(NPCNode->NodePosX, NPCNode->NodePosY);

				NewStateLocation.X += 300.f;

				AddNewNode.PerformAction(DialogueGraph, NPCNode->GetOutputPin(), NewStateLocation);


				// Update UI
				CurrentGraphEditor->NotifyGraphChanged();

				UObject* GraphOwner = DialogueGraph->GetOuter();
				if (GraphOwner)
				{
					GraphOwner->PostEditChange();
					GraphOwner->MarkPackageDirty();
				}
			}

		}
	}
}

bool FDialogueGraphEditor::CanQuickAddNode() const
{
	return true;
}

void FDialogueGraphEditor::PostUndo(bool bSuccess)
{	
	FBlueprintEditor::PostUndo(bSuccess);
	//if (bSuccess)
	//{
	//}

	//FEditorUndoClient::PostUndo(bSuccess);

	//if (DialogueBlueprint->DialogueGraph)
	//{
	//	// Update UI
	//	DialogueBlueprint->DialogueGraph->NotifyGraphChanged();
	//	if (DialogueBlueprint)
	//	{
	//		DialogueBlueprint->PostEditChange();
	//		DialogueBlueprint->MarkPackageDirty();
	//	}
	//}

}

void FDialogueGraphEditor::PostRedo(bool bSuccess)
{
	FBlueprintEditor::PostRedo(bSuccess);

	//// Update UI
	//if (DialogueBlueprint->DialogueGraph)
	//{
	//	DialogueBlueprint->DialogueGraph->NotifyGraphChanged();
	//	if (DialogueBlueprint)
	//	{
	//		DialogueBlueprint->PostEditChange();
	//		DialogueBlueprint->MarkPackageDirty();
	//	}
	//}
}


void FDialogueGraphEditor::CreateDefaultCommands()
{
	FBlueprintEditor::CreateDefaultCommands();

	FDialogueEditorCommands::Register();
	
	if (Debugger)
	{
		Debugger->BindCommands(ToolkitCommands);
	}
}

UBlueprint* FDialogueGraphEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (int32 i = 0; i < EditingObjs.Num(); ++i)
	{
		if (EditingObjs[i]->IsA<UDialogueBlueprint>()) { return (UBlueprint*)EditingObjs[i]; }
	}
	return nullptr;
}

void FDialogueGraphEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);

	// Custom menu for K2 schemas
	if (InGraph->Schema != nullptr && InGraph->Schema->IsChildOf(UDialogueGraphSchema::StaticClass()))
	{
		InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FDialogueGraphEditor::OnDialogueNodeDoubleClicked);
	}
}

//void FDialogueGraphEditor::StartEditingDefaults(bool bAutoFocus /*= true*/, bool bForceRefresh /*= false*/)
//{
//	if (DialogueBlueprint && DialogueBlueprint->DialogueTemplate)
//	{
//		UObject* DefaultObject = GetBlueprintObj()->GeneratedClass->GetDefaultObject();
//
//		// Update the details panel
//		FString Title;
//		DefaultObject->GetName(Title);
//		SKismetInspector::FShowDetailsOptions Options(FText::FromString(Title), bForceRefresh);
//		Options.bShowComponents = false;
//
//		Inspector->ShowDetailsForSingleObject(DialogueBlueprint->DialogueTemplate, Options);
//	}
//	else
//	{
//		FBlueprintEditor::StartEditingDefaults(bAutoFocus, bForceRefresh);
//	}
//}

void FDialogueGraphEditor::OnAddInputPin()
{
	FGraphPanelSelectionSet CurrentSelection;
	TSharedPtr<SGraphEditor> FocusedGraphEd = UpdateGraphEdPtr.Pin();
	if (FocusedGraphEd.IsValid())
	{
		CurrentSelection = FocusedGraphEd->GetSelectedNodes();
	}

	// Iterate over all nodes, and add the pin
	for (FGraphPanelSelectionSet::TConstIterator It(CurrentSelection); It; ++It)
	{
		//UBehaviorTreeDecoratorGraphNode_Logic* LogicNode = Cast<UBehaviorTreeDecoratorGraphNode_Logic>(*It);
		//if (LogicNode)
		//{
		//	const FScopedTransaction Transaction(LOCTEXT("AddInputPin", "Add Input Pin"));

		//	LogicNode->Modify();
		//	LogicNode->AddInputPin();

		//	const UEdGraphSchema* Schema = LogicNode->GetSchema();
		//	Schema->ReconstructNode(*LogicNode);
		//}
	}

	// Refresh the current graph, so the pins can be updated
	if (FocusedGraphEd.IsValid())
	{
		FocusedGraphEd->NotifyGraphChanged();
	}
}

bool FDialogueGraphEditor::CanAddInputPin() const
{
	return true;
}

void FDialogueGraphEditor::OnRemoveInputPin()
{

}

bool FDialogueGraphEditor::CanRemoveInputPin() const
{
	return true;
}

FText FDialogueGraphEditor::GetDialogueEditorTitle() const
{
	return FText::FromString(GetNameSafe(DialogueBlueprint));
}

FGraphAppearanceInfo FDialogueGraphEditor::GetDialogueGraphAppearance() const
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NARRATIVE DIALOGUE EDITOR");
	return AppearanceInfo;
}

bool FDialogueGraphEditor::CanAccessDialogueEditorMode() const
{
	return IsValid(DialogueBlueprint);
}

FText FDialogueGraphEditor::GetLocalizedMode(FName InMode)
{
	static TMap< FName, FText > LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(DialogueEditorMode, LOCTEXT("DialogueEditorMode", "Dialogue Graph"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	return *OutDesc;
}


UDialogueBlueprint* FDialogueGraphEditor::GetDialogueAsset() const
{
	return DialogueBlueprint;
}

TSharedRef<SWidget> FDialogueGraphEditor::SpawnProperties()
{
	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			DetailsView.ToSharedRef()
		];
}

void FDialogueGraphEditor::RegisterToolbarTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FDialogueGraphEditor::RestoreDialogueGraph()
{
	UDialogueGraph* MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);
	const bool bNewGraph = MyGraph == NULL;
	if (MyGraph == NULL)
	{
		DialogueBlueprint->DialogueGraph = FBlueprintEditorUtils::CreateNewGraph(DialogueBlueprint, TEXT("Dialogue Graph"), UDialogueGraph::StaticClass(), UDialogueGraphSchema::StaticClass());
		MyGraph = Cast<UDialogueGraph>(DialogueBlueprint->DialogueGraph);

		FBlueprintEditorUtils::AddUbergraphPage(DialogueBlueprint, MyGraph);

		// Initialize the behavior tree graph
		const UEdGraphSchema* Schema = MyGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*MyGraph);

		MyGraph->OnCreated();
	}
	else
	{
		MyGraph->OnLoaded();
	}

	MyGraph->Initialize();

	TSharedRef<FTabPayload_UObject> Payload = FTabPayload_UObject::Make(MyGraph);
	TSharedPtr<SDockTab> DocumentTab = DocumentManager->OpenDocument(Payload, bNewGraph ? FDocumentTracker::OpenNewDocument : FDocumentTracker::RestorePreviousDocument);

	if (DialogueBlueprint->LastEditedDocuments.Num() > 0)
	{
		TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(DocumentTab->GetContent());
		GraphEditor->SetViewLocation(DialogueBlueprint->LastEditedDocuments[0].SavedViewOffset, DialogueBlueprint->LastEditedDocuments[0].SavedZoomAmount);
	}
}

void FDialogueGraphEditor::OnDialogueNodeDoubleClicked(UEdGraphNode* Node)
{
	// read only
	if (!IsGraphEditable())
	{
		return;
	}
	
	if (UDialogueGraphNode* DNode = Cast<UDialogueGraphNode>(Node))
	{
		if (DNode->DialogueNode)
		{
			if (UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(DialogueBlueprint))
			{
				if (EventGraph->Nodes.Find(DNode->OnPlayedCustomNode) < 0)
				{
					if (UFunction* Func = DNode->FindFunction(GET_FUNCTION_NAME_CHECKED(UDialogueGraphNode, OnStartedOrFinished)))
					{
						const FScopedTransaction Transaction(NSLOCTEXT("DialogueGraphEditor", "DialogueGraphEditor_EventFromNode", "Create event for node"));
						DialogueBlueprint->Modify();
						
						//Try use the ID of the node if we gave it one prior to event add
						const FString NodeID = (DNode->DialogueNode->GetID() != NAME_None ? DNode->DialogueNode->GetID().ToString() : DNode->DialogueNode->GetName());
						FString OnEnteredFuncName = "OnDialogueNode Started/Finished Playing - " + NodeID;

						if (UK2Node_CustomEvent* OnPlayedEvent = UK2Node_CustomEvent::CreateFromFunction(EventGraph->GetGoodPlaceForNewNode(), EventGraph, OnEnteredFuncName, Func, false))
						{
							DNode->DialogueNode->OnPlayNodeFuncName = FName(OnEnteredFuncName);
							DNode->OnPlayedCustomNode = OnPlayedEvent;

							OnPlayedEvent->NodeComment = FString::Printf(TEXT("This event will automatically be called when this dialogue line starts/finishes. Use the bStarted param to check which occured."));
							OnPlayedEvent->SetMakeCommentBubbleVisible(true);

							OnPlayedEvent->bCanRenameNode = OnPlayedEvent->bIsEditable = false;

							//Jump to our newly created event! 
							JumpToNode(OnPlayedEvent, false);
						}
					}
				}
				else if(DNode->OnPlayedCustomNode)
				{
					JumpToNode(DNode->OnPlayedCustomNode, false);
				}
			}
		}
	}
}

void FDialogueGraphEditor::SaveEditedObjectState()
{
	DialogueBlueprint->LastEditedDocuments.Empty();
	DocumentManager->SaveAllState();
}

void FDialogueGraphEditor::OnCreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = FocusedGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			if (const UEdGraphSchema* Schema = Graph->GetSchema())
			{
				if (Schema->IsA(UEdGraphSchema_K2::StaticClass()) || Schema->IsA(UDialogueGraphSchema::StaticClass()))
				{
					FEdGraphSchemaAction_K2AddComment CommentAction;
					CommentAction.PerformAction(Graph, nullptr, GraphEditor->GetPasteLocation2f());
				}
			}
		}
	}
}

void FDialogueGraphEditor::AddNodesFromAssets(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* DestinationGraph, UEdGraphNode* SelectedNode) const
{
	if (Assets.IsEmpty() || !DestinationGraph || !GEditor)
	{
		return;
	}

	TSharedPtr<SGraphEditor> FocusedGraph = FocusedGraphEdPtr.Pin();
	if (!FocusedGraph.IsValid())
	{
		return;
	}

	// get dialogue
	UDialogue* Dialogue = GetDialogueAsset()? Cast<UDialogue>(GetDialogueAsset()->GeneratedClass->GetDefaultObject()) : nullptr;
		
	const FScopedTransaction Transaction(NSLOCTEXT("FDialogueGraphEditor", "FDialogueGraphEditor_NodeFromAssets", "Create nodes from assets"));
	DestinationGraph->Modify();

	// pin to connect to
	UEdGraphPin* ConnectedPin = SelectedNode? SelectedNode->FindPin(NAME_None, EGPD_Output) : nullptr;

	FocusedGraph->ClearSelectionSet();
	
	FVector2D NewNodePos = GraphPosition;
	for (const FAssetData& Asset : Assets)
	{
		// currently sound waves are only supported
		if (USoundWave* SoundWave = Cast<USoundWave>(Asset.GetAsset()))
		{			
			// check if there is a speaker in the asset name
			const FString WavName = SoundWave->GetFName().ToString();
			const FSpeakerInfo* Speaker = Dialogue->Speakers.FindByPredicate([&WavName](const FSpeakerInfo& SpeakerInfo)
			{
				return WavName.Contains(SpeakerInfo.SpeakerID.ToString());
			});

			// create new graph node
			const UClass* NodeClass = (Speaker && Speaker->bIsPlayer) || WavName.Contains("Player")? UDialogueGraphNode_Player::StaticClass() : UDialogueGraphNode_NPC::StaticClass();
			UDialogueGraphNode* NewGraphNode = NewObject<UDialogueGraphNode>(DestinationGraph, NodeClass);
			if (!NewGraphNode)
			{
				continue;
			}

			FDialogueSchemaAction_NewNode AddNewNode;
			AddNewNode.NodeTemplate = NewGraphNode;

			// create node and assign sound wave
			AddNewNode.PerformAction(DestinationGraph, ConnectedPin, NewNodePos);
			NewGraphNode->DialogueNode->Line.DialogueSound = SoundWave;

			// offset next node position
			NewNodePos.X += 500.0f;

			// when node is a npc node, set the speaker to the found speaker if any
			if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(NewGraphNode->DialogueNode))
			{
				FName SpeakerID = Dialogue->Speakers.IsValidIndex(0)? Dialogue->Speakers[0].SpeakerID : NAME_None;

				if (Speaker)
				{
					SpeakerID = Speaker->SpeakerID;
				}

				NPCNode->SetSpeakerID(SpeakerID);
			}

			// generate an id
			NewGraphNode->DialogueNode->GenerateIDFromText();
			
			DestinationGraph->NotifyGraphChanged();
			
			// add new node to selection
			FocusedGraph->SetNodeSelection(NewGraphNode, true);
			
			GetDialogueAsset()->PostEditChange();
			GetDialogueAsset()->MarkPackageDirty();
			
		}
	}
}

void FDialogueGraphEditor::SetGraphEditable(const bool Editable)
{
	if (IsValid(DialogueBlueprint) && IsValid(DialogueBlueprint->DialogueGraph))
	{
		DialogueBlueprint->DialogueGraph->bEditable = Editable;
	}
}

bool FDialogueGraphEditor::IsGraphEditable() const
{
	return IsValid(DialogueBlueprint) && IsValid(DialogueBlueprint->DialogueGraph)? DialogueBlueprint->DialogueGraph->bEditable : false;
}

void FDialogueGraphEditor::JumpOrAddDialogueNode(FName NodeID)
{
	// get dialogue
	UDialogue* Dialogue = GetDialogueAsset()? Cast<UDialogue>(GetDialogueAsset()->GeneratedClass->GetDefaultObject()) : nullptr;
	UEdGraph* DestinationGraph = Dialogue? GetDialogueAsset()->DialogueGraph : nullptr;
	if (!DestinationGraph)
	{
		return;
	}

	// check if a node with the state ID already exists
	TObjectPtr<UEdGraphNode>* ExistingNode = DialogueBlueprint->DialogueGraph->Nodes.FindByPredicate([this, &NodeID](const UEdGraphNode* Node)
	{
		const UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
		return DialogueNode? DialogueNode->DialogueNode->GetID() == NodeID : false;
	});

	// when no existing node was found, create a new one
	if (!ExistingNode)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("FDialogueGraphEditor", "FDialogueGraphEditor_NewNodeFromQuest", "Create node from Quest State"));
		DialogueBlueprint->Modify();
		DestinationGraph->Modify();
	
		// create new graph node
		UDialogueGraphNode* NewGraphNode = NewObject<UDialogueGraphNode>(DestinationGraph, UDialogueGraphNode_NPC::StaticClass());
		if (!NewGraphNode)
		{
			return;
		}

		FDialogueSchemaAction_NewNode AddNewNode;
		AddNewNode.NodeTemplate = NewGraphNode;

		// create node
		AddNewNode.PerformAction(DestinationGraph, nullptr, {});

		// set NPC speaker
		if (UDialogueNode_NPC* NPCNode = Cast<UDialogueNode_NPC>(NewGraphNode->DialogueNode))
		{
			const FName SpeakerID = Dialogue->Speakers.IsValidIndex(0)? Dialogue->Speakers[0].SpeakerID : NAME_None;
			NPCNode->SetSpeakerID(SpeakerID);
		}
		
		// generate an id
		NewGraphNode->DialogueNode->SetID(NodeID);
		
		DestinationGraph->NotifyGraphChanged();
						
		GetDialogueAsset()->PostEditChange();
		GetDialogueAsset()->MarkPackageDirty();
		
		JumpToNode(NewGraphNode);
	}
	else
	{
		JumpToNode(*ExistingNode);
	}
}

void FDialogueGraphEditor::OnBeginPIE(bool bSimulating)
{
	SetGraphEditable(false);
}

void FDialogueGraphEditor::OnEndPIE(bool bSimulating)
{
	SetGraphEditable(true);
}

TSharedRef<class SGraphEditor> FDialogueGraphEditor::CreateDialogueGraphEditorWidget(UEdGraph* InGraph)
{
	check(InGraph);

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FDialogueGraphEditor::OnSelectedNodesChanged);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FDialogueGraphEditor::OnNodeDoubleClicked);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FDialogueGraphEditor::OnNodeTitleCommitted);

	// Make title bar
	TSharedRef<SWidget> TitleBarWidget =
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush(TEXT("Graph.TitleBackground")))
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.FillWidth(1.f)
		[
			SNew(STextBlock)
			.Text(this, &FDialogueGraphEditor::GetDialogueEditorTitle)
			.TextStyle(FAppStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
		]
		];

	// Make full graph editor
	const bool bGraphIsEditable = InGraph->bEditable;
	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(bGraphIsEditable)
		.Appearance(this, &FDialogueGraphEditor::GetDialogueGraphAppearance)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(InGraph)
		.GraphEvents(InEvents);

}

void FDialogueGraphEditor::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = false;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(NULL);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &FDialogueGraphEditor::OnFinishedChangingProperties);
}

void FDialogueGraphEditor::ExtendMenu()
{

}

void FDialogueGraphEditor::ExtendToolbar()
{

}

//void FDialogueGraphEditor::BindCommonCommands()
//{
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().ShowDialogueDetails,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::ShowDialogueDetails),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanShowDialogueDetails));
//
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().ViewTutorial,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::OpenNarrativeTutorialsInBrowser),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanOpenNarrativeTutorialsInBrowser));
//
//	ToolkitCommands->MapAction(FDialogueEditorCommands::Get().QuickAddNode,
//		FExecuteAction::CreateSP(this, &FDialogueGraphEditor::QuickAddNode),
//		FCanExecuteAction::CreateSP(this, &FDialogueGraphEditor::CanQuickAddNode));
//}

#undef LOCTEXT_NAMESPACE