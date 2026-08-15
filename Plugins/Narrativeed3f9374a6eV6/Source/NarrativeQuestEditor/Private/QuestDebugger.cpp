// Copyright Narrative Tools 2025.

#include "QuestDebugger.h"

#include "QuestEditorCommands.h"
#include "QuestGraph.h"
#include "QuestGraphEditor.h"
#include "QuestGraphNode_Root.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "Tales/Quest.h"

FQuestDebugger::FQuestDebugger()
	: bIsPIEActive(false), QuestBlueprintAsset(nullptr), ActiveQuest(nullptr), ActiveState(nullptr)
{
	FEditorDelegates::BeginPIE.AddRaw(this, &FQuestDebugger::OnBeginPIE);
	FEditorDelegates::EndPIE.AddRaw(this, &FQuestDebugger::OnEndPIE);
}

FQuestDebugger::~FQuestDebugger()
{
	ResetActiveNode();
	TalesComponentInstance.Reset();
	EditorOwner.Reset();
	QuestBlueprintAsset.Reset();
	ActiveQuest.Reset();
	ActiveState = nullptr;
}

void FQuestDebugger::Tick(float DeltaTime)
{
#if WITH_EDITORONLY_DATA
	if (ActiveQuest.IsValid() && IsDebuggerReady())
	{
		UQuestState* AssetCurrentState = ActiveQuest->GetCurrentState();
		const FName NewNodeID = AssetCurrentState->GetID();
		if (ActiveState.IsValid())
		{
			if (ActiveState != AssetCurrentState)
			{
				const FName CurrentNodeID = ActiveState->GetID();
				SetStateActive(CurrentNodeID, false);
			
				SetStateActive(NewNodeID, true);
				ActiveState = AssetCurrentState;
			}
		}
		else
		{
			SetStateActive(NewNodeID, true);
			ActiveState = AssetCurrentState;
		}
	}
#endif
}

bool FQuestDebugger::IsTickable() const
{
	return IsDebuggerReady();
}

void FQuestDebugger::Setup(UQuestBlueprint* QuestBlueprint, TSharedRef<FQuestGraphEditor> OwnerEditor)
{
	EditorOwner = OwnerEditor;
	QuestBlueprintAsset = QuestBlueprint;
	
	if (!IsPIENotSimulating())
	{
		// resolve tales comp instance when opening during PIE
		TalesComponentInstance = UNarrativeFunctionLibrary::GetTalesComponent(GEditor->PlayWorld);

		// resolve live quest object
		if (TalesComponentInstance.IsValid())
		{
			for (UQuest* Quest : TalesComponentInstance->GetAllQuests())
			{
				if (Quest->GetClass() == QuestBlueprintAsset->GeneratedClass)
				{
					ActiveQuest = Quest;
				}
			}
		}
		
		bIsPIEActive = true;
	}
}

void FQuestDebugger::BindCommands(TSharedRef<FUICommandList> EditorCommandList)
{
	EditorCommandList->MapAction(FQuestEditorCommands::Get().JumpToActiveNode,
		FExecuteAction::CreateSP(this, &FQuestDebugger::DebuggerJumpToNode),
		FCanExecuteAction::CreateSP(this, &FQuestDebugger::IsDebuggerReady));
}

void FQuestDebugger::RegisterDebuggerToolbar(UToolMenu* ToolMenu)
{
	FToolMenuSection* Section = ToolMenu->FindSection("Debugging");
	if (!Section)
	{
		return;
	}

	// toolbar button
	FToolMenuEntry NarrativeToolbarEntry = FToolMenuEntry::InitToolBarButton(
		FQuestEditorCommands::Get().JumpToActiveNode,
		FText::GetEmpty(),
		INVTEXT("Jump to the current active node"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Profiler.Misc.SortDescending")
	);

	Section->AddEntry(NarrativeToolbarEntry);
}

bool FQuestDebugger::IsPIENotSimulating()
{
	return !GEditor->bIsSimulatingInEditor && (GEditor->PlayWorld == nullptr);
}

void FQuestDebugger::OnBeginPIE(const bool bIsSimulating)
{
	bIsPIEActive = true;

	ResetActiveNode();

	FQuestDelegates::OnQuestStarted.RemoveAll(this);
	FQuestDelegates::OnQuestEnd.RemoveAll(this);
	FQuestDelegates::OnQuestStarted.AddRaw(this, &FQuestDebugger::OnQuestStarted);
	FQuestDelegates::OnQuestEnd.AddRaw(this, &FQuestDebugger::OnQuestEnd);
}

void FQuestDebugger::OnEndPIE(const bool bIsSimulating)
{
	bIsPIEActive = false;
	FQuestDelegates::OnQuestStarted.RemoveAll(this);
	FQuestDelegates::OnQuestEnd.RemoveAll(this);

	ResetActiveNode();
}

void FQuestDebugger::OnQuestStarted(UTalesComponent* TalesComponent, UQuest* Quest, const FName& OptionalStartFromID)
{
	/*UQuestBlueprint* QuestBP = QuestBlueprintAsset.Get();
	if (!IsValid(QuestBP) || !IsValid(QuestBP->GeneratedClass) || !IsValid(Quest))
	{
		// early back out
		ResetActiveNode();
		return;
	}
	
	if (Quest->GetClass() == QuestBP->GeneratedClass)
	{
		if (!TalesComponentInstance.IsValid())
		{
			TalesComponentInstance = TalesComponent;
		}
		
		ActiveQuest = Quest;
	}*/
}

void FQuestDebugger::OnQuestEnd(UTalesComponent* TalesComponent, UQuest* Quest)
{
	if (TalesComponentInstance.IsValid() && Quest->GetClass() == QuestBlueprintAsset->GeneratedClass)
	{
		ResetActiveNode();
	}
}

void FQuestDebugger::DebuggerJumpToNode()
{
	if (IsDebuggerReady() && EditorOwner.IsValid() && QuestBlueprintAsset.IsValid() && ActiveState.IsValid())
	{
		UQuestGraph* QuestGraph = Cast<UQuestGraph>(QuestBlueprintAsset->QuestGraph);

		TArray<UQuestGraphNode_State*> States;
		QuestGraph->GetNodesOfClass<UQuestGraphNode_State>(States);
		
		const FName StateNodeID = ActiveState->GetID();
		for (UQuestGraphNode_State* State : States)
		{
			if (State->QuestNode->GetID() == StateNodeID)
			{
				EditorOwner.Pin()->JumpToNode(State);
			}
		}		
	}
}

void FQuestDebugger::SetStateActive(const FName& StateNodeID, const bool Active)
{
	if (!QuestBlueprintAsset.IsValid())
	{
		return;
	}

	UQuestGraph* QuestGraph = Cast<UQuestGraph>(QuestBlueprintAsset->QuestGraph);

	TArray<UQuestGraphNode_State*> States;
	QuestGraph->GetNodesOfClass<UQuestGraphNode_State>(States);

	for (UQuestGraphNode_State* State : States)
	{
		if (State->QuestNode->GetID() == StateNodeID)
		{
			State->bActiveNode = Active;
			return;
		}
	}
}

void FQuestDebugger::ResetActiveNode()
{
	if (ActiveState.IsValid())
	{
		SetStateActive(ActiveState->GetID(), false);
	}
}

