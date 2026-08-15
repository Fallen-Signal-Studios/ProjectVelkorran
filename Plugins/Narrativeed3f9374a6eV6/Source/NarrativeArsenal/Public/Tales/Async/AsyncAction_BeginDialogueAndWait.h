// Copyright Narrative Tools 2025.

#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "Tales/Dialogue.h"
#include "Tales/DialogueSM.h"
#include "AsyncAction_BeginDialogueAndWait.generated.h"

class UDialogue;
class UTalesComponent;
struct FDialoguePlayParams;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FBeginDialogueAndWaitSignature, FName, NodeID, bool, bNodeStarted, const EExitDialogueReason, FinishReason);

UCLASS(MinimalAPI, BlueprintType, meta=(ExposedAsyncProxy="AsyncTask"))
class UAsyncAction_BeginDialogueAndWait : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

private:

	UPROPERTY()
	TObjectPtr<UTalesComponent> OwningTalesComponent;
	
	UPROPERTY()
	TSubclassOf<UDialogue> DialogueClass;

protected:

	UPROPERTY(BlueprintAssignable, meta=(ToolTip="called when dialogue finishes playing"))
	FBeginDialogueAndWaitSignature Finished;
	
	UPROPERTY(BlueprintAssignable, meta=(ToolTip="called when a dialogue node starts or ends"))
	FBeginDialogueAndWaitSignature OnDialogueNode;

public:
	
	/* UBlueprintAsyncActionBase */
	virtual void SetReadyToDestroy() override;
	/* UBlueprintAsyncActionBase */

	/**
	 * begins a dialogue with events
	 * @param TalesComponent owning tales component
	 * @param Dialogue dialogue to play
	 * @param PlayParams params
	 * @param bPersistant when true, the action stay alive until EndTask is manually called, allowing it to be used inside loops or if the calling BP goes away
	 * @param bDialogueStarted true if dialogue played
	 * @return the new task
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, meta = (BlueprintInternalUseOnly="true", Category="Dialogues", AdvancedDisplay="bPersistant"))
	static UAsyncAction_BeginDialogueAndWait* BeginDialogueAndWait(UTalesComponent* TalesComponent, TSubclassOf<UDialogue> Dialogue, const FDialoguePlayParams& PlayParams, const bool bPersistant, bool& bDialogueStarted);

	// when called, sets the task to be ready to destroy, removing all bound events
	UFUNCTION(BlueprintCallable, Category="BeginDialogueAndWait")
	void EndTask() { SetReadyToDestroy(); }
	
private:
	
	UFUNCTION()
	void OnNPCDialogueLineStarted(class UDialogue* Dialogue, class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker);

	UFUNCTION()
	void OnNPCDialogueLineFinished(class UDialogue* Dialogue, class UDialogueNode_NPC* Node, const FDialogueLine& DialogueLine, const FSpeakerInfo& Speaker);
	
	UFUNCTION()
	void OnPlayerDialogueLineStarted(class UDialogue* Dialogue, class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);

	UFUNCTION()
	void OnPlayerDialogueLineFinished(class UDialogue* Dialogue, class UDialogueNode_Player* Node, const FDialogueLine& DialogueLine);
	
	UFUNCTION()
	void OnDialogueFinished(class UDialogue* Dialogue, const bool bStartedNewDialogue, const EExitDialogueReason Reason);
	
};
