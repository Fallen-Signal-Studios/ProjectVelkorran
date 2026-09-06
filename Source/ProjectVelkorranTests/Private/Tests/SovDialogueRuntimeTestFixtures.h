// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Tales/Dialogue.h"
#include "Tales/TalesComponent.h"
#include "SovDialogueRuntimeTestFixtures.generated.h"

/** Only seeds graph context; native NPCFinishedTalking/revisions/selection remain production code. */
UCLASS(Transient, NotBlueprintable)
class USovDialogueRuntimeFixture : public UDialogue
{
	GENERATED_BODY()
public:
	void Stage(UTalesComponent* Tales);
	UDialogueNode_Player* Choice(int32 Index) const { return PlayerReplies[Index]; }
	void RetireRevision() { bRepliesPresented = false; ++ReplyPresentationRevision; }
	UDialogueLineCompletionToken* CompletionToken() const { return LineCompletionToken; }
	void RestartPrompt()
	{ ++ReplyPresentationRevision; bRepliesPresented = false; bCurrentLineFinished = false; BeginLineCompletionOwnership(); }
	bool HasDeferredCompletion() const { return DeferredLineCompletion != nullptr; }
	int32 FinishedLines = 0;
protected:
	virtual void FinishDialogueNode_Implementation(UDialogueNode* Node, const FDialogueLine& Line, const FSpeakerInfo& Speaker, AActor* SpeakerActor, AActor* ListenerActor) override
	{ ++FinishedLines; Super::FinishDialogueNode_Implementation(Node, Line, Speaker, SpeakerActor, ListenerActor); }
	// Media is omitted in this deterministic fixture, but native selection/events/revision invalidation still execute.
	virtual void PlayPlayerDialogue_Implementation(UDialogueNode_Player* Reply, const FDialogueLine& Line) override {}
};

UCLASS(Transient, NotBlueprintable)
class USovDialogueRuntimeProbe : public UObject
{
	GENERATED_BODY()
public:
	int32 Notifications = 0;
	int64 Revision = 0;
	UFUNCTION() void Replies(UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& Choices)
	{ ++Notifications; Revision = Dialogue->GetReplyPresentationRevision(); }
};
