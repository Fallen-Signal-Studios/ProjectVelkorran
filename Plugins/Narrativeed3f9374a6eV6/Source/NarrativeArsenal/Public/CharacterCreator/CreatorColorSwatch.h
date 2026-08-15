// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CreatorColorSwatch.generated.h"

/**
 * Holds a collection of colours, used by the character creator 
 */
UCLASS()
class NARRATIVEARSENAL_API UCharacterCreatorColorSwatch : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UCharacterCreatorColorSwatch();

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Swatch Colors")
	TArray<FLinearColor> Colors; 

	FLinearColor GetColor(const uint32 Color) const;

	FLinearColor GetColorRandom() const
	{
		const int32 RandIndex = FMath::RandRange(0, Colors.Num()-1);
		return GetColor(RandIndex);
	}
	
	FLinearColor GetColorRandom(const FRandomStream& Stream) const
	{
		const int32 RandIndex = Stream.RandRange(0, Colors.Num()-1);
		return GetColor(RandIndex);
	}

};
