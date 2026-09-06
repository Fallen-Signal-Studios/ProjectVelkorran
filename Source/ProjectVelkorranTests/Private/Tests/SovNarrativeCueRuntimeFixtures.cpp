#include "Tests/SovNarrativeCueRuntimeFixtures.h"
#include "Tales/TalesComponent.h"
#include "Tales/DialogueSM.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Narrative/SovNarrativeCueComponent.h"
void USovNarrativeCueRuntimeDialogue::Stage(UTalesComponent* Component)
{
	OwningComp = Component; OwningPawn = Component->GetOwningPawn(); OwningController = Component->GetOwningController();
	Component->CurrentDialogue=this;
	bFreeMovement = true; bDeinitialized = false; bShowCinematicBars = false; PlayerSpeakerInfo.OwnedTags.Reset();
	RootDialogue = NewObject<UDialogueNode_NPC>(this); RootDialogue->SetID(TEXT("TestLine"));
	RootDialogue->Line.Text = FText::FromString(TEXT("A recoverable line."));
	RootDialogue->OwningComponent = Component; RootDialogue->OwningDialogue = this;
	NPCReplies.Add(RootDialogue); CurrentNode = RootDialogue; CurrentLine = RootDialogue->Line;
	// Two authored, nonempty replies keep the graph at player choice. A single
	// empty default reply is a routing node and legitimately auto-completes next.
	TestChoice = NewObject<UDialogueNode_Player>(this); TestChoice->SetID(TEXT("TestChoice"));
	TestChoice->Line.Text = FText::FromString(TEXT("Ask about the route."));
	PlayerReplies.Add(TestChoice); AvailableResponses.Add(TestChoice);
	auto* OtherChoice = NewObject<UDialogueNode_Player>(this); OtherChoice->SetID(TEXT("OtherChoice"));
	OtherChoice->Line.Text = FText::FromString(TEXT("Ask about the supplies."));
	PlayerReplies.Add(OtherChoice); AvailableResponses.Add(OtherChoice);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_NPCReplyFinished,
		FTimerDelegate::CreateWeakLambda(this, [this]() { FinishNPCDialogue(); }), 10.f, false);
}
bool USovNarrativeCueRuntimeDialogue::IsNativeLineTimerPaused() const
{ return GetWorld()->GetTimerManager().IsTimerPaused(TimerHandle_NPCReplyFinished); }
bool USovNarrativeCueRuntimeDialogue::IsNativeLineTimerActive() const
{ return GetWorld()->GetTimerManager().IsTimerActive(TimerHandle_NPCReplyFinished); }
void USovNarrativeCueRuntimeDialogue::FinishDialogueNode_Implementation(UDialogueNode* Node,const FDialogueLine& Line,const FSpeakerInfo& Speaker,AActor* SpeakerActor,AActor* ListenerActor)
{
	++FinishNotifications;
	if (bReenterFinish) { FinishNPCDialogue(); }
	Super::FinishDialogueNode_Implementation(Node,Line,Speaker,SpeakerActor,ListenerActor);
}
void USovNarrativeCueRuntimeObserver::ClearDialogueDuringFinish(UDialogue* Dialogue,bool bStartingNew,EExitDialogueReason Reason)
{ if (IsValid(Tales)) { Tales->CurrentDialogue=nullptr; } }
void USovNarrativeCueRuntimeObserver::LoadDuringCueStart(USovNarrativeCue* Cue,AActor* Speaker,const FText& Caption,float Seconds)
{ if (IsValid(Cues)) { Cues->Load_Implementation(); } }
