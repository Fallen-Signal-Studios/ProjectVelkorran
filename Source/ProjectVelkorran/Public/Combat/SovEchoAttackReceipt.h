// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GAS/SovAttackReceiptSource.h"
#include "SovEchoAttackReceipt.generated.h"
class UNarrativeCombatAbility;

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
private:
	TWeakObjectPtr<UNarrativeCombatAbility> SourceAbility;
	TWeakObjectPtr<AActor> SourceActor;
	FGuid CapturedAttackId;
	bool bHeavyAttack = false;
};
