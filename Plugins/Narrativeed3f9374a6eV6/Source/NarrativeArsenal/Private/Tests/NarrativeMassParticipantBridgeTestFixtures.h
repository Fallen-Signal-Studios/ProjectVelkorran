// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AI/Mass/Peds/NarrativeMassParticipantBridge.h"
#include "GameFramework/Actor.h"
#include "NarrativeMassParticipantBridgeTestFixtures.generated.h"

UCLASS(Transient, NotBlueprintable)
class ANarrativeMassParticipantTestOwner : public AActor, public INarrativeMassParticipantOwner
{
	GENERATED_BODY()

public:
	bool bAccept = true;
	int32 ReleaseCalls = 0;
	FMassEntityHandle CurrentEntity;
	FNarrativeMassParticipantFragment CurrentParticipant;
	TWeakObjectPtr<AActor> CurrentActor;

	virtual bool AcceptMassRepresentation(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		AActor& Actor, const FNarrativeMassParticipantFragment& Participant) override
	{
		if (!bAccept) { return false; }
		CurrentEntity = Entity;
		CurrentParticipant = Participant;
		CurrentActor = &Actor;
		return true;
	}

	virtual bool IsMassRepresentationCurrent(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		const AActor& Actor, const FNarrativeMassParticipantFragment& Participant) const override
	{
		return bAccept && CurrentEntity == Entity && CurrentActor.Get() == &Actor
			&& CurrentParticipant.HasSameIdentity(Participant);
	}

	virtual void ReleaseMassRepresentation(FMassEntityManager& EntityManager, FMassEntityHandle Entity,
		AActor& Actor, const FNarrativeMassParticipantFragment& Participant) override
	{
		++ReleaseCalls;
		if (CurrentEntity == Entity && CurrentActor.Get() == &Actor && CurrentParticipant.HasSameIdentity(Participant))
		{
			CurrentEntity = FMassEntityHandle();
			CurrentActor.Reset();
		}
	}
};
