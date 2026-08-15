// Copyright Narrative Tools 2024. 


#include "CharacterCreator/Items/CharacterCreatorItem_Groom.h"
#include <GroomAsset.h>

UCharacterCreatorItem_Groom::UCharacterCreatorItem_Groom()
{

}

#if WITH_EDITOR

void UCharacterCreatorItem_Groom::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UCharacterCreatorItem_Groom, Groom))
	{
		if (UGroomAsset* LoadedGroom = Groom.LoadSynchronous())
		{
			GroomMaterials.Empty();

			int32 Idx = 0;
			for (auto& GroomMat : LoadedGroom->GetHairGroupsMaterials())
			{
				FCreatorMeshMaterialOption GroomMaterial;
				GroomMaterial.Material = GroomMat.Material;
				GroomMaterial.MaterialIdx = Idx;

				GroomMaterials.Add(GroomMaterial);

				++Idx;
			}
		}
	}
}

#endif