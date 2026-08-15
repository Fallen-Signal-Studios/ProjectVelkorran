// Copyright Narrative Tools 2025.

#pragma once

#include "IPropertyTypeCustomization.h"

struct FNodeIDSelector;

/// IPropertyTypeCustomization base class for narrative node selectors
class NARRATIVEARSENALPREEDITOR_API FNodeSelectorPropertyTypeCustomizationBase : public IPropertyTypeCustomization 
{
	
protected:

	TArray<TSharedPtr<FName>> NodeUserIDList;
	// optional asset property
	TSharedPtr<IPropertyHandle> AssetPropertyHandle;
	// node ID property
	TSharedPtr<IPropertyHandle> NodeIDPropertyHandle;
	// selected node user ID 
	TSharedPtr<SEditableText> SelectedNodeIDEditableText;
	// node ID selection combo
	TSharedPtr<SComboBox<TSharedPtr<FName>>> NodeUserIDComboBox;
	TSharedPtr<FName> InitiallySelectedItem;
	FDelegateHandle OnAssetEditorOpened;

public:

	FNodeSelectorPropertyTypeCustomizationBase() {}

	/* IPropertyTypeCustomization */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	/* IPropertyTypeCustomization */

protected:

	// take the parent property handle and get a struct value from it of Type
	template<typename T>
	static T* GetStructFromHandle(TSharedRef<IPropertyHandle> StructHandle)
	{
		if (void* RawData = nullptr;
			StructHandle->GetValueData(RawData) == FPropertyAccess::Success && RawData)
		{
			return static_cast<T*>(RawData);
		}
		return nullptr;
	}

	template<typename T>
	T* GetBlueprintFromClass(UClass* Class)
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		return Cast<T>(BP);
	}
	
	template<typename T>
	T* GetBlueprintFromClass(UClass* Class) const
	{
		UBlueprint* BP = UBlueprint::GetBlueprintFromClass(Class);
		return Cast<T>(BP);
	}

	// get the node user ID
	FText GetNodeUserIDText() const;
	// work around or odd asset property handle bug
	virtual UClass* GetSetAsset() const = 0;
	// invalid node ID tool tip
	FText GetInvalidNodeToolTipText() const { return FText::Format(INVTEXT("Could not find node ID: \"{0}\""), GetNodeUserIDText()); }
	// returns either "Node ID" or GetInvalidNodeToolTipText()
	FText NodeIDEditableTextToolTip() const;
	
	// simple function override for child classes of this to set their struct ptr without needing to override CustomizeChildren()
	virtual void SetStructPtr(TSharedRef<IPropertyHandle> PropertyHandle) = 0;
	// if the asset handle is empty, then try see if the property is inside an asset of the same type
	virtual void FillAssetIfNeeded() = 0;
	// get user IDs
	virtual void FillNodeUserIDList() = 0;
	void RefreshNodeIdList();
	
	// returns true when the node ID can be found in the graph.
	virtual bool IsValidGraphNode() const = 0;
	// returns true is the node ID belongs to an existing node.
	virtual bool IsValidNodeID(const FName& NameOverride = NAME_None) const = 0;
	
	// called when asset property is changed
	virtual void OnAssetPropertyChanged();
	// when the node ID selection is changed, update the GUID ref
	void OnNodeIDSelectionChanged(TSharedPtr<FName> Name, ESelectInfo::Type SelectMethod);
	// when user node ID is changed on this property specifically, call OnNodeIDSelectionChanged() to try get the GUID
	void OnSelectedNodeIDTextChanged(const FText& Text, ETextCommit::Type CommitMethod);

	// get selector struct as base struct, useful for when node ID is only needed.
	virtual FNodeIDSelector* GetSelectorStruct() = 0;
	virtual FNodeIDSelector* GetSelectorStruct() const = 0;
	
	// visible when node is a valid node in the graph
	EVisibility ValidGraphNodeVisibility() const { return IsValidGraphNode()? EVisibility::Visible : EVisibility::Collapsed; }
	EVisibility NonValidGraphNodeVisibility() const { return IsValidGraphNode()? EVisibility::Collapsed : EVisibility::Visible; }
	FSlateColor ValidGraphNodeColor() const { return IsValidGraphNode()? FLinearColor::Transparent : FLinearColor::Yellow; }
	// browse to node, if possible
	FReply TryBrowseToNode();
	// actual browse to the node
	virtual void BrowseToNode(UObject* Object);
	
};
