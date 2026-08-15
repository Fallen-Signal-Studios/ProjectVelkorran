// Copyright Narrative Tools 2025.


#include "Spawners/SpawnerDetailsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Spawners/NPCSpawner.h"

#define LOCTEXT_NAMESPACE "FSpawnerDetailsCustomization"

TSharedRef<IDetailCustomization> FSpawnerDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FSpawnerDetailsCustomization);
}

void FSpawnerDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() == 0) return;

	ANPCSpawner* Target = Cast<ANPCSpawner>(Objects[0].Get());
	if (!Target) return;

	// Create a custom category at the top
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Editor Tools", FText::GetEmpty(), ECategoryPriority::Variable);

	// Create custom row and button for executing CreateNPCSpawner function
	Category.AddCustomRow(LOCTEXT("CreateNPCSpawner", "Create NPC Spawn"))
	.WholeRowWidget
	[
		SNew(SButton)
		.Text(LOCTEXT("CreateNPCSpawnerButton", "Create NPC Spawn"))
		.HAlign(HAlign_Center)
		.OnClicked_Lambda([Target]()
		{
			if (Target)
			{
				Target->CreateNPCSpawner();
			}
			return FReply::Handled();
		})
	];
}

#undef LOCTEXT_NAMESPACE
