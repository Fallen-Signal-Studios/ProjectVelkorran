// Copyright Fallen Signal Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Characters/SovNPCCharacterBase.h"
#include "SovDominionHandler.generated.h"

class USovCommandLinkComponent;
class USovGameplayAbility_DominionHandlerCommandHound;
class UNarrativeAbilitySystemComponent;
struct FGameplayAbilitySpecHandle;

/**
 * Native gameplay profile for the Dominion Handler commander.
 *
 * The Handler owns one authored command link and is the only actor that may
 * order its linked hounds to use Horn Charge. Selection and activation remain
 * server authoritative; Blueprint receives presentation hooks only.
 */
UCLASS(Blueprintable)
class PROJECTVELKORRAN_API ASovDominionHandler : public ASovNPCCharacterBase
{
	GENERATED_BODY()

public:
	ASovDominionHandler(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Command Link")
	USovCommandLinkComponent* GetCommandLinkComponent() const
	{
		return CommandLinkComponent;
	}

	/** Living, friendly, in-range linked actors with an exact Horn Charge ability. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command")
	TArray<AActor*> GetCommandableHounds() const;

	/** Best candidate using link-scoped least-recently-anticipated order. */
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command")
	AActor* FindBestCommandableHound() const;

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Command")
	float GetMaximumCommandDistance() const
	{
		return FMath::Max(MaximumCommandDistance, 0.0f);
	}

	UFUNCTION(BlueprintPure, Category = "Sovereign|Dominion Handler|Command")
	bool RequiresCommandLineOfSight() const
	{
		return bRequireLineOfSightToHound;
	}

	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "Sovereign|Dominion Handler|Command")
	AActor* GetLastCommandedHound() const
	{
		return LastCommandedHound.Get();
	}

protected:
	/** Replicated one-shot cue at the start of the authored command wind-up. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPresentHoundHornChargeAnticipation(
		AActor* IntendedHound,
		FGuid LinkInstanceId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Handler|Presentation", meta = (DisplayName = "Hound Horn Charge Anticipation"))
	void ReceiveHoundHornChargeAnticipation(
		AActor* IntendedHound,
		FGuid LinkInstanceId);

	/** Maximum Handler-to-hound distance for issuing a coordinated attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Command", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumCommandDistance = 2500.0f;

	/** Visibility obstruction prevents orders; the hound's own target trace stays authoritative. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sovereign|Dominion Handler|Command")
	bool bRequireLineOfSightToHound = true;

	/** Cosmetic notification for the selected hound and its resolved target. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPresentHoundHornChargeOrder(
		AActor* OrderedHound,
		AActor* ChargeTarget,
		FGuid LinkInstanceId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Sovereign|Dominion Handler|Presentation", meta = (DisplayName = "Hound Horn Charge Ordered"))
	void ReceiveHoundHornChargeOrdered(
		AActor* OrderedHound,
		AActor* ChargeTarget,
		FGuid LinkInstanceId);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sovereign|Components")
	TObjectPtr<USovCommandLinkComponent> CommandLinkComponent;

private:
	friend class USovGameplayAbility_DominionHandlerCommandHound;
	friend class FSovDominionHandlerHornChargeSpecMultiplicityTest;

	/**
	 * Activates exactly one linked hound's Horn Charge ability.
	 *
	 * This is deliberately private to the native Handler command ability so
	 * Blueprint cannot bypass anticipation, cleanup, or success-only cooldown.
	 */
	bool TryOrderLinkedHoundHornCharge(
		const FGuid& ExpectedLinkInstanceId,
		AActor* ExpectedHound,
		AActor*& OutOrderedHound,
		AActor*& OutChargeTarget);

	bool CanIssueHoundCommands() const;
	bool IsCommandableHound(
		AActor* Candidate,
		bool bAllowTransientAuthorization = false) const;
	bool HasCommandLineOfSightTo(const AActor* Candidate) const;
	bool TryActivateExactHornCharge(
		AActor* Candidate,
		const FGuid& ExpectedLinkInstanceId,
		AActor*& OutChargeTarget) const;
	static FGameplayAbilitySpecHandle FindSingleInactiveExactHornChargeAbility(
		const UNarrativeAbilitySystemComponent* AbilitySystem);
	TArray<AActor*> BuildCommandableHoundCandidates() const;
	uint64 GetCommandAttemptSequenceFor(
		const AActor* Candidate,
		const FGuid& ActiveLinkInstanceId) const;
	void RecordCommandAttempt(
		AActor* Candidate,
		const FGuid& ActiveLinkInstanceId);

	TWeakObjectPtr<AActor> LastCommandedHound;
	TMap<TWeakObjectPtr<AActor>, uint64> CommandAttemptSequenceByHound;
	FGuid CommandAttemptHistoryLinkInstanceId;
	uint64 NextCommandAttemptSequence = 1;
};
