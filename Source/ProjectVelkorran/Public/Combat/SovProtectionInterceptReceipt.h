// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/SovCombatTypes.h"
#include "SovProtectionInterceptReceipt.generated.h"
class ANarrativeCharacter;
class UAbilitySystemComponent;

/** Native proof that one actual Drone shot would have struck a living ally without Tarrik. */
UCLASS(Transient, NotBlueprintable)
class PROJECTVELKORRAN_API USovProtectionInterceptReceipt : public UObject
{
	GENERATED_BODY()
public:
	/** Repeats the exact shot query and counterfactual query. No caller-provided safety/award flag. */
	static USovProtectionInterceptReceipt* TryCreateForDroneShot(ANarrativeCharacter* Threat,
		AActor* IntendedFocus, const FHitResult& ActualHit, const FVector& Start, const FVector& End, float Radius);
	bool ArmForDamage(const FGameplayEffectContextHandle& Context, UAbilitySystemComponent* Source, UAbilitySystemComponent* Target);
	void Disarm();
	bool ConsumeForProtector(AActor* Protector, const FSovDamageResult& Result, AActor*& OutThreat, AActor*& OutProtected);
	/** Independent read-only subscriber, valid only during delivery of this exact native transaction. */
	bool MatchesCommittedForProtector(AActor* Protector, const FSovDamageResult& Result, AActor*& OutProtected) const;
	UFUNCTION() void ReceiveResult(const FSovDamageResult& Result);
private:
	bool Matches(const FSovDamageResult& Result) const;
	TWeakObjectPtr<AActor> ThreatActor;
	TWeakObjectPtr<AActor> ProtectorActor;
	TWeakObjectPtr<AActor> ProtectedActor;
	TWeakObjectPtr<UAbilitySystemComponent> ThreatASC;
	TWeakObjectPtr<UAbilitySystemComponent> ProtectorASC;
	FGameplayEffectContextHandle ExpectedContext;
	FGuid CommittedTransaction;
	bool bDelivered = false;
	bool bConsumed = false;
};
