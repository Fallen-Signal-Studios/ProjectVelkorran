// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/SovAttackReceiptSource.h"
#include "SovEchoAttackReceipt.generated.h"
class UNarrativeCombatAbility;
struct FSovDamageResult;

/** Immutable identity captured from one live native combat activation, never a reward. */
UCLASS(BlueprintType)
class PROJECTVELKORRAN_API USovEchoAttackReceipt : public UObject, public ISovAttackReceiptSource
{
	GENERATED_BODY()
public:
	/** Use as the damage context SourceObject for every victim of this attack. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Sovereign|Combat|Receipts")
	static USovEchoAttackReceipt* CreateForActiveAbility(UNarrativeCombatAbility* Ability);
	virtual bool GetSovAttackIdentity(const AActor* ExpectedSource, FGuid& OutAttackId) const override;
	/** Only for a result whose AttackId was validated by the damage pipeline before target callbacks. */
	bool MatchesCommittedHeavyAttack(const AActor* ExpectedSource, const FGuid& AttackId) const;
	/** Three retained targets at most; this attack receipt, not the hero, owns reward replay state. */
	bool ConsumeHeavyMultiTargetReward(const UObject* Consumer, uint32 Scope, const FSovDamageResult& Result) const;
private:
	TWeakObjectPtr<UNarrativeCombatAbility> SourceAbility;
	TWeakObjectPtr<AActor> SourceActor;
	FGuid CapturedAttackId;
	bool bHeavyAttack = false;
	mutable TWeakObjectPtr<const UObject> HeavyRewardConsumer;
	mutable uint32 HeavyRewardScope = 0;
	mutable bool bHeavyRewardStarted = false;
	mutable bool bHeavyRewardConsumed = false;
	mutable TSet<TWeakObjectPtr<AActor>> HeavyRewardTargets;
};
