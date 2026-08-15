// Copyright Narrative Tools 2024. 


#include "CharacterCreator/Items/CharacterCreatorItem_Mesh.h"
#include "Engine/SkinnedAssetCommon.h"


#define LOCTEXT_NAMESPACE "CharacterCreatorItem_Mesh"

UCharacterCreatorItem_Mesh::UCharacterCreatorItem_Mesh()
{
	ItemDisplayName = LOCTEXT("MeshDisplayName", "Mesh Item");
}

FCharacterCreatorMeshMaterialParam::FCharacterCreatorMeshMaterialParam()
{

}

#if WITH_EDITOR

void UCharacterCreatorItem_Mesh::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#undef LOCTEXT_NAMESPACE