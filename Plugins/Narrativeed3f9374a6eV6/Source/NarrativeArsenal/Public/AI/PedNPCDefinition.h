// Copyright Narrative Tools 2025.

#pragma once

#include "CoreMinimal.h"
#include "AI/NPCDefinition.h"
#include "PedNPCDefinition.generated.h"

/**
 * Special NPC definition intended to be used with Narratives Mass NPCs. 
 */
UCLASS()
class NARRATIVEARSENAL_API UPedNPCDefinition : public UNPCDefinition
{
	GENERATED_BODY()
	
public:

	/**  Baked SM appearance*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ped Appearance")
	TSoftObjectPtr<class UStaticMesh> BakedPedMesh;

	/** High res ped mesh*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ped Appearance")
	TSoftObjectPtr<class USkeletalMesh> PedMesh;

	/** High res ped mesh*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ped Appearance")
	TSoftClassPtr<class UAnimInstance> PedMeshAnimBP;
};
