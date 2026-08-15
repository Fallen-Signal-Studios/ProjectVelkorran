// Copyright Narrative Tools 2025.

#include "DialogueNodeSelectorPropertyCustomization.h"
#include "DialogueBlueprint.h"
#include "DialogueGraphEditor.h"
#include "DialogueGraphNode.h"
#include "Tales/Dialogue.h"
#include "Tales/DialogueSM.h"
#include "Tales/NarrativeNodeSelector.h"

UClass* FDialogueNodeSelectorPropertyTypeCustomization::GetSetAsset() const
{
	// if already loaded, return the class object, however if not loaded then load it
	return SelectorStruct? SelectorStruct->Asset.LoadSynchronous() : nullptr;
}

void FDialogueNodeSelectorPropertyTypeCustomization::SetStructPtr(TSharedRef<IPropertyHandle> PropertyHandle)
{
	SelectorStruct = GetStructFromHandle<FDialogueNodeSelector>(PropertyHandle);
}

void FDialogueNodeSelectorPropertyTypeCustomization::FillAssetIfNeeded()
{
	if (!GetSetAsset())
	{
		TArray<UObject*> OuterObjects;
		AssetPropertyHandle->GetParentHandle()->GetOuterObjects(OuterObjects);
		for (UObject* Outer : OuterObjects)
		{
			UObject* DecidedOuter = Outer;

			// if not in a bp directly, look outwards to see if it is in a dialogue bp at all
			if (!DecidedOuter->IsA<UDialogue>())
			{
				DecidedOuter = Outer->GetOutermostObject();
				if (UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(DecidedOuter))
				{
					DecidedOuter = DialogueBlueprint->GeneratedClass.GetDefaultObject();
				}
			}
			
			if (UDialogue* DialogueAsset = Cast<UDialogue>(DecidedOuter))
			{
				AssetPropertyHandle->SetValue(DialogueAsset->GetClass());
				break;
			}
		}
	}
}

void FDialogueNodeSelectorPropertyTypeCustomization::FillNodeUserIDList()
{
	if (SelectorStruct)
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(GetSetAsset());
		if (UDialogueBlueprint* DialogueBlueprint = Cast<UDialogueBlueprint>(BP))
		{
			TArray<FName> NodeIDs;
			for (const UDialogueNode* DialogueNode : DialogueBlueprint->DialogueTemplate->GetNodes())
			{
				NodeUserIDList.Add(MakeShared<FName>(DialogueNode->GetID()));
			}
		}
	}
}

FNodeIDSelector* FDialogueNodeSelectorPropertyTypeCustomization::GetSelectorStruct()
{
	return SelectorStruct;
}

FNodeIDSelector* FDialogueNodeSelectorPropertyTypeCustomization::GetSelectorStruct() const
{
	return SelectorStruct;
}

bool FDialogueNodeSelectorPropertyTypeCustomization::IsValidGraphNode() const
{
	bool FoundNodeID = false;
	if (SelectorStruct)
	{
		if (UDialogueBlueprint* DialogueBlueprint = GetBlueprintFromClass<UDialogueBlueprint>(GetSetAsset()))
		{
			FoundNodeID = DialogueBlueprint->DialogueGraph->Nodes.FindByPredicate([this](const UEdGraphNode* Node)
			{
				const UDialogueGraphNode* DialogueNode = Cast<UDialogueGraphNode>(Node);
				return DialogueNode? DialogueNode->DialogueNode->GetID() == SelectorStruct->NodeID : false;
			}) != nullptr;
		}
	}	
	return FoundNodeID;
}

bool FDialogueNodeSelectorPropertyTypeCustomization::IsValidNodeID(const FName& NameOverride) const
{
	bool FoundNodeID = false;
	if (SelectorStruct)
	{
		const FName NodeID = NameOverride.IsNone()? SelectorStruct->NodeID : NameOverride;
		if (UDialogueBlueprint* DialogueBlueprint = GetBlueprintFromClass<UDialogueBlueprint>(GetSetAsset()))
		{
			FoundNodeID = DialogueBlueprint->DialogueTemplate->GetNodes().FindByPredicate([&NodeID](const UDialogueNode* Node)
			{
				return Node->GetID() == NodeID;
			}) != nullptr;
		}
	}	
	return FoundNodeID;
}

void FDialogueNodeSelectorPropertyTypeCustomization::BrowseToNode(UObject* Object)
{
	FNodeSelectorPropertyTypeCustomizationBase::BrowseToNode(Object);
	
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
				return DialogueNode ? DialogueNode->DialogueNode->GetID() == SelectorStruct->NodeID : false;
			});

			// try jump to node
			if (EdNode)
			{
				DialogueGraphEditor->JumpToNode(*EdNode);
			}
		}
	}
}
