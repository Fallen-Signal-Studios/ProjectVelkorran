// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryNarrativeItem.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENALPREEDITOR_API UActorFactoryNarrativeItem : public UActorFactory
{
	GENERATED_BODY()

	UActorFactoryNarrativeItem();

	// UActorFactory interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual UClass* GetDefaultActorClass(const FAssetData& AssetData) override;
	// End of UActorFactory interface
};
