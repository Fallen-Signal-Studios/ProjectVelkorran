// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AI/Mass/Peds/NarrativePedFragments.h"
#include "Components/ActorComponent.h"
#include "UObject/Interface.h"
#include "NarrativeMassParticipantBridge.generated.h"

class FMassEntityManager;

/** Project-owned lifecycle policy; NarrativeArsenal does not depend on a campaign module. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UNarrativeMassParticipantOwner : public UInterface
{
	GENERATED_BODY()
};

class NARRATIVEARSENAL_API INarrativeMassParticipantOwner
{
	GENERATED_BODY()

public:
	virtual bool AcceptMassRepresentation(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		AActor& Actor, const FNarrativeMassParticipantFragment& Participant) = 0;

	virtual bool IsMassRepresentationCurrent(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const AActor& Actor, const FNarrativeMassParticipantFragment& Participant) const = 0;

	virtual void ReleaseMassRepresentation(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		AActor& Actor, const FNarrativeMassParticipantFragment& Participant) {}
};

/**
 * Actor-side receipt for one exact entity/owner/generation association. Mass can
 * reuse a plain AActor without a UMassAgentComponent, so a weak actor alone is not
 * a sufficient identity check for deferred representation commands.
 */
UCLASS(Transient, NotBlueprintable)
class NARRATIVEARSENAL_API UNarrativeMassParticipantReceiptComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNarrativeMassParticipantReceiptComponent();

	bool Bind(TSharedRef<FMassEntityManager> InEntityManager, FMassEntityHandle InEntity);
	void Reset();
	bool IsCurrent(FMassEntityManager& InEntityManager) const;
	bool MatchesReceipt(FMassEntityManager& InEntityManager, uint64 ExpectedEpoch) const;
	uint64 GetBindingEpoch() const { return BindingEpoch; }
	FMassEntityHandle GetEntity() const { return Entity; }
	const FNarrativeMassParticipantFragment& GetParticipant() const { return Participant; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UPROPERTY(Transient)
	FNarrativeMassParticipantFragment Participant;

	FMassEntityHandle Entity;
	TWeakPtr<FMassEntityManager> EntityManager;
	uint64 BindingEpoch = 0;
	bool bBound = false;
};
