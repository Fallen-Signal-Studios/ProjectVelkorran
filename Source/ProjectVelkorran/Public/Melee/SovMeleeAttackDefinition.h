// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SovMeleeAttackDefinition.generated.h"
class UAnimMontage;
class UGameplayAbility;
class USkeletalMeshComponent;
/** One swept blade edge: two socket (or bone) points on the trace mesh, each optionally offset in its socket's space. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovMeleeTraceSegment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName StartSocket;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName EndSocket;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FVector StartOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FVector EndOffset=FVector::ZeroVector;
    /** Component-space start/end of this edge on Mesh; false when a socket is missing or the result is not finite. */
    bool ResolveComponentSpace(const USkeletalMeshComponent& Mesh,FVector& OutStart,FVector& OutEnd) const;
};
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovMeleeAttackNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") TObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName Section;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName StartSocket=TEXT("blade_root");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FName EndSocket=TEXT("blade_tip");
    /** Offsets in each socket's own space, for weapon meshes that carry a grip socket but no blade-tip socket. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FVector StartOffset=FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") FVector EndOffset=FVector::ZeroVector;
    /** Further edges swept with the primary one and sharing its hit ledger, e.g. both ends of a double blade. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") TArray<FSovMeleeTraceSegment> AdditionalSegments;
    /** The primary edge followed by AdditionalSegments. */
    TArray<FSovMeleeTraceSegment> TraceSegments() const;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="2",ClampMax="60")) float TraceRadius=8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float Startup=.15f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0.01")) float Active=.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float Recovery=.3f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") float BranchOpen=.35f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee") float BranchClose=.6f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0",ClampMax="0.1")) float HitConfirmAdvance=.05f;
    /**
     * Seconds from node start during which the attacker's poise cannot break, in the same space as
     * BranchOpen/BranchClose. A window exists only when Close is greater than Open, so a node with
     * both left at zero has no super armour and behaves exactly as before.
     *
     * The tag this drives was already read by the damage resolver, the Cinder Slam, Selene's payload
     * and the thermal fracture - and added by nothing at all, so the committed-action armour TDD 6.6
     * asks for did not exist (audit PC2-13).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float SuperArmorOpen=0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Melee",meta=(ClampMin="0")) float SuperArmorClose=0.f;
    /** Whether this node arms at all. Kept separate so the predicate below reads honestly. */
    bool HasSuperArmorWindow() const
    { return FMath::IsFinite(SuperArmorOpen) && FMath::IsFinite(SuperArmorClose) && SuperArmorClose > SuperArmorOpen; }
    /** Half-open on the close edge, so a window ending exactly at another's start never overlaps it. */
    bool IsSuperArmored(float Elapsed) const
    { return HasSuperArmorWindow() && FMath::IsFinite(Elapsed) && Elapsed >= SuperArmorOpen && Elapsed < SuperArmorClose; }
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
