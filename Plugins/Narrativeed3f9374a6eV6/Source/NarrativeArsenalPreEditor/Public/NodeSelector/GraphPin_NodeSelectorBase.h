// Copyright Narrative Tools 2025.

#pragma once

#include "SGraphPin.h"

class SClassPropertyEntryBox;
struct FNodeIDSelector;

class NARRATIVEARSENALPREEDITOR_API SGraphPin_NodeSelectorBase : public SGraphPin
{
public:
	
	SLATE_BEGIN_ARGS(SGraphPin_NodeSelectorBase) {}
	SLATE_END_ARGS()
	
	TArray<TSharedPtr<FName>> NodeUserIDList;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> NodeUserIDComboBox;
	TSharedPtr<SEditableText> SelectedNodeIDEditableText;
	TSharedPtr<FName> InitiallySelectedItem;
	FDelegateHandle OnAssetEditorOpened;
	
public:
	
	/* SGraphPin */
	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;
	/* SGraphPin */

protected:

	template<typename T>
	T* GetBlueprintFromClass(const UClass* Class)
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		return Cast<T>(BP);
	}
	
	template<typename T>
	T* GetBlueprintFromClass(const UClass* Class) const
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		return Cast<T>(BP);
	}

	// static struct type
	virtual UScriptStruct* GetStaticStruct() const = 0;
	// ptr to the actual struct var
	virtual FNodeIDSelector* GetSelectorStruct() = 0;
	// asset class type
	virtual UClass* GetMetaClass() = 0;
	// what class is selected
	virtual const UClass* SelectedClass() const = 0;
	// what ID to display is selected
	virtual FText GetNodeUserIDText() const = 0;
	FText NodeIDEditableTextToolTip() const;
	
	virtual bool IsValidGraphNode() const = 0;
	virtual bool IsValidNodeID(const FName& NameOverride = NAME_None) const = 0;
	
	// set the asset when if needed
	virtual void FillAssetIfNeeded() = 0;
	// fill the node ID list
	virtual void FillNodeUserIDList() = 0;
	
	// when a class is selected
	virtual void OnSetClass(const UClass* SetClass);
	// when a node ID is selected
	virtual void OnNodeIDSelectionChanged(TSharedPtr<FName> Name, ESelectInfo::Type SelectMethod);
	// when the text is manually changed
	virtual void OnSelectedNodeIDTextChanged(const FText& Text, ETextCommit::Type CommitMethod);
	
	// set new default value
	void SetDefaultValue();

	EVisibility BrowseToNodeVisibility() const { return IsValidGraphNode()? EVisibility::Visible : EVisibility::Collapsed; }
	FSlateColor ValidGraphNodeColor() const { return IsValidGraphNode()? FLinearColor::Transparent : FLinearColor::Yellow; }
	FReply TryBrowseToNode();
	virtual void BrowseToNode(UObject* Object);
	
};
