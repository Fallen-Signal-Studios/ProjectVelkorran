// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "SovCombatTypes.generated.h"

class UNarrativeAttributeSetBase;
class UAbilitySystemComponent;
struct FSovDamageConsumptionReceipt;

/** The action-state defense that intercepted a Sovereign damage transaction. */
UENUM(BlueprintType)
enum class ESovDefenseKind : uint8
{
	None UMETA(DisplayName = "None"),
	Guard UMETA(DisplayName = "Guard"),
	Deflection UMETA(DisplayName = "Deflection")
};

/** Authoritative outcome returned by the project-owned status resolver. */
UENUM(BlueprintType)
enum class ESovStatusApplicationResult : uint8
{
	Applied,
	Refreshed,
	RejectedInvalidRequest,
	RejectedAuthority,
	RejectedDeadTarget,
	RejectedImmune,
	RejectedIneligibleTarget,
	RejectedWeakerExisting,
	RejectedDuplicate
};

/**
 * One exact status application request emitted after a Sovereign damage
 * transaction resolves. Damage-delivered requests reuse that transaction's
 * identity, making (RequestId, StatusTag) a stable deduplication key.
 */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSovStatusApplicationRequest
{
	GENERATED_BODY()

	/** Direct authoritative requests have no damage origin. Damage-produced
	 * requests must still belong to the target life captured by their publisher. */
	bool IsCurrentDamageOrigin() const;

private:
	friend class UNarrativeAttributeSetBase;
	TSharedPtr<FSovDamageConsumptionReceipt> OriginatingDamageReceipt;

public:
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGuid RequestId;

	/** Exact Sov.Status.Apply.* tag requested by the source effect. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGameplayTag StatusTag;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	float Magnitude = 1.f;

	/** Zero delegates duration selection to the status definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	float Duration = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	float EffectLevel = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGameplayEffectContextHandle Context;

	/**
	 * Filtered Sov.Ability.* source identity retained by periodic status
	 * payloads without copying damage channels or Status.Apply tags.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	FGameplayTagContainer SourceAbilityTags;

	/** True when this request was gated by positive applied damage or Poise. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Status")
	bool bRequiresAppliedDamage = false;
};

/** One authoritative, ordered result for a Sovereign damage transaction. */
USTRUCT(BlueprintType)
struct NARRATIVEARSENAL_API FSovDamageResult
{
	GENERATED_BODY()

	/** Copies share one bounded native receipt; a channel separates one consumer's uses. */
	bool ConsumeNativeReceipt(const UObject* Consumer, uint8 Channel = 0) const;
	bool HasNativeReceipt() const;
	/** False after target restoration, avatar replacement, attribute-set removal or destruction.
	 * An expected ASC also fences consumers rebound to another ASC on the same avatar. */
	bool IsCurrentTargetLife(const UAbilitySystemComponent* ExpectedTargetASC = nullptr) const;

private:
	friend class UNarrativeAttributeSetBase;
	/** Only the canonical damage publisher may mint this game-thread receipt. */
	void InitializeNativeReceipt(const UNarrativeAttributeSetBase* TargetAttributes);
	TSharedPtr<FSovDamageConsumptionReceipt> NativeConsumptionReceipt;

public:
	/** Unique identity for this resolved application, even when an Effect Context is reused. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGuid TransactionId;

	/** Authority-produced identity shared by one attack's victims. Invalid when no trusted receipt exists. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGuid AttackId;

	/** Periodic effect delivery (including its initial execution), captured from the authoritative spec. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPeriodicDamage = false;

	/** Target state captured before Stamina, damage, break, death, or presentation callbacks. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer TargetTagsBeforeDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer DamageChannels;

	/** Declared portions rejected by channel immunity or zero weight. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer RejectedDamageChannels;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AcceptedChannelFraction = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer AttackClassifications;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayTagContainer RequestedStatusTags;

	/** True only when this transaction passed action-state defense for a status request. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bStatusApplicationRequested = false;

	/** Poise payload before Guard/Deflection and recovery-floor routing. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float RequestedPoiseDamage = 0.f;


	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FName HitZone = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float BaseDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float ResolvedDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float RequestedShieldDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedShieldDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedHealthDamage = 0.f;

	/** Requested Health damage beyond the target's remaining Health. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float HealthOverkillDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedPoiseDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float AppliedStaminaDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	float ShieldBypassRatio = 0.f;

	/** Exact defense policy that intercepted the hit. None means no defense action succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	ESovDefenseKind DefenseKind = ESovDefenseKind::None;

	/** Legacy Guard compatibility flag. Deflection deliberately leaves this false. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bGuarded = false;

	/** Blueprint convenience mirror for DefenseKind == Deflection. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bDeflected = false;

	/** True for a correctly timed Guard or Deflection. Consult DefenseKind for the action. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPerfectDefense = false;

	/** Stable source-policy mirror used to prevent Echo-spend damage from refunding Echo. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bFromEchoAbility = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bGuardBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bShieldWasTargeted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bShieldBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bPoiseBroken = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bFatal = false;

	/** Authored non-rescuable fatal transaction, captured before damage callbacks. */
	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bCanonicalFatal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	bool bShouldRestartShieldRecharge = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sovereign|Damage")
	FGameplayEffectContextHandle EffectContext;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovDamageResolvedSignature,
	const FSovDamageResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSovStatusApplicationRequestedSignature,
	const FSovStatusApplicationRequest&, Request);

/** Reflected copies must retain native consumption ownership, not only UPROPERTY fields. */
template<> struct TStructOpsTypeTraits<FSovDamageResult> : TStructOpsTypeTraitsBase2<FSovDamageResult>
{
	enum { WithCopy = true };
};

template<> struct TStructOpsTypeTraits<FSovStatusApplicationRequest> : TStructOpsTypeTraitsBase2<FSovStatusApplicationRequest>
{
	enum { WithCopy = true };
};
