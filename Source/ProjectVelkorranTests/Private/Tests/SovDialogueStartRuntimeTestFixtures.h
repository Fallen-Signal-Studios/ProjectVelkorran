// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tests/SovDialogueRuntimeTestFixtures.h"
#include "TimerManager.h"
#include "SovDialogueStartRuntimeTestFixtures.generated.h"

/** Real Narrative line-start owner; overrides only designer/media callbacks for deterministic reentry. */
UCLASS(Transient, NotBlueprintable)
class USovDialogueStartRuntimeFixture : public USovDialogueRuntimeFixture
{
	GENERATED_BODY()
public:
	void StartPlayer(UDialogueNode_Player* Node) { PlayPlayerDialogueNode(Node); }
	void StartNPC() { PlayNPCDialogueNode(RootDialogue); }
	float Remaining(bool bNPC) const;
	bool IsTimerPaused(bool bNPC) const;
	FString CurrentText() const { return CurrentLine.Text.ToString(); }
	int32 PlayerMedia = 0;
	int32 NPCMedia = 0;
	int32 PlayerHooks = 0;
	int32 DurationCalls = 0;
	float Duration = 10.f;
	TFunction<void()> DuringPlayerHook;
	TFunction<void()> DuringPlayerMedia;
	TFunction<void()> DuringNPCMedia;
	TFunction<void()> DuringDuration;
	TFunction<void()> DuringVariable;
	TMap<FString, int32> VariableCalls;
protected:
	virtual void PlayPlayerDialogue_Implementation(UDialogueNode_Player*, const FDialogueLine&) override
	{ ++PlayerMedia; auto Callback = MoveTemp(DuringPlayerMedia); if (Callback) { Callback(); } }
	virtual void PlayNPCDialogue_Implementation(UDialogueNode_NPC*, const FDialogueLine&, const FSpeakerInfo&) override
	{ ++NPCMedia; auto Callback = MoveTemp(DuringNPCMedia); if (Callback) { Callback(); } }
	virtual void OnPlayerDialogueLineStarted_Implementation(UDialogueNode_Player*, const FDialogueLine&) override
	{ ++PlayerHooks; auto Callback = MoveTemp(DuringPlayerHook); if (Callback) { Callback(); } }
	virtual float GetLineDuration_Implementation(UDialogueNode*, const FDialogueLine&) override
	{
		++DurationCalls;
		const float Result = Duration;
		auto Callback = MoveTemp(DuringDuration);
		if (Callback) { Callback(); }
		return Result;
	}
	virtual FString GetStringVariable_Implementation(const UDialogueNode*, const FDialogueLine&, const FString& VariableName) override
	{
		++VariableCalls.FindOrAdd(VariableName);
		auto Callback = MoveTemp(DuringVariable);
		if (Callback) { Callback(); }
		return VariableName == TEXT("Hero") ? TEXT("Selene") : TEXT("Dominion");
	}
};

UCLASS(Transient, NotBlueprintable)
class USovDialogueStartRuntimeProbe : public UObject
{
	GENERATED_BODY()
public:
	int32 NPCStarts = 0;
	int32 Resumes = 0;
	bool bExitNPC = false;
	UFUNCTION() void SuspensionChanged(UDialogue* Dialogue, bool bSuspended)
	{ if (!bSuspended) { ++Resumes; } }
	UFUNCTION() void PlayerStarted(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& Line)
	{ Dialogue->ExitDialogue(EExitDialogueReason::EDR_NoLines); }
	UFUNCTION() void NPCStarted(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker)
	{ ++NPCStarts; if (bExitNPC) { Dialogue->ExitDialogue(EExitDialogueReason::EDR_NoLines); } }
};
