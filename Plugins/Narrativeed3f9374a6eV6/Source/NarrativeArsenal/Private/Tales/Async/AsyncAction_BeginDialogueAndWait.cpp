// Copyright Narrative Tools 2025.

#include "Tales/Async/AsyncAction_BeginDialogueAndWait.h"
#include "Tales/Dialogue.h"
#include "Tales/TalesComponent.h"

void UAsyncAction_BeginDialogueAndWait::SetReadyToDestroy()
{
	// un bind all assigned events
	if (OwningTalesComponent)
	{
		OwningTalesComponent->OnNPCDialogueLineStarted.RemoveAll(this);
		OwningTalesComponent->OnNPCDialogueLineFinished.RemoveAll(this);
		OwningTalesComponent->OnPlayerDialogueLineStarted.RemoveAll(this);
		OwningTalesComponent->OnPlayerDialogueLineFinished.RemoveAll(this);
		OwningTalesComponent->OnPlayerDialogueLineFinished.RemoveAll(this);
	}
	
	Super::SetReadyToDestroy();
}

UAsyncAction_BeginDialogueAndWait* UAsyncAction_BeginDialogueAndWait::BeginDialogueAndWait(UTalesComponent* TalesComponent, TSubclassOf<UDialogue> Dialogue, const FDialoguePlayParams& PlayParams, const bool bPersistant, bool& bDialogueStarted)
{
	UAsyncAction_BeginDialogueAndWait* Action = TalesComponent && Dialogue? NewObject<UAsyncAction_BeginDialogueAndWait>() : nullptr;
	
	if (Action)
	{
		Action->OwningTalesComponent = TalesComponent;
		Action->DialogueClass = Dialogue;

		if (bPersistant)
		{
			Action->RegisterWithGameInstance(TalesComponent);
		}

		// assign to relevant events
		TalesComponent->OnNPCDialogueLineStarted.AddDynamic(Action, &UAsyncAction_BeginDialogueAndWait::OnNPCDialogueLineStarted);
		TalesComponent->OnNPCDialogueLineFinished.AddDynamic(Action, &UAsyncAction_BeginDialogueAndWait::OnNPCDialogueLineFinished);
		TalesComponent->OnPlayerDialogueLineStarted.AddDynamic(Action, &UAsyncAction_BeginDialogueAndWait::OnPlayerDialogueLineStarted);
		TalesComponent->OnPlayerDialogueLineFinished.AddDynamic(Action, &UAsyncAction_BeginDialogueAndWait::OnPlayerDialogueLineFinished);
		TalesComponent->OnDialogueFinished.AddDynamic(Action, &UAsyncAction_BeginDialogueAndWait::OnDialogueFinished);
		bDialogueStarted = TalesComponent->BeginDialogue(Dialogue, PlayParams);

		// dialogue did not play, clear any events
		if (!bDialogueStarted)
		{
			Action->EndTask();
		}
	}

	return Action;
}

void UAsyncAction_BeginDialogueAndWait::OnNPCDialogueLineStarted(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{
	if (OnDialogueNode.IsBound() && Dialogue->GetClass() == DialogueClass)
	{
		OnDialogueNode.Broadcast(Node->GetID(), true, EExitDialogueReason::EDR_NoLines);
	}
}

void UAsyncAction_BeginDialogueAndWait::OnNPCDialogueLineFinished(UDialogue* Dialogue, UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker)
{
	if (OnDialogueNode.IsBound() && Dialogue->GetClass() == DialogueClass)
	{
		OnDialogueNode.Broadcast(Node->GetID(), false, EExitDialogueReason::EDR_NoLines);
	}
}

void UAsyncAction_BeginDialogueAndWait::OnPlayerDialogueLineStarted(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& DialogueLine)
{
	if (OnDialogueNode.IsBound() && Dialogue->GetClass() == DialogueClass)
	{
		OnDialogueNode.Broadcast(Node->GetID(), true, EExitDialogueReason::EDR_NoLines);
	}
}

void UAsyncAction_BeginDialogueAndWait::OnPlayerDialogueLineFinished(UDialogue* Dialogue, UDialogueNode_Player* Node, const FDialogueLine& DialogueLine)
{
	if (OnDialogueNode.IsBound() && Dialogue->GetClass() == DialogueClass)
	{
		OnDialogueNode.Broadcast(Node->GetID(), false, EExitDialogueReason::EDR_NoLines);
	}
}

void UAsyncAction_BeginDialogueAndWait::OnDialogueFinished(class UDialogue* Dialogue, const bool bStartedNewDialogue, const EExitDialogueReason Reason)
{
	if (Finished.IsBound() && Dialogue->GetClass() == DialogueClass)
	{
		Finished.Broadcast(NAME_None, false, Reason);
		
		// dialogue is completed, events are no longer needed.
		EndTask();
	}
}
