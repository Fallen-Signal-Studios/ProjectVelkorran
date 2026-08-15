// Copyright Narrative Tools 2025.

#include "GraphPin_DialogueNodeSelector.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphEditor.h"
#include "DialogueGraphNode.h"
#include "Tales/Dialogue.h"

void SGraphPin_DialogueNodeSelector::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin_NodeSelectorBase::Construct(SGraphPin_NodeSelectorBase::FArguments(), InGraphPinObj);
}

UClass* SGraphPin_DialogueNodeSelector::GetMetaClass()
{
	return UDialogue::StaticClass();
}

const UClass* SGraphPin_DialogueNodeSelector::SelectedClass() const
{
	// if already loaded, return the class object, however if not loaded then load it
	return SelectorStruct.Asset.LoadSynchronous();
}

void SGraphPin_DialogueNodeSelector::FillAssetIfNeeded()
{
	const UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(GraphPinObj->GetOuter()->GetOutermostObject());
	if (DialogueBlueprint && !SelectorStruct.Asset)
	{
		SelectorStruct.Asset = DialogueBlueprint->GeneratedClass;
	}
}

void SGraphPin_DialogueNodeSelector::FillNodeUserIDList()
{
	if (const UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(UBlueprint::GetBlueprintFromClass(SelectedClass())))
	{
		for (const UDialogueNode* DialogueNode : DialogueBlueprint->DialogueTemplate->GetNodes())
		{
			NodeUserIDList.Add(MakeShared<FName>(DialogueNode->GetID()));
		}
	}
}

void SGraphPin_DialogueNodeSelector::OnSetClass(const UClass* SetClass)
{
	SelectorStruct.Asset = SetClass;
	
	SGraphPin_NodeSelectorBase::OnSetClass(SetClass);
}

bool SGraphPin_DialogueNodeSelector::IsValidGraphNode() const
{
	bool FoundNodeID = false;
	if (UDialogueBlueprint* DialogueBlueprint = GetBlueprintFromClass<UDialogueBlueprint>(SelectedClass()))
	{
		FoundNodeID = DialogueBlueprint->DialogueGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
		{
			const UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
			return DialogueNode? DialogueNode->DialogueNode->GetID() == SelectorStruct.NodeID : false;
		}) != nullptr;
	}	
	return FoundNodeID;
}

bool SGraphPin_DialogueNodeSelector::IsValidNodeID(const FName& NameOverride) const
{
	bool FoundNodeID = false;
	if (UDialogueBlueprint* DialogueBlueprint = GetBlueprintFromClass<UDialogueBlueprint>(SelectedClass()))
	{
		const FName NodeID = NameOverride.IsNone()? SelectorStruct.NodeID : NameOverride;
		FoundNodeID = DialogueBlueprint->DialogueTemplate->GetNodes().FindByPredicate([&NodeID](const UDialogueNode* Node)
		{
			return Node->GetID() == NodeID;
		}) != nullptr;
	}
	return FoundNodeID;
}

FText SGraphPin_DialogueNodeSelector::GetNodeUserIDText() const
{
	return FText::FromName(SelectorStruct.NodeID);
}

void SGraphPin_DialogueNodeSelector::BrowseToNode(UObject* Object)
{
	SGraphPin_NodeSelectorBase::BrowseToNode(Object);
	
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		// find graph editor for bp
		UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(Object);
		if (FDialogueGraphEditor* DialogueGraphEditor = static_cast<FDialogueGraphEditor*>(AssetEditorSubsystem->FindEditorForAsset(DialogueBlueprint, true)))
		{
			// attempt to find node
			TObjectPtr<UEdGraphNode>* EdNode = DialogueBlueprint->DialogueGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
			{
				const UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
				return DialogueNode ? DialogueNode->DialogueNode->GetID() == SelectorStruct.NodeID : false;
			});

			// try jump to node
			if (EdNode)
			{
				DialogueGraphEditor->JumpToNode(*EdNode);
			}
		}
	}
}

TSharedPtr<SGraphPin> FPinFactoryDialogueNodeSelector::CreatePin(UEdGraphPin* InPin) const
{
	if (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
		InPin->PinType.PinSubCategoryObject == FDialogueNodeSelector::StaticStruct())
	{
		return SNew(SGraphPin_DialogueNodeSelector, InPin);
	}
	return nullptr;
}
