// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Tests/SovDialogueRuntimeTestFixtures.h"

void USovDialogueRuntimeFixture::Stage(UTalesComponent* Tales)
{
	OwningComp = Tales;
	OwningController = Tales->GetOwningController();
	OwningPawn = Tales->GetOwningPawn();
	Tales->CurrentDialogue = this;
	bDeinitialized = false;
	bFreeMovement = true;
	RootDialogue = NewObject<UDialogueNode_NPC>(this);
	RootDialogue->SetID(TEXT("PressurePrompt"));
	RootDialogue->Line.Text = NSLOCTEXT("SovDialogueTests", "Prompt", "What should we do?");
	RootDialogue->ReplyPressureSeconds = 4.f;
	RootDialogue->SilenceReplyID = TEXT("Silence");
	RootDialogue->OwningDialogue = this;
	RootDialogue->OwningComponent = Tales;
	CurrentNode = RootDialogue;
	CurrentLine = RootDialogue->Line;
	NPCReplies.Add(RootDialogue);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UDialogueNode_Player* Reply = NewObject<UDialogueNode_Player>(this);
		Reply->SetID(Index == 0 ? TEXT("Answer") : TEXT("Silence"));
		Reply->Line.Text = Index == 0 ? NSLOCTEXT("SovDialogueTests", "Answer", "We continue.")
			: NSLOCTEXT("SovDialogueTests", "Silence", "Remain silent.");
		Reply->Line.Duration = ELineDuration::LD_Never;
		Reply->OwningDialogue = this;
		Reply->OwningComponent = Tales;
		PlayerReplies.Add(Reply);
		AvailableResponses.Add(Reply);
		RootDialogue->PlayerReplies.Add(Reply);
	}
	BeginLineCompletionOwnership();
}
