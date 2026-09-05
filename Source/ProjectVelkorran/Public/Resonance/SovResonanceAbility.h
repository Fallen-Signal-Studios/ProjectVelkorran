// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Effects/SovGameplayEffect_CinderWard.h"
#include "SovResonanceAbility.generated.h"
class USovResonanceComponent;

/** Private activation source: both separate ASCs receive the same interaction ID. */
UCLASS(Transient, NotBlueprintable)
class PROJECTVELKORRAN_API USovResonanceTicket : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<USovResonanceComponent> Coordinator;
	TWeakObjectPtr<UAbilitySystemComponent> ExpectedASC;
	FGuid InteractionId;
};

/** Short, source-validated participation, never granted by a progression tree. */
UCLASS(NotBlueprintable)
class PROJECTVELKORRAN_API USovResonanceAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	USovResonanceAbility();
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEnd, bool bCanceled) override;
private:
	UPROPERTY(Transient) TObjectPtr<USovResonanceTicket> Ticket;
};

/** Distinct effect identity preserves an unrelated Cinder Slam ward and its duration. */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayEffect_ResonanceCorridor : public USovGameplayEffect_CinderWard
{
	GENERATED_BODY()
public:
	USovGameplayEffect_ResonanceCorridor();
};
