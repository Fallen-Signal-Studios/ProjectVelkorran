// Copyright Narrative Tools 2024. 


#include "AssetTypeActions_ItemCollection.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_ItemCollection"

FAssetTypeActions_ItemCollection::FAssetTypeActions_ItemCollection(uint32 InAssetCategory) : Category(InAssetCategory)
{

}

const TArray<FText>& FAssetTypeActions_ItemCollection::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("NarrativeItemsSubMenu", "Narrative Items"),
	};

	return SubMenus;
}

#undef LOCTEXT_NAMESPACE
