// Copyright Narrative Tools 2025.

#include "Interaction/ActorFactoryNarrativeItem.h"
#include "ArsenalSettings.h"
#include "Interaction/InteractableItemPickup.h"
#include "Items/NarrativeItem.h"

UActorFactoryNarrativeItem::UActorFactoryNarrativeItem()
{
	DisplayName = FText::FromString("Interactable Item Pickup");
}

void UActorFactoryNarrativeItem::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	AItemPickup* Pickup = Cast<AItemPickup>(NewActor);
	if (!Pickup) { return; }

	auto ItemBlueprint = Cast<UBlueprint>(Asset);

	FPickupConfiguration PickupConfig;
	PickupConfig.PickupClass = ItemBlueprint->GeneratedClass;

	if (UNarrativeItem* CDO = Cast<UNarrativeItem>(ItemBlueprint->GeneratedClass.GetDefaultObject()))
	{
		PickupConfig.QuantityToGive = CDO->GetQuantity();

		FPickupMeshData Config = CDO->GetPickupMeshData(PickupConfig.QuantityToGive);

		Pickup->SetActorRotation(Config.PickupOffset.GetRotation());
		Pickup->SetActorScale3D(Config.PickupOffset.GetScale3D());
	}

	Pickup->SetPickup(PickupConfig);
}

bool UActorFactoryNarrativeItem::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	auto ItemBlueprint = Cast<UBlueprint>(AssetData.GetAsset());
	auto GeneratedClass = ItemBlueprint ? ItemBlueprint->GeneratedClass : nullptr;
	bool bIsNarrativeItem = GeneratedClass ? GeneratedClass->IsChildOf(UNarrativeItem::StaticClass()) : false;
	
	if (AssetData.IsValid() && bIsNarrativeItem)
	{
		return true;
	}
	else
	{
		OutErrorMsg = FText::FromString("Asset is not a NarrativeItem");
		return false;
	}
}

UClass* UActorFactoryNarrativeItem::GetDefaultActorClass(const FAssetData& AssetData)
{
	const UArsenalSettings* ArsenalSettings = GetDefault<UArsenalSettings>();
	return ArsenalSettings->DefaultInteractablePickup;
}
