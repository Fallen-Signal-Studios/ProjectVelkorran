// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "UObject/Interface.h"
#include "GameplayAbilitySpec.h"
#include "SovBotAttackCoordinator.generated.h"
class UNarrativeAbilitySystemComponent;
class UNarrativeCombatAbility;
class AActor;
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class USovBotAttackCoordinator : public UInterface { GENERATED_BODY() };
/** Encounter composition admission, orthogonal to Narrative's existing attack token lease. */
class NARRATIVEARSENAL_API ISovBotAttackCoordinator
{
	GENERATED_BODY()
public:
	virtual bool CanAdmitAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) const = 0;
	virtual FGuid ReserveAttack(UNarrativeAbilitySystemComponent* Source, AActor* Target,
		const UNarrativeCombatAbility* Ability, FGameplayAbilitySpecHandle Handle) = 0;
	virtual void ReleaseAttack(FGuid ReservationId) = 0;
	virtual bool IsAttackReservationCurrent(FGuid ReservationId, const UNarrativeAbilitySystemComponent* Source,
		const AActor* Target, FGameplayAbilitySpecHandle Handle) const = 0;
};
