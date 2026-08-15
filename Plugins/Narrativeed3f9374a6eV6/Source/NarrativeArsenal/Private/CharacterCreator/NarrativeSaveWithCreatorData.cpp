// Copyright Narrative Tools 2024. 


#include "CharacterCreator/NarrativeSaveWithCreatorData.h"
#include "CharacterCreator/Items/CharacterCreatorItem_Mesh.h"
#include "CharacterCreator/Items/CharacterCreatorItem_Groom.h"


void UNarrativeSaveWithCreatorData::ClearMeshesAndGrooms()
{
	CharacterCreatorAttributes.Meshes.Empty();
	CharacterCreatorAttributes.Grooms.Empty();
}

void UNarrativeSaveWithCreatorData::SetCreatorDataMesh(class UCharacterCreatorItem_Mesh* MeshItem)
{
	if (MeshItem)
	{
		FCharacterCreatorAttribute_Mesh MeshAttribute = MeshItem->MeshAttributes;

		//MeshAttribute.Mesh = MeshItem->Mesh;
		//MeshAttribute.MeshAnimBP = MeshItem->MeshAnimBP;
		//MeshAttribute.bUseLeaderPose = MeshItem->bUseLeaderPose;

		//Convert the editor mesh materials options into materials  
		TArray<FCreatorMeshMaterial> NewMeshMaterials;

		for (auto& MeshMat : MeshItem->MaterialOptions)
		{
			NewMeshMaterials.Add(MeshMat.GetDefaultMaterial());
		}

		for (auto& MeshMorph : MeshItem->Morphs)
		{
			FCreatorMeshMorph NewMorph;
			NewMorph.MorphNames = MeshMorph.MorphNames;
			NewMorph.ScalarTag = MeshMorph.ScalarTag;
			MeshAttribute.Morphs.Add(NewMorph);
		}

		MeshAttribute.MeshMaterials = NewMeshMaterials;

		CharacterCreatorAttributes.Meshes.Add(MeshItem->Slot, MeshAttribute);
	}
}

void UNarrativeSaveWithCreatorData::SetCreatorDataGroom(class UCharacterCreatorItem_Groom* GroomItem)
{
	//Create a runtime attribute we can apply to the character from the provided groom item
	if (GroomItem)
	{
		FCharacterCreatorAttribute_Groom GroomAttribute;

		GroomAttribute.GroomAsset = GroomItem->Groom;
		GroomAttribute.GroomBindingAsset = GroomItem->GroomBinding;

		TArray<FCreatorMeshMaterial> NewGroomMaterials;

		for (auto& GroomMat : GroomItem->GroomMaterials)
		{
			NewGroomMaterials.Add(GroomMat.GetDefaultMaterial());
		}

		GroomAttribute.GroomMaterials = NewGroomMaterials;

		CharacterCreatorAttributes.Grooms.Add(GroomItem->Slot, GroomAttribute);
	}
}

void UNarrativeSaveWithCreatorData::SetCreatorScalarValue(const FGameplayTag& TagID, const float NewValue)
{
	float& FloatValue = CharacterCreatorAttributes.ScalarValues.FindOrAdd(TagID);

	FloatValue = NewValue;
}

void UNarrativeSaveWithCreatorData::SetCreatorVectorValue(const FGameplayTag& TagID, const FLinearColor NewValue)
{
	FLinearColor& VectorValue = CharacterCreatorAttributes.VectorValues.FindOrAdd(TagID);

	VectorValue = NewValue;
}
