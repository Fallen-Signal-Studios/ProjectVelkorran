// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
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
