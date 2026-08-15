// Copyright Narrative Tools 2025.


#include "Navigation/POIDetailsCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Navigation/POIActor.h"

TSharedRef<IDetailCustomization> FPOIDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FPOIDetailsCustomization);
}

void FPOIDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() == 0) return;

	APOIActor* Target = Cast<APOIActor>(Objects[0].Get());
	if (!Target) return;

	// Create a custom category at the top
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Editor Tools", FText::GetEmpty(), ECategoryPriority::Variable);
}
