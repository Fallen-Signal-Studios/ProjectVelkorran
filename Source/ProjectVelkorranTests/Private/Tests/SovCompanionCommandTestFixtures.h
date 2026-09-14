// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "Companions/SovProtagonistCompanionCharacter.h"
#include "Abilities/SovGameplayAbility_TarrikGuard.h"
#include "SovCompanionCommandTestFixtures.generated.h"

/** Excludes content loading; the native command, ASC and activity selection remain real. */
UCLASS(Transient, NotBlueprintable)
class ASovCompanionCommandTestProxy : public ASovProtagonistCompanionCharacter
{
	GENERATED_BODY()
public:
	void InitializeCommandCombat();
	virtual void PossessedBy(AController* NewController) override { APawn::PossessedBy(NewController); }
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
};

/** An immediately completed defense isolates the command scheduler from montage content. */
UCLASS(Transient, NotBlueprintable)
class USovCompanionCommandTestDefense : public USovGameplayAbility_TarrikGuard
{
	GENERATED_BODY()
public:
	int32 ActivationCount = 0;
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* Event) override;
};
