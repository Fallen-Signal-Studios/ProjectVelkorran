// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryNPCDefinition.generated.h"

/**
 * 
 */
UCLASS()
class NARRATIVEARSENALPREEDITOR_API UActorFactoryNPCDefinition : public UActorFactory
{
	GENERATED_BODY()

	UActorFactoryNPCDefinition();

	// UActorFactory interface
	virtual void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	// End of UActorFactory interface
};
