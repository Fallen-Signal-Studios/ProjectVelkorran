// Copyright Narrative Tools 2025.

#include "QuestNodeSelectorPropertyCustomization.h"
#include "QuestBlueprint.h"
#include "QuestGraphEditor.h"
#include "QuestGraphNode_Action.h"
#include "QuestGraphNode_State.h"
#include "Tales/NarrativeNodeSelector.h"
#include "Tales/Quest.h"

UClass* FQuestStateSelectorPropertyTypeCustomization::GetSetAsset() const
{
	// if already loaded, return the class object, however if not loaded then load it
	return SelectorStruct? SelectorStruct->Asset.LoadSynchronous() : nullptr;
}

void FQuestStateSelectorPropertyTypeCustomization::SetStructPtr(TSharedRef<IPropertyHandle> PropertyHandle)
{
	SelectorStruct = GetStructFromHandle<FQuestStateSelector>(PropertyHandle);
}

void FQuestStateSelectorPropertyTypeCustomization::FillAssetIfNeeded()
{
	if (GetSetAsset())
	{
		TArray<UObject*> OuterObjects;
		AssetPropertyHandle->GetParentHandle()->GetOuterObjects(OuterObjects);
		for (UObject* Outer : OuterObjects)
		{
			UObject* DecidedOuter = Outer;

			// if not in a bp directly, look outwards to see if it is in a Quest bp at all
			if (!DecidedOuter->IsA<UQuest>())
			{
				DecidedOuter = Outer->GetOutermostObject();
				if (UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(DecidedOuter))
				{
					DecidedOuter = QuestBlueprint->GeneratedClass.GetDefaultObject();
				}
			}
			
			if (UQuest* QuestAsset = Cast<UQuest>(DecidedOuter))
			{
				AssetPropertyHandle->SetValue(QuestAsset->GetClass());
				break;
			}
		}
	}
}

void FQuestStateSelectorPropertyTypeCustomization::FillNodeUserIDList()
{
	if (!SelectorStruct)
	{
		return;
	}

	if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(GetSetAsset()))
	{
		TArray<FName> NodeIDs;
		for (UQuestState* QuestStateNode : QuestBlueprint->QuestTemplate->GetStates())
		{
			NodeUserIDList.Add(MakeShared<FName>(QuestStateNode->GetID()));
		}
	}
}

FNodeIDSelector* FQuestStateSelectorPropertyTypeCustomization::GetSelectorStruct()
{
	return SelectorStruct;
}

FNodeIDSelector* FQuestStateSelectorPropertyTypeCustomization::GetSelectorStruct() const
{
	return SelectorStruct;
}

bool FQuestStateSelectorPropertyTypeCustomization::IsValidGraphNode() const
{
	bool FoundNodeID = false;
	if (SelectorStruct)
	{
		if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(GetSetAsset()))
		{
			TArray<UQuestGraphNode_State*> States;
			QuestBlueprint->QuestGraph->GetNodesOfClass<UQuestGraphNode_State>(States);
			FoundNodeID = States.FindByPredicate([this](const UQuestGraphNode_State* State)
			{
				return State->State->GetID() == SelectorStruct->NodeID;
			}) != nullptr;
		}
	}
	
	return FoundNodeID;
}

bool FQuestStateSelectorPropertyTypeCustomization::IsValidNodeID(const FName& NameOverride) const
{
	bool FoundNodeID = false;
	if (SelectorStruct)
	{
		if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(GetSetAsset()))
		{
			const FName NodeID = NameOverride.IsNone()? SelectorStruct->NodeID : NameOverride;
			FoundNodeID = QuestBlueprint->QuestTemplate->GetStates().FindByPredicate([&NodeID](const UQuestState* State)
			{
				return State->GetID() == NodeID;
			}) != nullptr;
		}
	}

	return FoundNodeID;
}

void FQuestStateSelectorPropertyTypeCustomization::BrowseToNode(UObject* Object)
{
	FNodeSelectorPropertyTypeCustomizationBase::BrowseToNode(Object);
	
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		// find graph editor for bp
		UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(Object);
		if (FQuestGraphEditor* QuestGraphEditor = static_cast<FQuestGraphEditor*>(AssetEditorSubsystem->FindEditorForAsset(QuestBlueprint, true)))
		{
			// attempt to find node
			TObjectPtr<UEdGraphNode>* EdNode = QuestBlueprint->QuestGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
			{
				const UQuestGraphNode* QuestNode = Cast<UQuestGraphNode>(Node);
				return QuestNode ? QuestNode->QuestNode->GetID() == SelectorStruct->NodeID : false;
			});

			// try jump to node
			if (EdNode)
			{
				QuestGraphEditor->JumpToNode(*EdNode);
			}
		}
	}
}

UClass* FQuestBranchSelectorPropertyTypeCustomization::GetSetAsset() const
{
	// if already loaded, return the class object, however if not loaded then load it
	return SelectorStruct? SelectorStruct->Asset.LoadSynchronous() : nullptr;
}

void FQuestBranchSelectorPropertyTypeCustomization::SetStructPtr(TSharedRef<IPropertyHandle> PropertyHandle)
{
	SelectorStruct = GetStructFromHandle<FQuestBranchSelector>(PropertyHandle);
}

void FQuestBranchSelectorPropertyTypeCustomization::FillAssetIfNeeded()
{
	if (GetSetAsset())
	{
		TArray<UObject*> OuterObjects;
		AssetPropertyHandle->GetParentHandle()->GetOuterObjects(OuterObjects);
		for (UObject* Outer : OuterObjects)
		{
			UObject* DecidedOuter = Outer;

			// if not in a bp directly, look outwards to see if it is in a Quest bp at all
			if (!DecidedOuter->IsA<UQuest>())
			{
				DecidedOuter = Outer->GetOutermostObject();
				if (UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(DecidedOuter))
				{
					DecidedOuter = QuestBlueprint->GeneratedClass.GetDefaultObject();
				}
			}
			
			if (UQuest* QuestAsset = Cast<UQuest>(DecidedOuter))
			{
				AssetPropertyHandle->SetValue(QuestAsset->GetClass());
				break;
			}
		}
	}
}

void FQuestBranchSelectorPropertyTypeCustomization::FillNodeUserIDList()
{
	if (!SelectorStruct)
	{
		return;
	}

	if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(GetSetAsset()))
	{
		TArray<FName> NodeIDs;
		for (UQuestBranch* QuestBranchNode : QuestBlueprint->QuestTemplate->GetBranches())
		{
			NodeUserIDList.Add(MakeShared<FName>(QuestBranchNode->GetID()));
		}
	}
}

FNodeIDSelector* FQuestBranchSelectorPropertyTypeCustomization::GetSelectorStruct()
{
	return SelectorStruct;
}

FNodeIDSelector* FQuestBranchSelectorPropertyTypeCustomization::GetSelectorStruct() const
{
	return SelectorStruct;
}

bool FQuestBranchSelectorPropertyTypeCustomization::IsValidGraphNode() const
{
	bool FoundNodeID = false;
	if (SelectorStruct)
	{
		if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(GetSetAsset()))
		{
			TArray<UQuestGraphNode_Action*> Actions;
			QuestBlueprint->QuestGraph->GetNodesOfClass<UQuestGraphNode_Action>(Actions);
			FoundNodeID = Actions.FindByPredicate([this](const UQuestGraphNode_Action* Action)
			{
				return Action->Branch->GetID() == SelectorStruct->NodeID;
			}) != nullptr;
		}
	}
	
	return FoundNodeID;
}

bool FQuestBranchSelectorPropertyTypeCustomization::IsValidNodeID(const FName& NameOverride) const
{
	bool FoundNodeID = false;
	if (SelectorStruct)
	{
		if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(GetSetAsset()))
		{
			const FName NodeID = NameOverride.IsNone()? SelectorStruct->NodeID : NameOverride;
			FoundNodeID = QuestBlueprint->QuestTemplate->GetBranches().FindByPredicate([&NodeID](const UQuestBranch* State)
			{
				return State->GetID() == NodeID;
			}) != nullptr;
		}
	}

	return FoundNodeID;
}

void FQuestBranchSelectorPropertyTypeCustomization::BrowseToNode(UObject* Object)
{
	FNodeSelectorPropertyTypeCustomizationBase::BrowseToNode(Object);
	
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		// find graph editor for bp
		UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(Object);
		if (FQuestGraphEditor* QuestGraphEditor = static_cast<FQuestGraphEditor*>(AssetEditorSubsystem->FindEditorForAsset(QuestBlueprint, true)))
		{
			// attempt to find node
			TObjectPtr<UEdGraphNode>* EdNode = QuestBlueprint->QuestGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
			{
				const UQuestGraphNode* QuestNode = Cast<UQuestGraphNode>(Node);
				return QuestNode ? QuestNode->QuestNode->GetID() == SelectorStruct->NodeID : false;
			});

			// try jump to node
			if (EdNode)
			{
				QuestGraphEditor->JumpToNode(*EdNode);
			}
		}
	}
}