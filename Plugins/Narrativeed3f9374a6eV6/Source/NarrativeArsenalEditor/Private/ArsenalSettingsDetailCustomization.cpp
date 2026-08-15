// Copyright Narrative Tools 2025.

#include "ArsenalSettingsDetailCustomization.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "ProjectSetup/NarrativeProjectSetupNotice.h"

TSharedRef<IDetailCustomization> FArsenalSettingsDetailCustomization::MakeInstance()
{
	return MakeShareable(new FArsenalSettingsDetailCustomization);
}

void FArsenalSettingsDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("Narrative Pro");
	FDetailWidgetRow& DetailWidgetRow = DetailCategoryBuilder.AddCustomRow(INVTEXT("A"));
	DetailWidgetRow.WholeRowContent()
	[
		SNew(SNarrativeProjectSetupNotice)
		.IsEnabled(true)
	];
}
