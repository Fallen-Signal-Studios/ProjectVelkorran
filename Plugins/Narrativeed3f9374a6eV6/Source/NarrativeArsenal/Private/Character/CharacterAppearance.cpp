// Copyright Narrative Tools 2024. 


#include "Character/CharacterAppearance.h"
#include <Engine/SkinnedAssetCommon.h>
#include "Character/NarrativeCharacterVisual.h"
#include "CharacterCreator/CreatorColorSwatch.h"
#include "UnrealFramework/NarrativeCharacter.h"

FLinearColor FVectorVariation::Get() const
{
	if (Swatch)
	{
		return Swatch->GetColorRandom();
	}

	const int32 RandIndex = FMath::RandRange(0, Vectors.Num()-1);
	return GetVector(RandIndex);
}

FLinearColor FVectorVariation::Get(const FRandomStream& Stream) const
{
	if (Swatch)
	{
		return Swatch->GetColorRandom(Stream);
	}
	
	const int32 RandIndex = Stream.RandRange(0, Vectors.Num()-1);
	return GetVector(RandIndex);
}

UCharacterAppearance::UCharacterAppearance()
{
}

#if WITH_EDITOR
void UCharacterAppearance::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	//Make sure all our meshes have materials set on them automatically 
	for (auto& MeshKVP : CharacterAttributes.Meshes)
	{
		if (MeshKVP.Value.Mesh)
		{
			if (!MeshKVP.Value.MeshMaterials.Num())
			{
				for (auto& MeshMat : MeshKVP.Value.Mesh->GetMaterials())
				{
					FCreatorMeshMaterial NewMeshMat;
					NewMeshMat.Material = MeshMat.MaterialInterface;

					MeshKVP.Value.MeshMaterials.Add(NewMeshMat);
				}
			}
		}
	}
}
#endif 

FCharacterCreatorAttributeSet UCharacterAppearance::GetAppearanceAttributes(const ANarrativeCharacter* Requester) const
{
	FCharacterCreatorAttributeSet AppearanceAttributes = CharacterAttributes;

	//Seed a stream so appearance is the same every time. 
	const int32 RandomSeed = Requester->GetCharacterRandomSeed();
	
	for (const auto&[SlotTag, Variation] : Variations.Meshes)
	{
		FCharacterCreatorAttribute_Mesh& MeshAttribute = AppearanceAttributes.Meshes.FindOrAdd(SlotTag);
		MeshAttribute = Variation.Get(RandomSeed);
	}

	for (const auto&[SlotTag, Variation] : Variations.ScalarValues)
	{
		float& ScalarAttribute = AppearanceAttributes.ScalarValues.FindOrAdd(SlotTag);
		ScalarAttribute = Variation.Get(RandomSeed);
	}

	for (const auto&[SlotTag, Variation] : Variations.VectorValues)
	{
		FLinearColor& VectorAttribute = AppearanceAttributes.VectorValues.FindOrAdd(SlotTag);
		VectorAttribute = Variation.Get(RandomSeed);
	}

	return AppearanceAttributes;
}
