// Copyright Narrative Tools 2025.

#pragma once

#include "GameplayTagContainer.h"
#include "GameFramework/WorldSettings.h"
#include "NarrativeWorldSettings.generated.h"

class UTaggedMusicSet;

UCLASS(BlueprintType)
class NARRATIVEARSENAL_API ANarrativeWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	
	/// when set to a music set asset, the default music set will be this asset.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category="Music")
	TObjectPtr<UTaggedMusicSet> DefaultMusicSetOverride;


};
