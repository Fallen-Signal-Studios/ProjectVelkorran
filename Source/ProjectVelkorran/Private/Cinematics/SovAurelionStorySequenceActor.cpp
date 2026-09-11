// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Cinematics/SovAurelionStorySequenceActor.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Framework/SovPlayerController.h"
#include "UI/SovFrontendComponent.h"
#include "UI/SovAccessibilityPresentation.h"
#include "Engine/World.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"

ASovAurelionStorySequenceActor::ASovAurelionStorySequenceActor(const FObjectInitializer& Initializer) : Super(Initializer)
{
    PrimaryActorTick.bCanEverTick = true;
    NarrativeSequenceParams.bAutoPlay = false;
    PlaybackSettings.bAutoPlay = false;
    CampaignCinematic = CreateDefaultSubobject<USovCampaignCinematicComponent>(TEXT("CampaignCinematic"));
    CampaignCinematic->SetAutoActivate(true);
}

bool ASovAurelionStorySequenceActor::ValidateDialogueCues(const TArray<FSovAurelionDialogueCue>& Cues, double SequenceSeconds, FString& Error)
{
    Error.Reset();
    if (Cues.IsEmpty() || Cues.Num() > 128 || !FMath::IsFinite(SequenceSeconds) || SequenceSeconds <= 0.0 || SequenceSeconds > 1800.0)
    { Error = TEXT("A story scene requires a finite sequence and one to 128 dialogue cues."); return false; }
    double PreviousEnd[3] = { 0.0, 0.0, 0.0 };
    for (const auto& Cue : Cues)
    {
        // At most twenty source characters per second, plus a short reading lead-in.
        // Accessibility presentation may retain additional readable pages at a line end.
        const double MinimumReadingSeconds = FMath::Max(2.0, Cue.Text.ToString().Len() / 20.0 + .5);
        const double End = static_cast<double>(Cue.StartSeconds) + Cue.DurationSeconds;
        if (!FMath::IsFinite(Cue.StartSeconds) || !FMath::IsFinite(Cue.DurationSeconds)
            || Cue.StartSeconds < 0.f || Cue.DurationSeconds < MinimumReadingSeconds || Cue.DurationSeconds > 120.f
            || End > SequenceSeconds + KINDA_SMALL_NUMBER || Cue.Speaker.IsEmpty() || Cue.Speaker.ToString().Len() > 96
            || Cue.Text.IsEmpty() || Cue.Text.ToString().Len() > 1600 || static_cast<uint8>(Cue.RequiredPriority) > 2)
        { Error = TEXT("Dialogue must be ordered, non-overlapping, readable, and contained in the sequence playback range."); return false; }
        for (uint8 Branch = 0; Branch < 3; ++Branch)
        {
            if (Cue.RequiredPriority != ESovAurelionRescuePriority::Unset && static_cast<uint8>(Cue.RequiredPriority) != Branch) { continue; }
            if (Cue.StartSeconds < PreviousEnd[Branch]) { Error = TEXT("Dialogue cues overlap on an applicable priority branch."); return false; }
            PreviousEnd[Branch] = End;
        }
    }
    return true;
}

int32 ASovAurelionStorySequenceActor::FindDialogueCue(const TArray<FSovAurelionDialogueCue>& Cues, double Seconds, ESovAurelionRescuePriority Priority)
{
    if (!FMath::IsFinite(Seconds) || Seconds < 0.0) { return INDEX_NONE; }
    for (int32 Index = 0; Index < Cues.Num(); ++Index)
    {
        const auto& Cue = Cues[Index];
        if (Cue.RequiredPriority != ESovAurelionRescuePriority::Unset && Cue.RequiredPriority != Priority) { continue; }
        if (Seconds >= Cue.StartSeconds && Seconds < static_cast<double>(Cue.StartSeconds) + Cue.DurationSeconds) { return Index; }
    }
    return INDEX_NONE;
}

bool ASovAurelionStorySequenceActor::ValidateStoryContent(FString& Error) const
{
    if (!IsValid(CampaignCinematic) || CampaignCinematic->GetOwner() != this || !CampaignCinematic->ValidateConfiguration(Error)) { return false; }
    const ULevelSequence* Asset = CampaignCinematic->Sequence.LoadSynchronous();
    const UMovieScene* Movie = Asset ? Asset->GetMovieScene() : nullptr;
    if (!Movie || Movie->GetPlaybackRange().IsEmpty() || Movie->GetPlaybackRange().GetLowerBound().IsOpen()
        || Movie->GetPlaybackRange().GetUpperBound().IsOpen())
    { Error = TEXT("Story playback range must be finite and non-empty."); return false; }
    const double Duration = (Movie->GetPlaybackRange().GetUpperBoundValue() - Movie->GetPlaybackRange().GetLowerBoundValue()).Value / Movie->GetTickResolution().AsDecimal();
    return ValidateDialogueCues(DialogueCues, Duration, Error);
}

void ASovAurelionStorySequenceActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto Phase = CampaignCinematic ? CampaignCinematic->GetPhase() : ESovCinematicPhase::Idle;
    if (Phase != ESovCinematicPhase::Playing && Phase != ESovCinematicPhase::Paused)
    {
        CurrentCue = INDEX_NONE; DialogueGeneration = MAX_uint64;
        DialogueController.Reset(); DialoguePresentation.Reset();
        // Finite subtitle durations finish through their existing readable-page policy.
        // Never clear a shared speech surface that another scene/dialogue may now own.
        return;
    }
    auto* Player = GetSequencePlayer();
    auto* PC = GetWorld() ? Cast<ASovPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
    const auto* State = PC ? PC->GetCampaignState() : nullptr;
    auto* Surface = PC && PC->GetFrontend() ? PC->GetFrontend()->GetPresentation() : nullptr;
    if (!Player || !PC || !Surface || !State || !State->IsStateValid() || !State->GetActiveMission()
        || State->GetActiveMission()->MissionId != CampaignCinematic->MissionId) { return; }
    if (DialogueGeneration != GetPlaybackGeneration() || DialogueController != PC || DialoguePresentation != Surface)
    { DialogueGeneration = GetPlaybackGeneration(); DialogueController = PC; DialoguePresentation = Surface; CurrentCue = INDEX_NONE; }
    if (Phase == ESovCinematicPhase::Paused) { return; }
    const double Seconds = Player->GetCurrentTime().AsSeconds() - Player->GetStartTime().AsSeconds();
    const int32 NextCue = FindDialogueCue(DialogueCues, Seconds, ASovAurelionPrioritySupport::ReadPriority(State));
    if (NextCue == CurrentCue) { return; }
    CurrentCue = NextCue;
    if (!DialogueCues.IsValidIndex(NextCue)) { return; }
    const auto Cue = DialogueCues[NextCue];
    const float Remaining = static_cast<float>(Cue.StartSeconds + Cue.DurationSeconds - Seconds);
    Surface->PresentSpeech(Cue.Speaker, Cue.Text, FMath::Max(2.f, Remaining), GetActorLocation(), true);
}
