#pragma once
#include "CoreMinimal.h"
#include "Tales/Dialogue.h"
#include "SovNarrativeCueRuntimeFixtures.generated.h"
class UTalesComponent;
class USovNarrativeCueComponent;
class USovNarrativeCue;
/** Seeds graph context only; pause/timers/input gates remain the production Narrative implementation. */
UCLASS(Transient, NotBlueprintable)
class USovNarrativeCueRuntimeDialogue : public UDialogue
{
	GENERATED_BODY()
public:
	void Stage(UTalesComponent* Component);
	bool IsNativeLineTimerPaused() const;
	bool IsNativeLineTimerActive() const;
	UDialogueNode_Player* GetTestChoice() const { return TestChoice; }
	bool bReenterFinish=false;
	int32 FinishNotifications=0;
	virtual void FinishDialogueNode_Implementation(UDialogueNode* Node,const FDialogueLine& Line,const FSpeakerInfo& Speaker,AActor* SpeakerActor,AActor* ListenerActor) override;
private:
	UPROPERTY() TObjectPtr<UDialogueNode_Player> TestChoice;
};
UCLASS(Transient, NotBlueprintable)
class USovNarrativeCueRuntimeObserver : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY() TObjectPtr<UTalesComponent> Tales;
	UPROPERTY() TObjectPtr<USovNarrativeCueComponent> Cues;
	UFUNCTION() void ClearDialogueDuringFinish(UDialogue* Dialogue,bool bStartingNew,EExitDialogueReason Reason);
	UFUNCTION() void LoadDuringCueStart(USovNarrativeCue* Cue,AActor* Speaker,const FText& Caption,float Seconds);
};
