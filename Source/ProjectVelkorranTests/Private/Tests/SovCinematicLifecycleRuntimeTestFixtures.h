// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "World/SovWorldTransitActor.h"
#include "Engine/DamageEvents.h"
#include "SovCinematicLifecycleRuntimeTestFixtures.generated.h"

UCLASS()
class ASovSequenceLifecycleTestActor : public ANarrativeLevelSequenceActor
{
	GENERATED_BODY()
public:
	ASovSequenceLifecycleTestActor(const FObjectInitializer& Initializer) : Super(Initializer) {}
	UPROPERTY() TArray<TObjectPtr<UObject>> TestParticipants;
	virtual TArray<UObject*> GetBoundObjects() const override
	{ TArray<UObject*> Result; for (UObject* Participant : TestParticipants) { Result.Add(Participant); } return Result; }
	void InitializeTestSequence(ULevelSequence* Sequence) { SetSequence(Sequence); InitializePlayer(); }
};
UCLASS()
class USovSequenceLifecycleProbe : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<USovCampaignCinematicComponent> Managed;
	UPROPERTY() TObjectPtr<ASovWorldTransitActor> Transit;
	bool bMutateTransitOnce = true;
	UFUNCTION() void ChangeTransitLock(ESovWorldTransitState State, const FText& Message)
	{
		if (Transit && bMutateTransitOnce)
		{ bMutateTransitOnce = false; Transit->SetLockReason(FText::FromString(TEXT("External lock owner"))); }
	}
	UFUNCTION() void DamageTransitOnChange(ESovWorldTransitState State, const FText& Message)
	{
		if (Transit && bMutateTransitOnce)
		{ bMutateTransitOnce = false; Transit->TakeDamage(10000.f, FDamageEvent(), nullptr, nullptr); }
	}
	UFUNCTION() void AbortManaged() { if (Managed) { Managed->Abort(TEXT("Reentrant pause interruption")); } }
	int32 FinishedCount = 0;
	int32 InterruptedCount = 0;
	int32 FailedCount = 0;
	int32 BlendCount = 0;
	UFUNCTION() void Finished() { ++FinishedCount; }
	UFUNCTION() void Interrupted() { ++InterruptedCount; }
	UFUNCTION() void Failed() { ++FailedCount; }
	UFUNCTION() void Blended() { ++BlendCount; }
};

/** Uses the real tagged binding resolver, unlike the external-ownership test spy. */
UCLASS()
class ASovRequiredCharacterSequenceTestActor : public ANarrativeLevelSequenceActor
{
	GENERATED_BODY()
public:
	void InitializeTestSequence(ULevelSequence* Sequence) { SetSequence(Sequence); InitializePlayer(); }
};
