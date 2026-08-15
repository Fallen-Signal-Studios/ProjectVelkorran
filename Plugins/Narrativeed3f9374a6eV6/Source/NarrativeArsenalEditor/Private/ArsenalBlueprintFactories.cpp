// Copyright Narrative Tools 2024. 


#include "ArsenalBlueprintFactories.h"
#include "Items/WeaponItem.h"
#include "Items/GameplayEffectItem.h"
#include "ArsenalBlueprints.h"
#include "AI/NPCDefinition.h"
#include "Character/PlayerDefinition.h"
#include <Kismet2/KismetEditorUtilities.h>

#include "DialogueAssetFactory.h"
#include "DialogueBlueprint.h"
#include "NarrativeGameplayTags.h"
#include "PackageTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Character/CharacterAppearance.h"
#include "Items/InventoryComponent.h"
#include "CharacterCreator/Options/CharacterCreatorOption_Mesh.h"
#include "CharacterCreator/Options/CharacterCreatorOption_Groom.h"
#include "CharacterCreator/Options/CharacterCreatorOption_Scalar.h"
#include "CharacterCreator/Options/CharacterCreatorOption_Vector.h"
#include "CharacterCreator/Items/CharacterCreatorItem_Mesh.h"
#include "CharacterCreator/Items/CharacterCreatorItem_Groom.h"
#include "CharacterCreator/CharacterCreatorConfiguration.h"
#include "CharacterCreator/CreatorColorSwatch.h"
#include "Factories/DataAssetFactory.h"
#include "Tales/Dialogue.h"
#include "Tales/TaggedDialogueSet.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "ArsenalBlueprintFactories"

UWeaponItemBlueprintFactory::UWeaponItemBlueprintFactory()
{
	SupportedClass = UWeaponItemBlueprint::StaticClass();
	ParentClass = UWeaponItem::StaticClass();
	bSkipClassPicker = true;
}

UObject* UWeaponItemBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UWeaponItemBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UWeaponItemBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

UGameplayEffectItemBlueprintFactory::UGameplayEffectItemBlueprintFactory()
{
	SupportedClass = UGameplayEffectItemBlueprint::StaticClass();
	ParentClass = UGameplayEffectItem::StaticClass();
	bSkipClassPicker = true;
}

UObject* UGameplayEffectItemBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	// Make sure we are trying to factory a blueprint, then create and init one
	check(Class->IsChildOf(UBlueprint::StaticClass()));

	return FKismetEditorUtilities::CreateBlueprint(ParentClass, InParent, Name, BPTYPE_Normal, UGameplayEffectItemBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), CallingContext);
}

UObject* UGameplayEffectItemBlueprintFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(Class, InParent, Name, Flags, Context, Warn, NAME_None);
}

UNPCDefinitionFactory::UNPCDefinitionFactory()
{
	SupportedClass = UNPCDefinition::StaticClass();
	bCreateNew = true; 
}

class SNPCDefinitionCreateDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SNPCDefinitionCreateDialog ){}

	SLATE_END_ARGS()

	FReply OkClicked()
	{
		CloseDialog(true);
		return FReply::Handled();
	}
	
	FReply CancelClicked()
	{
		CloseDialog();
		return FReply::Handled();
	}

	void CloseDialog(bool bWasPicked=false)
	{
		bOkClicked = bWasPicked;
		
		if ( PickerWindow.IsValid() )
		{
			PickerWindow.Pin()->RequestDestroyWindow();
		}
	}

	FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			CloseDialog();
			return FReply::Handled();
		}
		return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
	
	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		bOkClicked = false;
		ParentClass = UNPCDefinition::StaticClass();

		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;
		
		NPCDefDetailsView = EditModule.CreateDetailView(DetailsViewArgs);
		
		ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(FAppStyle::GetBrush("ChildWindow.Background"))
			[
				SNew(SVerticalBox)

				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					NPCDefDetailsView->AsShared()
				]

				// Ok/Cancel buttons
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(10.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
					.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
					.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
					+SUniformGridPanel::Slot(0,0)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("CreateNPCDefinition_Tooltip", "Create a new NPCDefinition.\nOptionally select checkboxes to create appearance and dialogue assets."))
						.HAlign(HAlign_Center)
						.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
						.OnClicked(this, &SNPCDefinitionCreateDialog::OkClicked)
						.Text(LOCTEXT("CreateNPCDefinition", "Create"))
					]
					+SUniformGridPanel::Slot(1,0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
						.OnClicked(this, &SNPCDefinitionCreateDialog::CancelClicked)
						.Text(LOCTEXT("CreateNPCDefinitionCancel", "Cancel"))
					]
				]
			]
		];
	}

	// Creates a configuration window before creating NPC Definition
	bool ConfigureProperties(TWeakObjectPtr<UNPCDefinitionFactory> InNPCDefinitionFactory)
	{
		NPCDefinitionFactory = InNPCDefinitionFactory;

		TSharedRef<SWindow> Window = SNew(SWindow)
		.Title( LOCTEXT("CreateNPCDefinitionOptions", "Create NPC Definition") )
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(500.f, 300.f))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		[
			AsShared()
		];

		NPCDefDetailsView->SetObject(NPCDefinitionFactory.Get(), true);

		PickerWindow = Window;

		GEditor->EditorAddModalWindow(Window);
		NPCDefinitionFactory.Reset();

		return bOkClicked;
	}

public:
	TWeakObjectPtr<UNPCDefinitionFactory> NPCDefinitionFactory;
	TSharedPtr<IDetailsView> NPCDefDetailsView;
	TWeakPtr<SWindow> PickerWindow;
	bool bOkClicked;
	UClass* ParentClass;
	
	bool bCreateDialogue = false;
	bool bCreateAppearance = false;
};

UObject* UNPCDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	FNarrativeGameplayTags NarrativeTags = FNarrativeGameplayTags::Get();
	
	auto NPCDefinition = NewObject<UNPCDefinition>(InParent, Class, Name, Flags, Context);
	check(NPCDefinition);

	// Grab name of NPC - remove the prefix if necessary
	int UnderscoreIndex = Name.ToString().Find("_");
	FString NameString = UnderscoreIndex == INDEX_NONE ? Name.ToString() : Name.ToString().RightChop(UnderscoreIndex+1);

	// Fix up NPCDefinition variables
	NPCDefinition->NPCID = FName(NameString);
	NPCDefinition->NPCName = FText::FromString(NameString);
	NPCDefinition->MinLevel = MinLevel;
	NPCDefinition->MaxLevel = MaxLevel;

	// lambda for creating asset convenience
	auto CreateAsset = [&InParent, &NameString](UClass* Class, UClass* FactoryClass, const FString& Prefix, const FString& Suffix = "")
	{
		auto AssetName = Prefix + NameString + Suffix;
		FString NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName()) + TEXT("/") + AssetName;
		UPackage* Package = UPackageTools::FindOrCreatePackageForAssetType(FName(*NewPackageName), Class);
	
		auto DialogueAssetFactory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
		auto Asset = DialogueAssetFactory->FactoryCreateNew(Class, Package, *AssetName, RF_Standalone|RF_Public, NULL, GWarn);
		FAssetRegistryModule::AssetCreated(Asset);
		Package->SetDirtyFlag(true);
		return Asset;
	};

	auto CreateDialogueAsset = [&CreateAsset, &NPCDefinition](const FString& Suffix = "")
	{
		auto DialogueAsset = Cast<UDialogueBlueprint>(CreateAsset(UDialogueBlueprint::StaticClass(), UDialogueAssetFactory::StaticClass(), "DBP_", Suffix));

		// Add NPC speaker to dialogue
		auto Dialogue = DialogueAsset ? Cast<UDialogue>(DialogueAsset->GeneratedClass->GetDefaultObject()) : nullptr;
		if (ensure(Dialogue))
		{
			FSpeakerInfo Speaker = FSpeakerInfo();
			Speaker.NPCDataAsset = NPCDefinition;
			Speaker.SpeakerID = NPCDefinition->NPCID;
			Dialogue->Speakers.Emplace(Speaker);

			// update root node to use this speaker initially
			if (Dialogue->RootDialogue)
			{
				Dialogue->RootDialogue->SetSpeakerID(Speaker.SpeakerID);
			}
		}
		return DialogueAsset;
	};

	// Create dialogue asset
	if (bCreateDialogueAsset)
	{
		auto DialogueAsset = CreateDialogueAsset();
		NPCDefinition->Dialogue = DialogueAsset->GeneratedClass;
	}

	// Create appearance asset
	if (bCreateAppearanceAsset)
	{
		auto AppearanceAsset = Cast<UCharacterAppearance>(CreateAsset(UCharacterAppearance::StaticClass(), UDataAssetFactory::StaticClass(), "Appearance_"));
		NPCDefinition->DefaultAppearance = AppearanceAsset;
	}

	// Create tagged dialogue
	if (bCreateTaggedDialogue)
	{
		auto TaggedDialogueAsset = Cast<UTaggedDialogueSet>(CreateAsset(UTaggedDialogueSet::StaticClass(), UDataAssetFactory::StaticClass(), "TaggedDialogues_"));
		NPCDefinition->TaggedDialogueSet = TaggedDialogueAsset;

		// autofill with default tagged dialogue settings
		if (bAutofillTaggedDialogue)
		{
			auto GreetDialogue = CreateDialogueAsset("_Greet");

			FTaggedDialogue GreetTaggedDialogue = FTaggedDialogue();
			GreetTaggedDialogue.Dialogue = GreetDialogue;
			GreetTaggedDialogue.Tag = NarrativeTags.TaggedDialogue_Greet;
			TaggedDialogueAsset->TaggedDialogues.Emplace(GreetTaggedDialogue);
		}
	}
	
	return NPCDefinition;
}

bool UNPCDefinitionFactory::ConfigureProperties()
{
	TSharedRef<SNPCDefinitionCreateDialog> Dialog = SNew(SNPCDefinitionCreateDialog);
	return Dialog->ConfigureProperties(this);
}

UPlayerDefinitionFactory::UPlayerDefinitionFactory()
{
	SupportedClass = UPlayerDefinition::StaticClass();
	bCreateNew = true; 
}

UObject* UPlayerDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UPlayerDefinition>(InParent, Class, Name, Flags, Context);
}

UCreatorOptionMeshFactory::UCreatorOptionMeshFactory()
{
	SupportedClass = UCharacterCreatorOption_Mesh::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorOptionMeshFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorOption_Mesh>(InParent, Class, Name, Flags, Context);
}

UCreatorOptionGroomFactory::UCreatorOptionGroomFactory()
{
	SupportedClass = UCharacterCreatorOption_Groom::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorOptionGroomFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorOption_Groom>(InParent, Class, Name, Flags, Context);
}

UCreatorOptionScalarFactory::UCreatorOptionScalarFactory()
{
	SupportedClass = UCharacterCreatorOption_Scalar::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorOptionScalarFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorOption_Scalar>(InParent, Class, Name, Flags, Context);
}

UCreatorOptionVectorFactory::UCreatorOptionVectorFactory()
{
	SupportedClass = UCharacterCreatorOption_Vector::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorOptionVectorFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorOption_Vector>(InParent, Class, Name, Flags, Context);
}

UCreatorItemMeshFactory::UCreatorItemMeshFactory()
{
	SupportedClass = UCharacterCreatorItem_Mesh::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorItemMeshFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorItem_Mesh>(InParent, Class, Name, Flags, Context);
}

UCreatorItemGroomFactory::UCreatorItemGroomFactory()
{
	SupportedClass = UCharacterCreatorItem_Groom::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorItemGroomFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorItem_Groom>(InParent, Class, Name, Flags, Context);
}


UCreatorFormFactory::UCreatorFormFactory()
{
	SupportedClass = UCharacterCreatorForm::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorFormFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorForm>(InParent, Class, Name, Flags, Context);
}

UCreatorSectionFactory::UCreatorSectionFactory()
{
	SupportedClass = UCharacterCreatorSection::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorSectionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorSection>(InParent, Class, Name, Flags, Context);
}

UCreatorPageFactory::UCreatorPageFactory()
{
	SupportedClass =  UCharacterCreatorPage::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorPageFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorPage>(InParent, Class, Name, Flags, Context);
}

UCreatorColorSwatchFactory::UCreatorColorSwatchFactory()
{
	SupportedClass = UCharacterCreatorColorSwatch::StaticClass();
	bCreateNew = true; 
}

UObject* UCreatorColorSwatchFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UCharacterCreatorColorSwatch>(InParent, Class, Name, Flags, Context);
}

#undef LOCTEXT_NAMESPACE