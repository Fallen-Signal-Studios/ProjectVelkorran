// Copyright Narrative Tools 2025.

#include "GraphPin_QuestNodeSelector.h"
#include "QuestBlueprint.h"
#include "QuestGraphEditor.h"
#include "QuestGraphNode.h"
#include "Tales/Quest.h"

void SGraphPin_QuestNodeSelector::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin_NodeSelectorBase::Construct(SGraphPin_NodeSelectorBase::FArguments(), InGraphPinObj);

	// workaround to make sure states are selectable when needed!
	if (InGraphPinObj->PinType.PinSubCategoryObject == FQuestStateSelector::StaticStruct())
	{
		bSelectStates = true;
	}
}

UClass* SGraphPin_QuestNodeSelector::GetMetaClass()
{
	return UQuest::StaticClass();
}

const UClass* SGraphPin_QuestNodeSelector::SelectedClass() const
{
	// if already loaded, return the class object, however if not loaded then load it
	return SelectorStruct.Asset.LoadSynchronous();
}

void SGraphPin_QuestNodeSelector::FillAssetIfNeeded()
{
	const UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(GraphPinObj->GetOuter()->GetOutermostObject());
	if (QuestBlueprint && !SelectorStruct.Asset)
	{
		SelectorStruct.Asset = QuestBlueprint->GeneratedClass;
	}
}

void SGraphPin_QuestNodeSelector::FillNodeUserIDList()
{
	if (const UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(UBlueprint::GetBlueprintFromClass(SelectedClass())))
	{
		if (bSelectStates)
		{
			for (const UQuestState* QuestStateNode : QuestBlueprint->QuestTemplate->GetStates())
			{				
				NodeUserIDList.Add(MakeShared<FName>(QuestStateNode->GetID()));
			}
		}
		else
		{
			for (const UQuestBranch* QuestBranchNode : QuestBlueprint->QuestTemplate->GetBranches())
			{				
				NodeUserIDList.Add(MakeShared<FName>(QuestBranchNode->GetID()));
			}
		}
	}
}

void SGraphPin_QuestNodeSelector::OnSetClass(const UClass* SetClass)
{
	SelectorStruct.Asset = SetClass;
	
	SGraphPin_NodeSelectorBase::OnSetClass(SetClass);
}

bool SGraphPin_QuestNodeSelector::IsValidGraphNode() const
{
	bool FoundNodeID = false;
	if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(SelectedClass()))
	{
		FoundNodeID = QuestBlueprint->QuestGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
		{
			const UQuestGraphNode* QuestGraphNode = Cast<UQuestGraphNode>(Node);
			return QuestGraphNode? QuestGraphNode->QuestNode->GetID() == SelectorStruct.NodeID : false;
		}) != nullptr;
	}	
	return FoundNodeID;
}

bool SGraphPin_QuestNodeSelector::IsValidNodeID(const FName& NameOverride) const
{
	bool FoundNodeID = false;
	if (UQuestBlueprint* QuestBlueprint = GetBlueprintFromClass<UQuestBlueprint>(SelectedClass()))
	{
		const FName NodeID = NameOverride.IsNone()? SelectorStruct.NodeID : NameOverride;
		FoundNodeID = QuestBlueprint->QuestTemplate->GetNodes().FindByPredicate([&NodeID](const UQuestNode* Node)
		{
			return Node->GetID() == NodeID;
		}) != nullptr;
	}
	return FoundNodeID;
}

FText SGraphPin_QuestNodeSelector::GetNodeUserIDText() const
{
	return FText::FromName(SelectorStruct.NodeID);
}

void SGraphPin_QuestNodeSelector::BrowseToNode(UObject* Object)
{
	SGraphPin_NodeSelectorBase::BrowseToNode(Object);
	
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		// find graph editor for bp
		UQuestBlueprint* QuestBlueprint = Cast<UQuestBlueprint>(Object);
		if (FQuestGraphEditor* QuestGraphEditor = static_cast<FQuestGraphEditor*>(AssetEditorSubsystem->FindEditorForAsset(QuestBlueprint, true)))
		{
			// attempt to find node
			TObjectPtr<UEdGraphNode>* EdNode = QuestBlueprint->QuestGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
			{
				const UQuestGraphNode* DialogueNode = Cast<UQuestGraphNode>(Node);
				return DialogueNode ? DialogueNode->QuestNode->GetID() == SelectorStruct.NodeID : false;
			});

			// try jump to node
			if (EdNode)
			{
				QuestGraphEditor->JumpToNode(*EdNode);
			}
		}
	}
}

TSharedPtr<SGraphPin> FPinFactoryQuestNodeSelector::CreatePin(UEdGraphPin* InPin) const
{
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
		(InPin->PinType.PinSubCategoryObject == FQuestStateSelector::StaticStruct() ||
		 InPin->PinType.PinSubCategoryObject == FQuestBranchSelector::StaticStruct() ))
	{
		return SNew(SGraphPin_QuestNodeSelector, InPin);
	}
	return nullptr;
}
