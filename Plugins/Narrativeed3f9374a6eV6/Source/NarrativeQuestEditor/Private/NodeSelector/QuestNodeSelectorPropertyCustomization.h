// Copyright Narrative Tools 2025.

#pragma once

#include "NodeSelector/NodeSelectorPropertyCustomizationBase.h"

struct FQuestStateSelector;
struct FQuestBranchSelector;

/// IPropertyTypeCustomization for FQuestStateSelector
class FQuestStateSelectorPropertyTypeCustomization final : public FNodeSelectorPropertyTypeCustomizationBase
{
	FQuestStateSelector* SelectorStruct;
	
public:

	FQuestStateSelectorPropertyTypeCustomization()
	: SelectorStruct(nullptr)
	{}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FQuestStateSelectorPropertyTypeCustomization);
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

/// IPropertyTypeCustomization for FQuestBranchSelector
class FQuestBranchSelectorPropertyTypeCustomization final : public FNodeSelectorPropertyTypeCustomizationBase
{
	FQuestBranchSelector* SelectorStruct;
	
public:

	FQuestBranchSelectorPropertyTypeCustomization()
	: SelectorStruct(nullptr)
	{}

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FQuestBranchSelectorPropertyTypeCustomization);
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
