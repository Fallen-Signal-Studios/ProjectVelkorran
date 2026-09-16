// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AI/SovAurelionElitePolicy.h"
#include "GAS/NarrativeCombatAbility.h"
#include "GameplayEffect.h"
#include "SovGameplayAbility_AurelionElite.generated.h"

class ANarrativeCharacter;
class ASovNPCCharacterBase;
class ASovEncounterDirector;
class UNPCDefinition;

/**
 * Shared base for the Aurelion Elite's boss repertoire.
 *
 * These are NPC abilities granted by the elite's ability configuration, not weapon abilities, so
 * they are selected by Narrative's bot attack selection and remain subject to the encounter
 * coordinator: attack tokens, role quotas, pressure relief and the offscreen ranged warning all
 * still apply. None of that is bypassed to make the boss hit harder - a ranged attack from behind
 * an unwarned player is supposed to be refused, and this base keeps it that way.
 */
UCLASS(Abstract)
class PROJECTVELKORRAN_API USovGameplayAbility_AurelionEliteBase : public UNarrativeCombatAbility
{
	GENERATED_BODY()

public:
	USovGameplayAbility_AurelionEliteBase();

	virtual float GetBotAttackMinimumRange_Implementation() const override { return MinimumAttackRange; }
	virtual float GetBotAttackMaximumRange_Implementation() const override { return MaximumAttackRange; }
	virtual bool RequiresBotAttackToken_Implementation() const override { return bBotRequiresAttackToken; }

	/** The phase this ability first becomes available; the opening phase stays readable. */
	UFUNCTION(BlueprintPure, Category = "Sovereign|Aurelion Elite")
	uint8 GetRequiredPhase() const { return static_cast<uint8>(RequiredPhase); }

	/** Authority-only: the living hostile this boss attack would act on, or null. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Aurelion Elite")
	AActor* GetCurrentBossTarget() const { return BossTarget.Get(); }

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** Runs on authority with a validated avatar and target. Return false to end without committing. */
	virtual bool ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target) { return false; }

	/** The elite's current phase, read from its live health fraction and never walked backwards. */
	SovAurelionElitePolicy::EPhase ResolvePhase() const;

	/** The encounter director owning this avatar as a participant, if any. */
	ASovEncounterDirector* ResolveOwningDirector() const;

	AActor* FindBossTarget(AActor* SourceActor) const;

	/** Cheapest correct guard: a boss ability never acts while its own phase forbids it. */
	bool IsPhaseAdmitted() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MinimumAttackRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Targeting", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumAttackRange = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDuration = 8.f;

	/** Phase gate. First means always available.
	 * Deliberately not a UPROPERTY: the scaling policy is an engine-free header covered by portable
	 * tests, so its enum is a plain C++ type. These abilities are configured in their constructors. */
	SovAurelionElitePolicy::EPhase RequiredPhase = SovAurelionElitePolicy::EPhase::First;

	UPROPERTY(Transient) TWeakObjectPtr<AActor> BossTarget;
	double NextAllowedActivationTime = 0.;
};

/**
 * Ground slam: a radial payload around the elite that punishes standing close during a phase.
 * Mirrors the established Cinder Slam shape - overlap, dedupe by ability system, hostility and
 * line of sight, distance falloff - so damage is resolved the same way the player's own slam is.
 */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayAbility_AurelionEliteSlam : public USovGameplayAbility_AurelionEliteBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_AurelionEliteSlam();

	UFUNCTION(BlueprintPure, Category = "Sovereign|Aurelion Elite|Slam")
	float GetSlamRadius() const { return SlamRadius; }

protected:
	virtual bool ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Slam", meta = (ClampMin = "0.0", Units = "cm"))
	float SlamRadius = 520.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Slam", meta = (ClampMin = "0.0"))
	float SlamDamage = 34.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Slam", meta = (ClampMin = "0.0"))
	float SlamPoiseDamage = 45.f;

	/** Damage retained at the edge of the radius, so positioning matters rather than only distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Slam", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumSlamDamageFraction = .45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Slam")
	TSubclassOf<UGameplayEffect> RadialDamageEffectClass;
};

/**
 * Ranged lance: an authoritative hitscan at the current target.
 *
 * Declared as ranged pressure, so the encounter coordinator will refuse it when the elite is
 * offscreen and no readable warning has been acknowledged. That refusal is intended.
 */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayAbility_AurelionEliteLance : public USovGameplayAbility_AurelionEliteBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_AurelionEliteLance();

protected:
	virtual bool ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Lance", meta = (ClampMin = "0.0"))
	float LanceDamage = 22.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Lance", meta = (ClampMin = "0.0"))
	float LancePoiseDamage = 12.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Lance", meta = (ClampMin = "0.0", Units = "cm"))
	float LanceTraceRadius = 18.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Lance")
	TSubclassOf<UGameplayEffect> LanceDamageEffectClass;
};

/**
 * Summons adds during the fight.
 *
 * The encounter system requires every victory participant to exist before the fight starts, and
 * that rule is not bent here: summoned adds are registered as attempt actors, never as
 * participants. They pressure the player and are cleaned up with the attempt; killing them is
 * never required to win, and the boss cannot be starved of a victory condition by them.
 *
 * The add is the authored Aurelion Enforcer, whose transitive content dependencies are all
 * version controlled, so a summon behaves identically on another machine.
 */
UCLASS()
class PROJECTVELKORRAN_API USovGameplayAbility_AurelionEliteSummon : public USovGameplayAbility_AurelionEliteBase
{
	GENERATED_BODY()

public:
	USovGameplayAbility_AurelionEliteSummon();

	/** Living summons this elite currently owns. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Aurelion Elite|Summon")
	int32 GetLivingSummonCount() const;

protected:
	virtual bool ExecuteBossPayload(ANarrativeCharacter* Avatar, AActor* Target) override;

	/** Authored add definition. Its own NPCClassPath supplies the class that is spawned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Summon")
	TSoftObjectPtr<UNPCDefinition> SummonDefinition;

	/** Hard ceiling on living summons regardless of phase, so the arena cannot be buried. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Summon", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaximumLivingSummons = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Aurelion Elite|Summon", meta = (ClampMin = "100.0", Units = "cm"))
	float SummonDistance = 420.f;

	UPROPERTY(Transient) TArray<TWeakObjectPtr<ASovNPCCharacterBase>> Summoned;
};
