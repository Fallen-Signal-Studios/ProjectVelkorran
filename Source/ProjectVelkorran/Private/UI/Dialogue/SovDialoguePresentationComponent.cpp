// Copyright Fallen Signal Studios. All Rights Reserved.
#include "UI/Dialogue/SovDialoguePresentationComponent.h"
#include "UI/Dialogue/SovDialogueChoiceWidget.h"
#include "UI/Dialogue/SovDialoguePressurePolicy.h"
#include "UI/Dialogue/SovDialoguePresentationState.h"
#include "Accessibility/SovAccessibleNarrationSubsystem.h"
#include "AI/NPCDefinition.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "Tales/TalesComponent.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

void FSovDialoguePresentationStateDeleter::operator()(FSovDialoguePresentationState* State) const { delete State; }

#define LOCTEXT_NAMESPACE "SovDialoguePresentation"
USovDialoguePresentationComponent::USovDialoguePresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	State.Reset(new FSovDialoguePresentationState());
}
USovDialoguePresentationComponent::~USovDialoguePresentationComponent() = default;

void USovDialoguePresentationComponent::BeginPlay()
{
	Super::BeginPlay();
	ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController()) { SetComponentTickEnabled(false); return; }
	Tales = PC->FindComponentByClass<UTalesComponent>();
	if (!Tales) { SetComponentTickEnabled(false); return; }
	Tales->OnDialogueRepliesAvailable.AddDynamic(this, &ThisClass::OnReplies);
	Tales->OnDialogueBegan.AddDynamic(this, &ThisClass::OnDialogueBegan);
	Tales->OnDialogueFinished.AddDynamic(this, &ThisClass::OnDialogueFinished);
	Tales->OnDialogueOptionSelected.AddDynamic(this, &ThisClass::OnOptionSelected);
	Tales->OnDialogueSuspensionChanged.AddDynamic(this, &ThisClass::OnSuspensionChanged);
	if (USovGameUserSettings* Settings = USovGameUserSettings::Get())
	{ Settings->OnUserSettingsChanged.AddDynamic(this, &ThisClass::OnSettingsChanged); }
	RefreshChoices();
}

void USovDialoguePresentationComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	CancelPresentation();
	if (Tales)
	{
		Tales->OnDialogueRepliesAvailable.RemoveDynamic(this, &ThisClass::OnReplies);
		Tales->OnDialogueBegan.RemoveDynamic(this, &ThisClass::OnDialogueBegan);
		Tales->OnDialogueFinished.RemoveDynamic(this, &ThisClass::OnDialogueFinished);
		Tales->OnDialogueOptionSelected.RemoveDynamic(this, &ThisClass::OnOptionSelected);
		Tales->OnDialogueSuspensionChanged.RemoveDynamic(this, &ThisClass::OnSuspensionChanged);
	}
	if (USovGameUserSettings* Settings = USovGameUserSettings::Get())
	{ Settings->OnUserSettingsChanged.RemoveDynamic(this, &ThisClass::OnSettingsChanged); }
	Tales = nullptr;
	Super::EndPlay(Reason);
}

USovAccessibleNarrationSubsystem* USovDialoguePresentationComponent::Narrator() const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
	return Player ? Player->GetSubsystem<USovAccessibleNarrationSubsystem>() : nullptr;
}

bool USovDialoguePresentationComponent::IsCurrent() const
{
	return !bEndingPlay && Tales && PresentedDialogue.IsValid()
		&& Tales->GetCurrentDialogue() == PresentedDialogue.Get()
		&& PresentedDialogue->IsCurrentReplyPresentation(State->ReplyRevision);
}

void USovDialoguePresentationComponent::RefreshChoices()
{
	if (Tales)
	{
		UDialogue* Dialogue = Tales->GetCurrentDialogue();
		if (IsValid(Dialogue) && Dialogue->AreRepliesPresented()) { OnReplies(Dialogue, Dialogue->AvailableResponses); }
	}
}

void USovDialoguePresentationComponent::OnReplies(UDialogue* Dialogue, const TArray<UDialogueNode_Player*>& Replies)
{
	if (bEndingPlay || !Tales || Dialogue != Tales->GetCurrentDialogue() || !IsValid(Dialogue)
		|| !Dialogue->AreRepliesPresented()) { return; }
	const TArray<UDialogueNode_Player*> SnapshotReplies = Replies;
	const int64 IncomingRevision = Dialogue->GetReplyPresentationRevision();
	const uint64 ExpectedGeneration = Generation + 1;
	CancelPresentation();
	if (Generation != ExpectedGeneration || Tales->GetCurrentDialogue() != Dialogue
		|| !Dialogue->IsCurrentReplyPresentation(IncomingRevision)) { return; }
	SeenDialogue = Dialogue;
	SeenRevision = Dialogue->GetReplyPresentationRevision();
	PresentedDialogue = Dialogue;
	State->ReplyRevision = SeenRevision;
	const FSovUserSettingsSnapshot Settings = USovGameUserSettings::Get()
		? USovGameUserSettings::Get()->GetSettingsSnapshot() : FSovUserSettingsSnapshot();
	State->bNarration = Settings.bMenuNarration;
	UDialogueNode_NPC* NPC = Cast<UDialogueNode_NPC>(Dialogue->GetCurrentNode());
	State->Speaker = LOCTEXT("SpeakerFallback", "Speaker");
	if (NPC)
	{
		const FSpeakerInfo Speaker = Dialogue->GetSpeaker(NPC->GetSpeakerID());
		if (Speaker.NPCDataAsset && !Speaker.NPCDataAsset->NPCName.IsEmpty()) { State->Speaker = Speaker.NPCDataAsset->NPCName; }
	}
	int32 SilenceMatches = 0;
	for (UDialogueNode_Player* Reply : SnapshotReplies)
	{
		if (!IsValid(Reply) || !Dialogue->AvailableResponses.Contains(Reply)) { continue; }
		const FText Text = Reply->GetOptionText(Dialogue);
		if (!IsCurrent()) { CancelPresentation(); return; }
		// Empty/invalid presentation cannot satisfy readiness and must not drive a silence timeout.
		if (Text.IsEmpty()) { CancelPresentation(); return; }
		const FText Hint = Reply->GetHintText(Dialogue);
		if (!IsCurrent()) { CancelPresentation(); return; }
		State->Choices.Add(Hint.IsEmpty() ? Text : FText::Format(LOCTEXT("ChoiceHint", "{0} {1}"), Text, Hint));
		PresentedReplies.Add(Reply);
		if (NPC && !NPC->SilenceReplyID.IsNone() && Reply->GetID() == NPC->SilenceReplyID
			&& !Reply->IsAutoSelect() && Reply->Conditions.IsEmpty() && NPC->PlayerReplies.Contains(Reply))
		{ SilenceReply = Reply; ++SilenceMatches; }
	}
	if (SilenceMatches != 1) { SilenceReply = nullptr; }
	if (PresentedReplies.IsEmpty()) { CancelPresentation(); return; }
	SovDialoguePressure::Mode Timing = SovDialoguePressure::Mode::Standard;
	if (Settings.DialoguePressureMode == ESovDialoguePressureMode::Extended) { Timing = SovDialoguePressure::Mode::Extended; }
	if (Settings.DialoguePressureMode == ESovDialoguePressureMode::Disabled) { Timing = SovDialoguePressure::Mode::Disabled; }
	// Remote clients retain ordinary Tales input but never invent a server-authoritative timeout.
	const bool bValidSilence = SilenceReply && Tales->HasAuthority();
	State->Pressure.Begin(Generation, NPC ? NPC->ReplyPressureSeconds : 0.f, bValidSilence, Timing,
		Settings.DialoguePressureExtension, Settings.DialogueMinimumReadSeconds, State->bNarration);
	ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOwner());
	UNarrativeGameplayHUD* HUD = PC ? PC->GetNarrativeGameplayHUD() : nullptr;
	if (HUD)
	{
		ChoiceWidget = Cast<USovDialogueChoiceWidget>(HUD->OpenMenu(USovDialogueChoiceWidget::StaticClass(), FNarrativeGameplayTags::Get().UI_Layer_Game));
	}
	if (!ChoiceWidget)
	{
		// HUD can appear later in startup. No text and no timeout until a real layer exists.
		SeenRevision = INDEX_NONE;
		CancelPresentation();
		return;
	}
	ChoiceWidget->Present(State->Speaker, State->Choices, Settings.UIScale, Settings.bHighContrastHUD);
	ChoiceWidget->SetTimerText(TimerDescription());
	ChoiceWidget->OnChoiceRequested.BindUObject(this, &ThisClass::Choose);
	ChoiceWidget->OnSelectionChanged.BindUObject(this, &ThisClass::OnFocusedChoice);
	ChoiceWidget->OnPresentationRemoved.BindUObject(this, &ThisClass::OnWidgetRemoved);
}

void USovDialoguePresentationComponent::CancelPresentation()
{
	++Generation;
	State->Pressure.Cancel();
	State->bAnnouncementPending = false;
	USovDialogueChoiceWidget* OldWidget = ChoiceWidget;
	ChoiceWidget = nullptr;
	PresentedDialogue.Reset();
	PresentedReplies.Reset();
	SilenceReply = nullptr;
	*State = FSovDialoguePresentationState();
	// Retire our fields before external callbacks can install another presentation.
	if (USovAccessibleNarrationSubsystem* Speech = Narrator()) { Speech->Cancel(this); }
	if (OldWidget) { OldWidget->Retire(); }
}

void USovDialoguePresentationComponent::OnWidgetRemoved() { CancelPresentation(); }
void USovDialoguePresentationComponent::OnDialogueBegan(UDialogue* Dialogue)
{ if (PresentedDialogue.Get() != Dialogue) { CancelPresentation(); } }
void USovDialoguePresentationComponent::OnDialogueFinished(UDialogue* Dialogue, bool, EExitDialogueReason)
{ if (PresentedDialogue.Get() == Dialogue) { CancelPresentation(); } }
void USovDialoguePresentationComponent::OnOptionSelected(UDialogue* Dialogue, UDialogueNode_Player*)
{ if (PresentedDialogue.Get() == Dialogue) { CancelPresentation(); } }
void USovDialoguePresentationComponent::OnSettingsChanged(const FSovUserSettingsSnapshot&)
{ if (IsCurrent()) { RefreshChoices(); } }

void USovDialoguePresentationComponent::OnSuspensionChanged(UDialogue* Dialogue, bool bSuspended)
{
	if (Dialogue == PresentedDialogue.Get() && bSuspended) { SuspendPresentation(); }
}

void USovDialoguePresentationComponent::SuspendPresentation()
{
	State->bSuspended = true;
	State->bFullAnnouncementComplete = false;
	State->bAnnouncementPending = false;
	State->Pressure.SuspendReading();
	++State->SpeechGeneration;
	if (USovAccessibleNarrationSubsystem* Speech = Narrator()) { Speech->Cancel(this); }
	State->SpeechRequest.Invalidate();
	if (ChoiceWidget) { ChoiceWidget->SetChoicesEnabled(false); ChoiceWidget->SetTimerText(TimerDescription()); }
}

bool USovDialoguePresentationComponent::IsPresentationPaused() const
{
	if (!GetWorld() || UGameplayStatics::IsGamePaused(GetWorld()) || !PresentedDialogue.IsValid()
		|| PresentedDialogue->IsPlaybackSuspended()) { return true; }
	const ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetOwner());
	UNarrativeGameplayHUD* HUD = PC ? PC->GetNarrativeGameplayHUD() : nullptr;
	if (HUD)
	{
		for (FGameplayTag Tag : {FNarrativeGameplayTags::Get().UI_Layer_Menu, FNarrativeGameplayTags::Get().UI_Layer_Modal})
		{
			if (UCommonActivatableWidgetContainerBase* Layer = HUD->GetLayerContainer(Tag))
			{ if (Layer->GetActiveWidget()) { return true; } }
		}
	}
	return false;
}

void USovDialoguePresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (!IsCurrent())
	{
		if (PresentedDialogue.IsValid()) { CancelPresentation(); }
		if (Tales && IsValid(Tales->GetCurrentDialogue()) && Tales->GetCurrentDialogue()->AreRepliesPresented()
			&& (SeenDialogue != Tales->GetCurrentDialogue() || SeenRevision != Tales->GetCurrentDialogue()->GetReplyPresentationRevision()))
		{ RefreshChoices(); }
		return;
	}
	if (IsPresentationPaused() || !ChoiceWidget || !ChoiceWidget->IsTextPresented())
	{
		if (!State->bSuspended) { SuspendPresentation(); }
		return;
	}
	if (State->bSuspended)
	{
		State->bSuspended = false;
		State->bUnsupported = false;
		ChoiceWidget->SetChoicesEnabled(true);
	}
	State->Pressure.TextReady = true;
	if (State->bNarration && !State->bFullAnnouncementComplete && !State->bAnnouncementPending && !State->bUnsupported)
	{ AnnounceChoices(); }
	ChoiceWidget->SetTimerText(TimerDescription());
	if (State->Pressure.Advance(Generation, DeltaTime, false))
	{
		const int32 Index = PresentedReplies.IndexOfByKey(SilenceReply);
		if (Index != INDEX_NONE) { Choose(Index); }
	}
}

FText USovDialoguePresentationComponent::TimerDescription() const
{
	if (State->Pressure.Duration <= 0.) { return LOCTEXT("NoPressure", "No time limit. Choose when ready."); }
	if (State->bSuspended) { return LOCTEXT("PressurePaused", "Response timer paused."); }
	if (State->bUnsupported) { return LOCTEXT("ReaderUnavailable", "Narration unavailable. Response timer will not expire; choose when ready."); }
	if (!State->Pressure.Ready())
	{
		return FText::Format(LOCTEXT("PressureWaiting", "Response timer waits for reading. Then {0} seconds; silence is a valid response."), FText::AsNumber(FMath::CeilToInt(State->Pressure.Remaining())));
	}
	return FText::Format(LOCTEXT("PressureRunning", "{0} seconds remaining. Silence is a valid response."), FText::AsNumber(FMath::CeilToInt(State->Pressure.Remaining())));
}

void USovDialoguePresentationComponent::AnnounceChoices()
{
	if (!IsCurrent() || State->bSuspended || !ChoiceWidget || !ChoiceWidget->IsTextPresented()) { return; }
	USovAccessibleNarrationSubsystem* Speech = Narrator();
	if (!Speech || !Speech->IsSupported()) { State->bUnsupported = true; return; }
	FText Announcement = FText::Format(LOCTEXT("ChoiceIntroduction", "{0}. {1} choices. {2}"),
		State->Speaker, FText::AsNumber(State->Choices.Num()), TimerDescription());
	for (int32 Index = 0; Index < State->Choices.Num(); ++Index)
	{
		Announcement = FText::Format(LOCTEXT("AppendChoice", "{0} Choice {1}: {2}."), Announcement, FText::AsNumber(Index + 1), State->Choices[Index]);
	}
	Announcement = FText::Format(LOCTEXT("AppendSelection", "{0} Selected choice {1}: {2}."), Announcement,
		FText::AsNumber(State->SelectedIndex + 1), State->Choices[State->SelectedIndex]);
	State->bAnnouncementPending = true;
	State->bSelectionAnnouncement = false;
	State->bUnsupported = false;
	State->AnnouncedSelection = State->SelectedIndex;
	const uint64 SpeechGeneration = ++State->SpeechGeneration;
	State->Pressure.SetSpeechComplete(Generation, false);
	const uint64 ExpectedGeneration = Generation;
	const bool bAccepted = Speech->Announce(this, Announcement, State->SpeechRequest,
		FSovNarrationCompletion::CreateUObject(this, &ThisClass::FinishAnnouncement, ExpectedGeneration, SpeechGeneration));
	if (!bAccepted && Generation == ExpectedGeneration && State->SpeechGeneration == SpeechGeneration)
	{ State->bAnnouncementPending = false; State->bUnsupported = true; }
}

void USovDialoguePresentationComponent::FinishAnnouncement(FGuid Request, bool bCompleted, uint64 ExpectedGeneration, uint64 ExpectedSpeechGeneration)
{
	if (Generation != ExpectedGeneration || State->SpeechGeneration != ExpectedSpeechGeneration || !IsCurrent()
		|| !State->bAnnouncementPending || !Request.IsValid() || Request != State->SpeechRequest) { return; }
	State->bAnnouncementPending = false;
	State->SpeechRequest.Invalidate();
	if (!bCompleted || State->bSuspended)
	{
		State->Pressure.SetSpeechComplete(Generation, false);
		// Interruption by another owner's narration never starts pressure or fights their focus.
		State->bUnsupported = !State->bSuspended;
		return;
	}
	if (!State->bSelectionAnnouncement) { State->bFullAnnouncementComplete = true; }
	State->bUnsupported = false;
	State->Pressure.SetSpeechComplete(Generation, State->bFullAnnouncementComplete);
	if (State->AnnouncedSelection != State->SelectedIndex) { OnFocusedChoice(State->SelectedIndex); }
}

void USovDialoguePresentationComponent::OnFocusedChoice(int32 Index)
{
	if (!IsCurrent() || !State->Choices.IsValidIndex(Index)) { return; }
	State->SelectedIndex = Index;
	if (!State->bNarration || State->bSuspended || !State->bFullAnnouncementComplete) { return; }
	USovAccessibleNarrationSubsystem* Speech = Narrator();
	if (!Speech || !Speech->IsSupported()) { State->bUnsupported = true; State->Pressure.SetSpeechComplete(Generation, false); return; }
	// Suspend pressure while a changed selection is spoken. No per-second speech spam.
	State->Pressure.SetSpeechComplete(Generation, false);
	State->bAnnouncementPending = true;
	State->bSelectionAnnouncement = true;
	State->bUnsupported = false;
	State->AnnouncedSelection = Index;
	const uint64 SpeechGeneration = ++State->SpeechGeneration;
	State->SpeechRequest.Invalidate();
	const FText Text = FText::Format(LOCTEXT("Selection", "Selected choice {0} of {1}: {2}. {3}"),
		FText::AsNumber(Index + 1), FText::AsNumber(State->Choices.Num()), State->Choices[Index], TimerDescription());
	const uint64 ExpectedGeneration = Generation;
	const bool bAccepted = Speech->Announce(this, Text, State->SpeechRequest,
		FSovNarrationCompletion::CreateUObject(this, &ThisClass::FinishAnnouncement, ExpectedGeneration, SpeechGeneration));
	if (!bAccepted && Generation == ExpectedGeneration && State->SpeechGeneration == SpeechGeneration)
	{ State->bAnnouncementPending = false; State->bUnsupported = true; }
}

void USovDialoguePresentationComponent::Choose(int32 Index)
{
	if (!IsCurrent() || IsPresentationPaused() || !ChoiceWidget || !ChoiceWidget->IsTextPresented()
		|| !PresentedReplies.IsValidIndex(Index)) { return; }
	UDialogue* Dialogue = PresentedDialogue.Get();
	UDialogueNode_Player* Reply = PresentedReplies[Index];
	if (!Dialogue->CanSelectDialogueOption(Reply) || !State->Pressure.TryCommit(Generation)) { return; }
	const int64 Revision = State->ReplyRevision;
	// Retire FIRST: graph events, selection broadcasts and speech cancellation can all reenter.
	UTalesComponent* OwnerTales = Tales;
	CancelPresentation();
	const uint64 RetiredGeneration = Generation;
	if (!OwnerTales->TrySelectPresentedDialogueOption(Dialogue, Revision, Reply)
		&& Generation == RetiredGeneration && Tales == OwnerTales && OwnerTales->GetCurrentDialogue() == Dialogue
		&& Dialogue->IsCurrentReplyPresentation(Revision))
	{
		// A cancellation callback may suspend the scene just before the graph accepts input.
		// Restore current choices later, not the rejected commit or an obsolete deadline.
		SeenRevision = INDEX_NONE;
	}
}
#undef LOCTEXT_NAMESPACE
