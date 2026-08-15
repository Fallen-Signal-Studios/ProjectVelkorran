// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include <GameplayTagContainer.h>
#include <Animation/AnimInstance.h>
#include "CharacterCreatorAttributes.generated.h"

//Base class for a creator attribute - these attributes make up our character creator appearance
USTRUCT(BlueprintType)
struct FCharacterCreatorAttribute
{
	GENERATED_BODY()

	FCharacterCreatorAttribute(){};

	////The ID of this character creator attribute - the creator needs this to find the attribute and set it 
	//UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	//FGameplayTag AttributeID;

	////The slot on the character this needs to apply to
	//UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.Equipment.Slot"))
	//FGameplayTag Slot;

};

//Base class for a morph target that can be changed on the character 
USTRUCT(BlueprintType)
struct FCharacterCreatorAttribute_Morph : public FCharacterCreatorAttribute
{
	GENERATED_BODY()

	FCharacterCreatorAttribute_Morph()
	{
		MorphValue = 0.f;
	};

	//The name of the morph 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Morph")
	FName MorphName;

	//The value we need to set the morph to 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Morph", meta = (ClampMin=0.0, ClampMax=1.0))
	float MorphValue;

};


USTRUCT(BlueprintType)
struct FCreatorMeshMaterialParam
{
	GENERATED_BODY()

	FCreatorMeshMaterialParam(){};

	//The ID of this param
	//UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh Material")
	//FName ParameterID;

	//The parameters to effect - this is an array so you can effect multiple parameters with the one value if needed. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh Material")
	TArray<FName> ParameterNames;

};

USTRUCT(BlueprintType)
struct FCreatorMeshMaterialParam_Vector : public FCreatorMeshMaterialParam
{
	GENERATED_BODY()

	FCreatorMeshMaterialParam_Vector(){};

	//The vector tag we'll read the vector value from 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh Material", meta = (Categories="Narrative.CharacterCreator.Vectors"))
	FGameplayTag VectorTagID;

};

USTRUCT(BlueprintType)
struct FCreatorMeshMaterialParam_Scalar : public FCreatorMeshMaterialParam
{
	GENERATED_BODY()

	FCreatorMeshMaterialParam_Scalar(){};

	//The scalar tag we'll read the scalar value from 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh Material", meta = (Categories="Narrative.CharacterCreator.Scalars"))
	FGameplayTag ScalarTagID;

};


USTRUCT(BlueprintType)
struct FCreatorMeshMorph
{
	GENERATED_BODY()

	FCreatorMeshMorph(){};

	//The scalar value we'll drive the morph with 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Morph Option", meta = (Categories="Narrative.CharacterCreator.Scalars"))
	FGameplayTag ScalarTag;

	//The morph names 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Morph Option")
	TArray<FName> MorphNames;

};


//Defines a material we need to apply to the set mesh, along with any params to set on it  
USTRUCT(BlueprintType)
struct FCreatorMeshMaterial
{
	GENERATED_BODY()

	FCreatorMeshMaterial(){};

	//The material to apply
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	TSoftObjectPtr<class UMaterialInterface> Material;

	//The vector parameter values to apply to the material 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	TArray<FCreatorMeshMaterialParam_Vector> VectorParams;

	//The scalar parameter values to apply to the material 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	TArray<FCreatorMeshMaterialParam_Scalar> ScalarParams;

};


//Base class for a default mesh for a slot 
USTRUCT(BlueprintType)
struct FCharacterCreatorAttribute_Mesh : public FCharacterCreatorAttribute
{
	GENERATED_BODY()

	FCharacterCreatorAttribute_Mesh()
	{
		bUseLeaderPose = true;
		bIsStaticMesh = false; 
	};

	//The mesh we should apply to the character 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh", meta = (EditCondition = "!bIsStaticMesh", EditConditionHides))
	TSoftObjectPtr<class USkeletalMesh> Mesh;

	//The mesh we should apply to the character in 1P - if empty we'll use 3P mesh for 1P. Only used for hands and torso meshes. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, AdvancedDisplay, SaveGame, Category = "Mesh", meta = (EditCondition = "!bIsStaticMesh", EditConditionHides))
	TSoftObjectPtr<class USkeletalMesh> Mesh1P;

	//Should the mesh follow the leader pose component?
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh", meta = (EditCondition = "!bIsStaticMesh", EditConditionHides))
	bool bUseLeaderPose;

	//Should this clothing piece use a static mesh instead of a skeletal mesh
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	bool bIsStaticMesh;
	
	//The static mesh to use. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh", meta = (EditCondition = "bIsStaticMesh", EditConditionHides))
	TSoftObjectPtr<class UStaticMesh> StaticMesh;

	//The attach socket for the mesh, if one is desired.  
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	FName MeshAttachSocket;

	//The attach offset for the mesh, if desired.  
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	FTransform MeshAttachOffset;

	//If we're not using leader pose this is the animBP the mesh should use
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh", meta = (EditCondition="!bUseLeaderPose && !bIsStaticMesh", EditConditionHides))
	TSoftClassPtr<class UAnimInstance> MeshAnimBP;

	//Maps material index -> Creator mesh material. By default if a value doesn't exist in this map we'll just use the material 
	// that was set on the mesh by default and wont set any params on it or anything. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	TArray<FCreatorMeshMaterial> MeshMaterials;

	//The morphs to apply to the mesh
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh", meta = (EditCondition = "!bIsStaticMesh", EditConditionHides))
	TArray<FCreatorMeshMorph> Morphs;

	//Add any slots you need to hide in here. IE a helmet might want Slot.Groom.Hair added as a hide slot, so hair hides when helmet is equipped.  
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, AdvancedDisplay, SaveGame, Category = "Mesh", meta = (Categories = "Narrative.Equipment.Slot"))
	FGameplayTagContainer HideSlots;

};

//Base class for a default groom for a slot 
USTRUCT(BlueprintType)
struct FCharacterCreatorAttribute_Groom : public FCharacterCreatorAttribute
{
	GENERATED_BODY()

	FCharacterCreatorAttribute_Groom(){};

	//The groom we should apply
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Groom")
	TSoftObjectPtr<class UGroomAsset> GroomAsset;

	//The groom binding we should apply
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Groom")
	TSoftObjectPtr<class UGroomBindingAsset> GroomBindingAsset;

	//Materials to apply to created groom 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Mesh")
	TArray<FCreatorMeshMaterial> GroomMaterials;
};



//A set of character attributes ready to be applied to a character  
USTRUCT(BlueprintType)
struct FCharacterCreatorAttributeSet
{

	GENERATED_BODY()

	FCharacterCreatorAttributeSet();

	//The form we've selected in the character creator 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.CharacterCreator.Forms"))
	FGameplayTag FormTag;

	//The visual class the character creator wants us to use 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Form")
	TSubclassOf<class ANarrativeCharacterVisual> CharacterVisualClass;

	//The base skeletal mesh that should be used as the leader pose Character, usually hidden. Mesh will be set to this mesh.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	TObjectPtr<class USkeletalMesh> BaseMesh;

	//Whether or not to hide the base mesh. Usually you'd hide it as your other meshes follow the base mesh. 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	bool bHideBaseMesh;

	//The AnimBP the character mesh should use - TODO this and basement probably should be soft ref?
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	TSubclassOf<class UAnimInstance> BaseMeshAnimBP;

	//The AnimBP the local character should use to allow for a 1P setup 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	TSubclassOf<class UAnimInstance> BaseLocalMeshAnimBP;

	//The anim layer we should apply to the Base Mesh AnimBP by default when unarmed - can be none
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	TSubclassOf<class UNarrativeAnimInstance> UnarmedAnimLayer;

	//The meshes to apply to the character
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.Equipment.Slot.Mesh", ForceInlineRow))
	TMap<FGameplayTag, FCharacterCreatorAttribute_Mesh> Meshes;

	//The grooms to apply to the character
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.Equipment.Slot.Groom", ForceInlineRow))
	TMap<FGameplayTag, FCharacterCreatorAttribute_Groom> Grooms;

	//The morphs to apply to the character
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute")
	TArray<FCharacterCreatorAttribute_Morph> Morphs;

	//Global scalar values that morphs and meshes can reference 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.CharacterCreator.Scalars"))
	TMap<FGameplayTag, float> ScalarValues;

	//Global vector values that morphs and meshes can reference 
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, SaveGame, Category = "Attribute", meta = (Categories = "Narrative.CharacterCreator.Vectors"))
	TMap<FGameplayTag, FLinearColor> VectorValues;

	FORCEINLINE FLinearColor GetVectorValue(const FGameplayTag& Tag) const
	{
		if (VectorValues.Contains(Tag))
		{
			return VectorValues[Tag];
		}

		return FLinearColor();
	};

	FORCEINLINE float GetScalarValue(const FGameplayTag& Tag) const
	{
		if (ScalarValues.Contains(Tag))
		{
			return ScalarValues[Tag];
		}

		return 0.f;
	};
};