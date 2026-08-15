// Copyright Narrative Tools 2025.

#pragma once

#include "EdGraphUtilities.h"
#include "NodeSelector/GraphPin_NodeSelectorBase.h"
#include "Tales/NarrativeNodeSelector.h"

class SGraphPin_QuestNodeSelector final : public SGraphPin_NodeSelectorBase
{
	FQuestNodeSelector SelectorStruct;
	bool bSelectStates = false;
	
public:
	
	SLATE_BEGIN_ARGS(SGraphPin_QuestNodeSelector) {}
	SLATE_END_ARGS()

	/* SGraphPin */
	void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj);
	/* SGraphPin */

private:

	virtual UScriptStruct* GetStaticStruct() const override { return SelectorStruct.StaticStruct(); }
	virtual FNodeIDSelector* GetSelectorStruct() override { return &SelectorStruct; }
	virtual UClass* GetMetaClass() override;
	virtual const UClass* SelectedClass() const override;
	
	virtual void FillAssetIfNeeded() override;
	virtual void FillNodeUserIDList() override;
	virtual FText GetNodeUserIDText() const override;

	virtual void OnSetClass(const UClass* SetClass) override;

	virtual bool IsValidGraphNode() const override;
	virtual bool IsValidNodeID(const FName& NameOverride = NAME_None) const override;
	virtual void BrowseToNode(UObject* Object) override;

};

struct FPinFactoryQuestNodeSelector : public FGraphPanelPinFactory
{
public:
	virtual TSharedPtr<SGraphPin> CreatePin(UEdGraphPin* InPin) const override;
};
