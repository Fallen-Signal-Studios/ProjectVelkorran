// Copyright Narrative Tools 2025.

#include "NodeSelector/GraphPin_NodeSelectorBase.h"
#include "PropertyCustomizationHelpers.h"
#include "Tales/NarrativeNodeSelector.h"

void SGraphPin_NodeSelectorBase::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);

	// get struct default
	if (const UScriptStruct* ScriptStruct = GetStaticStruct())
	{
		const FString DefaultValue = InGraphPinObj->GetDefaultAsString();
		ScriptStruct->ImportText(*DefaultValue, GetSelectorStruct(), nullptr, PPF_None, GLog, ScriptStruct->GetName());
	}
	
	FillAssetIfNeeded();
	FillNodeUserIDList();
}

TSharedRef<SWidget> SGraphPin_NodeSelectorBase::GetDefaultValueWidget()
{
	const FNodeIDSelector* NodeIDSelector = GetSelectorStruct();
	const FName CurrentNodeID = NodeIDSelector? GetSelectorStruct()->NodeID : NAME_None;
	if (TSharedPtr<FName>* NameElem = NodeUserIDList.FindByPredicate([&CurrentNodeID](TSharedPtr<FName> Elem)
	{
		return Elem.IsValid() ? *Elem.Get() == CurrentNodeID : false;
	}))
	{
		InitiallySelectedItem = *NameElem;
	}
	
	return
		SNew(SBox)
		.Padding(5.0f)
		.Visibility_Lambda([this]()
		{
			return SGraphPin::GetDefaultValueVisibility();
		})
		[
			SNew(SVerticalBox)
			
			// asset
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				// added to match offset of node ID combo
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor::Transparent)
				.HAlign(HAlign_Fill)
				[
					SNew(SClassPropertyEntryBox)
					.MetaClass(GetMetaClass())
					.AllowNone(true)
					.HideViewOptions(false)
					.OnSetClass_Raw(this, &SGraphPin_NodeSelectorBase::OnSetClass)
					.SelectedClass_Raw(this, &SGraphPin_NodeSelectorBase::SelectedClass)
				]	
			]
	
			// node ID
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				// yellow outline
				SNew(SBorder)
				.BorderBackgroundColor_Raw(this, &SGraphPin_NodeSelectorBase::ValidGraphNodeColor)
				.BorderImage(FAppStyle::Get().GetBrush("NotificationList.ItemBackground_Border_Transparent"))
				.HAlign(HAlign_Fill)
				[
					SNew(SHorizontalBox)
					
					// node ID
					+SHorizontalBox::Slot()
					[
						SAssignNew(NodeUserIDComboBox, SComboBox<TSharedPtr<FName>>)
						.OptionsSource(&NodeUserIDList)
						.OnSelectionChanged_Raw(this, &SGraphPin_NodeSelectorBase::OnNodeIDSelectionChanged)
						.InitiallySelectedItem(InitiallySelectedItem)
						.OnGenerateWidget_Lambda([](TSharedPtr<FName> InItem)
						{
							return SNew(STextBlock).Text(FText::FromName(*InItem));
						})
						[
							SAssignNew(SelectedNodeIDEditableText, SEditableText)
							.SelectAllTextWhenFocused(true)
							.Text_Raw(this, &SGraphPin_NodeSelectorBase::GetNodeUserIDText)
							.OnTextCommitted_Raw(this, &SGraphPin_NodeSelectorBase::OnSelectedNodeIDTextChanged)
							.ToolTipText_Raw(this, &SGraphPin_NodeSelectorBase::NodeIDEditableTextToolTip)
						]
					]
	
					// browse to
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.ToolTipText(INVTEXT("Browse to the selected node."))
						.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
						.OnClicked(this, &SGraphPin_NodeSelectorBase::TryBrowseToNode)
						.Visibility(this, &SGraphPin_NodeSelectorBase::BrowseToNodeVisibility)
						.Content()
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Search"))
						]
					]
				]
			]
		];
}

FText SGraphPin_NodeSelectorBase::NodeIDEditableTextToolTip() const
{
	if (IsValidNodeID())
	{
		return FText::Format(INVTEXT("Node ID: \"{0}\""), GetNodeUserIDText());
	}
	
	return FText::Format(INVTEXT("Could not find node ID: \"{0}\""), GetNodeUserIDText());
}

void SGraphPin_NodeSelectorBase::OnSetClass(const UClass* SetClass)
{
	// do the same as if a selection was made
	OnNodeIDSelectionChanged(MakeShared<FName>(NAME_None), ESelectInfo::Type::Direct);
	
	// empty current list
	NodeUserIDList.Empty();
	if (NodeUserIDComboBox.IsValid())
	{
		NodeUserIDComboBox->RefreshOptions();
	}
	
	FillNodeUserIDList();
	SetDefaultValue();
}

void SGraphPin_NodeSelectorBase::OnNodeIDSelectionChanged(TSharedPtr<FName> Name, ESelectInfo::Type SelectMethod)
{
	if (const FName* SelectedName = Name.Get())
	{
		// update text
		if (SelectedNodeIDEditableText.IsValid())
		{
			SelectedNodeIDEditableText->SetText(FText::FromName(*SelectedName));
		}

		if (FNodeIDSelector* NodeIDSelector = GetSelectorStruct())
		{
			NodeIDSelector->NodeID = *SelectedName;
		}
		
		SetDefaultValue();
	}
}

void SGraphPin_NodeSelectorBase::OnSelectedNodeIDTextChanged(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FName Value = Text.IsEmpty()? NAME_None : FName(Text.ToString());
	if (NodeUserIDComboBox.IsValid() && !IsValidNodeID(Value))
	{
		NodeUserIDComboBox->ClearSelection();
	}
	
	OnNodeIDSelectionChanged(MakeShared<FName>(Value), ESelectInfo::Type::Direct);
}

void SGraphPin_NodeSelectorBase::SetDefaultValue()
{	
	FString Result;
	GetStaticStruct()->ExportText(Result, GetSelectorStruct(), nullptr, nullptr, PPF_None, nullptr);
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, Result);
}

FReply SGraphPin_NodeSelectorBase::TryBrowseToNode()
{
	UBlueprint* BP = UBlueprint::GetBlueprintFromClass(SelectedClass());
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (BP && AssetEditorSubsystem)
	{
		// when there is an existing editor instance, browse to node right away
		if (AssetEditorSubsystem->FindEditorForAsset(BP, true))
		{
			BrowseToNode(BP);
		}
		else
		{
			// queue up the browse for when the editor is opened for the asset
			OnAssetEditorOpened = AssetEditorSubsystem->OnAssetEditorOpened().AddRaw(this, &SGraphPin_NodeSelectorBase::BrowseToNode);
			AssetEditorSubsystem->OpenEditorForAsset(BP);
		}
	}
	
	return FReply::Handled();
}

void SGraphPin_NodeSelectorBase::BrowseToNode(UObject* Object)
{
	// unbind handle
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		AssetEditorSubsystem && OnAssetEditorOpened.IsValid())
	{
		AssetEditorSubsystem->OnAssetEditorOpened().Remove(OnAssetEditorOpened);
	}
}
