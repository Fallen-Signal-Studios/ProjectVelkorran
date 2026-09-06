// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SovObjectiveTypes.generated.h"

UENUM(BlueprintType)
enum class ESovObjectiveState : uint8 { Inactive, Available, Active, Succeeded, Failed, Skipped, Superseded };

UENUM(BlueprintType)
enum class ESovObjectiveType : uint8
{
	General, ReachEscape, EliminateDisable, ProtectHold, InvestigateAuthenticate,
	RetrieveDeliver, ChoosePrioritize, Survive, InteractOperate, FollowEscort, MasteryRescue
};

/** Requests which do not complete a beat. The existing beat journal remains completion-only. */
USTRUCT(BlueprintType)
struct PROJECTVELKORRAN_API FSovObjectiveJournalEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame, BlueprintReadOnly) FGuid EventId;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 Sequence = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 AfterBeatSequence = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly) int32 AfterEvidenceCount = 0;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName MissionId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName BeatId;
	UPROPERTY(SaveGame, BlueprintReadOnly) FGameplayTag Protagonist;
	UPROPERTY(SaveGame, BlueprintReadOnly) ESovObjectiveState PreviousState = ESovObjectiveState::Inactive;
	UPROPERTY(SaveGame, BlueprintReadOnly) ESovObjectiveState State = ESovObjectiveState::Inactive;
	UPROPERTY(SaveGame, BlueprintReadOnly) FName ReasonId;
};
