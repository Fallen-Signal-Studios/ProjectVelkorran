// Copyright Narrative Tools 2025.

#pragma once

#include "NodeSelector/NodeSelectorPropertyCustomizationBase.h"

struct FDialogueNodeSelector;

/// IPropertyTypeCustomization for FDialogueNodeSelector
class FDialogueNodeSelectorPropertyTypeCustomization : public FNodeSelectorPropertyTypeCustomizationBase 
{
	FDialogueNodeSelector* SelectorStruct;
	
public:

	FDialogueNodeSelectorPropertyTypeCustomization()
	: SelectorStruct(nullptr)
	{}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FDialogueNodeSelectorPropertyTypeCustomization);
	}
	
private:

	virtual UClass* GetSetAsset() const override;
	virtual void SetStructPtr(TSharedRef<IPropertyHandle> PropertyHandle) override;
	virtual void FillAssetIfNeeded() override;
	virtual void FillNodeUserIDList() override;
	virtual FNodeIDSelector* GetSelectorStruct() override;
	virtual FNodeIDSelector* GetSelectorStruct() const override;

	virtual bool IsValidGraphNode() const override;
	virtual bool IsValidNodeID(const FName& NameOverride = NAME_None) const override;

	virtual void BrowseToNode(UObject* Object) override;
	
};
