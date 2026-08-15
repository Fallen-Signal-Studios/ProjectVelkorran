// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Chaos/ChaosEngineInterface.h"
#include "GameplayTagContainer.h"
#include "FoleyAudioBank.generated.h"

USTRUCT(BlueprintType)
struct FNarrativeFoleySound
{

	GENERATED_BODY()

	FNarrativeFoleySound() {};

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Foley")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<class USoundBase>> FoleySounds;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Foley")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<class UNiagaraSystem>> FoleyParticles;

};

/**
 * Enhanced version of GASPs audio bank - adds surface type functionality. 
 * 
 * Maps each foley event to a map of sounds for that event, mapped by surface type. ie Walk.Concrete, Walk.Metal, Land.Wood, etc. 
 */
UCLASS(BlueprintType)
class NARRATIVEARSENAL_API UFoleyAudioBank : public UDataAsset
{
	GENERATED_BODY()
	
public: 

	UFoleyAudioBank(const FObjectInitializer& ObjectInit);

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Foley")
	TMap<FGameplayTag, FNarrativeFoleySound> FoleySounds;

};
