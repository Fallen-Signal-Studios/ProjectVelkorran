// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SovMeleeAttackDefinition.generated.h"
class UAnimMontage;
class UGameplayAbility;
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovMeleeAttackNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") TObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName Section;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName StartSocket=TEXT("blade_root");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName EndSocket=TEXT("blade_tip");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="2",ClampMax="60")) float TraceRadius=8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float Startup=.15f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0.01")) float Active=.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float Recovery=.3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") float BranchOpen=.35f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") float BranchClose=.6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0",ClampMax="0.1")) float HitConfirmAdvance=.05f;
    /** Negative means this finite combo ends. Otherwise must point to a later node. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") int32 NextNode=INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FGameplayTag FollowUpInput;
    /** Optional legal defensive exit, paid only when the already granted ability can start. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FGameplayTag DefensiveInput;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") TSubclassOf<UGameplayAbility> DefensiveAbility;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float DefensiveCancelCost=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float Damage=18.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float ShieldCoefficient=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float HealthCoefficient=1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float PoiseDamage=10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FGameplayTagContainer DamageChannels;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FGameplayTagContainer AttackClassifications;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") bool bCharged=false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0.1",ClampMax="2")) float FullChargeSeconds=.65f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0.1",ClampMax="3")) float MaximumChargeSeconds=1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="1",ClampMax="4")) float ChargedMultiplier=1.75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0",ClampMax="25")) float MaximumAimCorrection=12.f;
};
/** Finite authored graph using the existing Narrative montage, weapon visual and GAS damage contracts. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovMeleeAttackDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") TArray<FSovMeleeAttackNode> Nodes;
    UFUNCTION(BlueprintPure, Category="Melee") bool Validate(FString& Error) const;
};
