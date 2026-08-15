// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include <GroomAsset.h>
#include <GameplayTagContainer.h>
#include "CharacterCreator/CharacterCreatorAttributes.h"
#include "CharacterAppearance.generated.h"

class ANarrativeCharacter;
class UCharacterCreatorColorSwatch;

/**
 * Defines what a character should look like without any items on - ie their default skin. 
 */
UCLASS(Blueprintable, BlueprintType)
class NARRATIVEARSENAL_API UCharacterAppearanceBase : public UDataAsset
{
	GENERATED_BODY()

public:


};

/**TODO - consider moving this data into CharacterVisual class and having that class manage all of this data - having 2 seperate places that define appearance is possibly overcomplicating things?*/
USTRUCT(BlueprintType)
struct  FCharacterCreatorVariation_Mesh
{
	GENERATED_BODY();

	FCharacterCreatorVariation_Mesh() {};

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Attribute")
	TArray<FCharacterCreatorAttribute_Mesh> RandomMeshes;

	/* helpers */
	FCharacterCreatorAttribute_Mesh GetMesh(const int32 Index) const { return RandomMeshes.IsValidIndex(Index)? RandomMeshes[Index] : FCharacterCreatorAttribute_Mesh(); }
	
	FCharacterCreatorAttribute_Mesh Get() const
	{
		const int32 RandIndex = FMath::RandRange(0, RandomMeshes.Num()-1);
		return GetMesh(RandIndex);
	}
	
	FCharacterCreatorAttribute_Mesh Get(const FRandomStream& Stream) const
	{
		const int32 RandIndex = Stream.RandRange(0, RandomMeshes.Num()-1);
		return GetMesh(RandIndex);
	}
	/* helpers */
};

// random float value from range
USTRUCT(BlueprintType)
struct FScalarVariation
{
	GENERATED_BODY()

	// a float value will be randomly selected from the range.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="ScalarVariation")
	FFloatRange Range;
	
	FScalarVariation()
	{
		// let the property specifier know that it should be in exclusive mode immediately.
		constexpr float Val = 0;
		Range = FFloatRange(FFloatRangeBound::Exclusive(Val), FFloatRangeBound::Exclusive(Val));
	}
	
	/* helpers */
	float Get() const { return FMath::RandRange(Range.GetLowerBoundValue(), Range.GetUpperBoundValue());	}
	float Get(const FRandomStream& Stream) const { return Stream.RandRange(Range.GetLowerBoundValue(), Range.GetUpperBoundValue()); }
	/* helpers */	
};

// random vector from asset or list of vectors
USTRUCT(BlueprintType)
struct FVectorVariation
{
	GENERATED_BODY()

	// when a swatch asset is selected, a random vector is pulled from the array of vectors from the swatch.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="VectorVariation")
	TObjectPtr<UCharacterCreatorColorSwatch> Swatch;

	// list of vectors to randomly pull from.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="VectorVariation", meta=(EditCondition="Swatch == nullptr", EditConditionHides))
	TArray<FLinearColor> Vectors;

	/* helpers */
	FLinearColor GetVector(const int32 Index) const { return Vectors.IsValidIndex(Index)? Vectors[Index] : FLinearColor::White; }
	FLinearColor Get() const;
	FLinearColor Get(const FRandomStream& Stream) const;
	/* helpers */
};

//A set of various appearance pieces that the appearance asset will use to generate a random appearance
USTRUCT(BlueprintType)
struct FCharacterCreatorVariationSet
{

	GENERATED_BODY()

public: 

	FCharacterCreatorVariationSet() {};

	//The mesh variations to choose from 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Attribute", meta = (Categories = "Narrative.Equipment.Slot.Mesh", ForceInlineRow))
	TMap<FGameplayTag, FCharacterCreatorVariation_Mesh> Meshes;

	////The grooms to apply to the character
	//UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.Equipment.Slot.Groom", ForceInlineRow))
	//TMap<FGameplayTag, FCharacterCreatorAttribute_Groom> Grooms;

	////The morphs to apply to the character
	//UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	//TArray<FCharacterCreatorAttribute_Morph> Morphs;

	//Global scalar values that morphs and meshes can reference 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.CharacterCreator.Scalars"))
	TMap<FGameplayTag, FScalarVariation> ScalarValues;

	//Global vector values that morphs and meshes can reference 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.CharacterCreator.Vectors"))
	TMap<FGameplayTag, FVectorVariation> VectorValues;
};

/**
 * Defines what a character should look like without any items on - ie their default skin.
 * Also contains basic logic for putting together a random appearance, useful for adding variation. 
 */
UCLASS(Blueprintable, BlueprintType)
class NARRATIVEARSENAL_API UCharacterAppearance : public UCharacterAppearanceBase
{
	GENERATED_BODY()
	
public:

	UCharacterAppearance();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 

	
	/** The attribute set for the character */
	UFUNCTION(BlueprintPure, Category = "Appearance")
	virtual FCharacterCreatorAttributeSet GetAppearanceAttributes(const ANarrativeCharacter* Requester) const;

protected:

	/** The attribute set for the character. If no variations are added, this is the appearance that will be used.  */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FCharacterCreatorAttributeSet CharacterAttributes;
	
	/** The variations we'll use to generate the attribute set the visual ends up using. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance")
	FCharacterCreatorVariationSet Variations;
};
