// Copyright Narrative Tools 2024. 

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AttackComboAnimSet.generated.h"


//Essentially just a pair of montages, one for 1P and one for 3P 
USTRUCT(BlueprintType)
struct FNarrativeCharacterAnimation
{
	GENERATED_BODY()

	//The 1P Montage
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	TObjectPtr<class UAnimMontage> Montage1P;

	//The 3P Montage
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	TObjectPtr<class UAnimMontage> Montage3P;
};

/**
 * Stores a set of animations to make them easily re-usable. Used for combos and flinches in the base tool. 
 */
UCLASS(Blueprintable, BlueprintType)
class NARRATIVEARSENAL_API UNarrativeAnimSet : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UNarrativeAnimSet();

	//The anims in the set. 
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Narrative Anim Set", meta = (ShowOnlyInnerProperties))
	TArray<FNarrativeCharacterAnimation> CharacterAnims;

};
