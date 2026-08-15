// Copyright Narrative Tools 2025.

#include "NodeSelector/NodeSelectorPropertyCustomizationBase.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Tales/NarrativeNodeSelector.h"

void FNodeSelectorPropertyTypeCustomizationBase::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
	.NameWidget
	[
		SNew(SHorizontalBox)
		// property name
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]

		// warning icon
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(5.0f, 0.0f)
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Warning")) // Use "Icons.WarningWithColor" for a colored version
			.ToolTipText_Raw(this, &FNodeSelectorPropertyTypeCustomizationBase::GetInvalidNodeToolTipText)
			.ColorAndOpacity(FLinearColor::Yellow)
			.Visibility(this, &FNodeSelectorPropertyTypeCustomizationBase::NonValidGraphNodeVisibility)
		]
	];
}

void FNodeSelectorPropertyTypeCustomizationBase::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	SetStructPtr(PropertyHandle);

	/* asset */
	AssetPropertyHandle = PropertyHandle->GetChildHandle(TEXT("Asset"));
	if (AssetPropertyHandle.IsValid())
	{
		AssetPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FNodeSelectorPropertyTypeCustomizationBase::OnAssetPropertyChanged));
		AssetPropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateRaw(this, &FNodeSelectorPropertyTypeCustomizationBase::FillAssetIfNeeded));

		// if the outer is a valid asset, then set the default property to be the outer asset when none is set
		FillAssetIfNeeded();
	
		/* add asset property */
		//ChildBuilder.AddProperty(AssetPropertyHandle.ToSharedRef());
		ChildBuilder.AddCustomRow(INVTEXT("Asset"))
		.NameContent()
		[
			AssetPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			// added to match offset of node ID combo
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Transparent)
			.HAlign(HAlign_Fill)
			[
				AssetPropertyHandle->CreatePropertyValueWidget()
			]
		];
	}	
	
	/* node ID */
	RefreshNodeIdList();
	const FNodeIDSelector* NodeIDSelector = GetSelectorStruct();
	const FName CurrentNodeID = NodeIDSelector? GetSelectorStruct()->NodeID : NAME_None;
	if (TSharedPtr<FName>* NameElem = NodeUserIDList.FindByPredicate([&CurrentNodeID](TSharedPtr<FName> Elem)
	{
		return Elem.IsValid() ? *Elem.Get() == CurrentNodeID : false;
	}))
	{
		InitiallySelectedItem = *NameElem;
	}
	
	NodeIDPropertyHandle = PropertyHandle->GetChildHandle(TEXT("NodeID"));
	if (NodeIDPropertyHandle.IsValid())
	{
		ChildBuilder.AddCustomRow(INVTEXT("NodeID"))
		.NameContent()
		[
			NodeIDPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SBorder)
			.BorderBackgroundColor_Raw(this, &FNodeSelectorPropertyTypeCustomizationBase::ValidGraphNodeColor)
			.BorderImage(FAppStyle::Get().GetBrush("NotificationList.ItemBackground_Border_Transparent"))
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
	
				// node ID
				+SHorizontalBox::Slot()
				[
					SAssignNew(NodeUserIDComboBox, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&NodeUserIDList)
					.OnSelectionChanged_Raw(this, &FNodeSelectorPropertyTypeCustomizationBase::OnNodeIDSelectionChanged)
					.InitiallySelectedItem(InitiallySelectedItem)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> InItem)
					{
						return SNew(STextBlock).Text(FText::FromName(*InItem));
					})
					[
						SAssignNew(SelectedNodeIDEditableText, SEditableText)
						.SelectAllTextWhenFocused(true)
						.Text_Raw(this, &FNodeSelectorPropertyTypeCustomizationBase::GetNodeUserIDText)
						.OnTextCommitted_Raw(this, &FNodeSelectorPropertyTypeCustomizationBase::OnSelectedNodeIDTextChanged)
						.ToolTipText_Raw(this, &FNodeSelectorPropertyTypeCustomizationBase::NodeIDEditableTextToolTip)
					]
				]

				// browse to
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ToolTipText(INVTEXT("Browse to the selected node."))
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.OnClicked(this, &FNodeSelectorPropertyTypeCustomizationBase::TryBrowseToNode)
					.Visibility(this, &FNodeSelectorPropertyTypeCustomizationBase::ValidGraphNodeVisibility)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Icons.Search"))
					]
				]
			]
		];
	}
}

void FNodeSelectorPropertyTypeCustomizationBase::RefreshNodeIdList()
{
	// empty current list
	NodeUserIDList.Empty();
	if (NodeUserIDComboBox.IsValid())
	{
		NodeUserIDComboBox->RefreshOptions();
	}
	
	FillNodeUserIDList();
}

void FNodeSelectorPropertyTypeCustomizationBase::OnAssetPropertyChanged()
{
	// do the same as if a selection was made
	OnNodeIDSelectionChanged(MakeShared<FName>(NAME_None), ESelectInfo::Type::Direct);
	
	RefreshNodeIdList();
}

void FNodeSelectorPropertyTypeCustomizationBase::OnNodeIDSelectionChanged(TSharedPtr<FName> Name ,ESelectInfo::Type SelectMethod)
{
	const FName SelectedID = Name? *Name : NAME_None;
	if (FNodeIDSelector* NodeIDSelector = GetSelectorStruct())
	{
		NodeIDSelector->NodeID = SelectedID;
	}

	if (SelectedNodeIDEditableText.IsValid())
	{
		SelectedNodeIDEditableText->SetText(FText::FromName(SelectedID));
	}
}

FText FNodeSelectorPropertyTypeCustomizationBase::GetNodeUserIDText() const
{
	FNodeIDSelector* NodeIDSelector = GetSelectorStruct();
	return FText::FromName(NodeIDSelector? NodeIDSelector->NodeID : NAME_None);
}

FText FNodeSelectorPropertyTypeCustomizationBase::NodeIDEditableTextToolTip() const
{
	if (IsValidNodeID())
	{
		return FText::Format(INVTEXT("Node ID: \"{0}\""), GetNodeUserIDText());
	}
	
	return GetInvalidNodeToolTipText();
}

void FNodeSelectorPropertyTypeCustomizationBase::OnSelectedNodeIDTextChanged(const FText& Text, ETextCommit::Type CommitMethod)
{
	const FName Value = Text.IsEmpty()? NAME_None : FName(Text.ToString());
	if (NodeUserIDComboBox.IsValid() && !IsValidNodeID(Value))
	{
		NodeUserIDComboBox->ClearSelection();
	}
	
	// do the same as if a selection was made
	OnNodeIDSelectionChanged(MakeShared<FName>(Value), ESelectInfo::Type::Direct);
}

FReply FNodeSelectorPropertyTypeCustomizationBase::TryBrowseToNode()
{
	if (!AssetPropertyHandle.IsValid())
	{
		return FReply::Unhandled();
	}

	UBlueprint* BP = UBlueprint::GetBlueprintFromClass(GetSetAsset());
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
			OnAssetEditorOpened = AssetEditorSubsystem->OnAssetEditorOpened().AddRaw(this, &FNodeSelectorPropertyTypeCustomizationBase::BrowseToNode);
			AssetEditorSubsystem->OpenEditorForAsset(BP);
		}
	}
	
	return FReply::Handled();
}

void FNodeSelectorPropertyTypeCustomizationBase::BrowseToNode(UObject* Object)
{
	// unbind handle
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem && OnAssetEditorOpened.IsValid())
	{
		AssetEditorSubsystem->OnAssetEditorOpened().Remove(OnAssetEditorOpened);
	}
}
