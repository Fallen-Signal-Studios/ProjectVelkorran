// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Narrative/SovNarrativeCueComponent.h"
#include "Narrative/SovNarrativeCuePolicy.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/AudioComponent.h"
#include "Components/SovEchoComponent.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NarrativeGameplayTags.h"
#include "NarrativeSavableActor.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Settings/SovGameUserSettings.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Tales/TalesComponent.h"

USovNarrativeCueComponent::USovNarrativeCueComponent()
{ PrimaryComponentTick.bCanEverTick = true; PrimaryComponentTick.TickInterval = .05f; }
void USovNarrativeCueComponent::BeginPlay() { Super::BeginPlay(); ResolveOwner(); }
bool USovNarrativeCueComponent::ResolveOwner()
{
	ASovPlayerController* PC = Cast<ASovPlayerController>(GetOwner());
	if (bOwnerEndingPlay || !IsValid(PC) || PC->IsActorBeingDestroyed() || !PC->HasAuthority() || PC->GetNetMode() != NM_Standalone) { return false; }
	UTalesComponent* Current = PC->FindComponentByClass<UTalesComponent>();
	if (Tales != Current)
	{
		UTalesComponent* PreviousTales=Tales; UDialogue* PreviousDialogue=OwnedDialogue;
		USovNarrativeCue* PreviousCue=CurrentConversation; const FSovQueuedCue PreviousRequest=ConversationRequest;
		OwnedDialogue=nullptr; CurrentConversation=nullptr; ++Epoch;
		if (IsValid(PreviousTales))
		{ PreviousTales->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished); PreviousTales->OnDialogueBegan.RemoveDynamic(this, &ThisClass::HandleDialogueBegan); }
		Tales = Current;
		RememberUnheard(PreviousCue, &PreviousRequest);
		if (IsValid(PreviousDialogue) && IsValid(PreviousTales) && PreviousTales->GetCurrentDialogue()==PreviousDialogue)
		{ PreviousDialogue->SetPreserveOnInterruption(false); PreviousTales->ExitDialogue(EExitDialogueReason::EDR_PlayerExited); }
		if (Tales)
		{ Tales->OnDialogueFinished.AddUniqueDynamic(this, &ThisClass::HandleDialogueFinished); Tales->OnDialogueBegan.AddUniqueDynamic(this, &ThisClass::HandleDialogueBegan); }
	}
	Controller = PC; return IsValid(Tales) && !Tales->bDialogueOwnerEndingPlay;
}
bool USovNarrativeCueComponent::MatchesContext(const USovNarrativeCue* Cue) const
{
	const USovCampaignStateComponent* State = Controller ? Controller->GetCampaignState() : nullptr;
	return IsValid(Cue) && State && State->IsStateValid() && State->GetActiveMission()
		&& (Cue->RequiredMission.IsNone() || Cue->RequiredMission == State->GetActiveMission()->MissionId)
		&& (!Cue->RequiredProtagonist.IsValid() || Cue->RequiredProtagonist == State->GetActiveProtagonist())
		&& State->HasKnowledge(State->GetActiveProtagonist(), Cue->RequiredKnowledge);
}
bool USovNarrativeCueComponent::MatchesRequestContext(const FSovQueuedCue& Request) const
{
    const auto* State=Controller?Controller->GetCampaignState():nullptr;
    return MatchesContext(Request.Cue) && State && State->GetActiveMission()
        && (!Request.Witness.IsValid() || Request.Witness==State->GetActiveProtagonist())
        && (Request.Mission.IsNone() || Request.Mission==State->GetActiveMission()->MissionId);
}
bool USovNarrativeCueComponent::IsCombatRequired() const
{
	if (!Controller || !Controller->GetPawn()) { return true; }
	const auto* P = Cast<ASovPlayerCharacterBase>(Controller->GetPawn());
	if (!P || !P->IsAlive() || !P->IsCharacterReady() || Controller->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return true; }
	if (P->GetEchoComponent() && P->GetEchoComponent()->IsEncounterActive()) { return true; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{ if (It->HasEncounterPlayer(P) && (It->GetEncounterState() == ESovEncounterState::Active || It->GetEncounterState() == ESovEncounterState::Restoring)) { return true; } }
	const auto* ASC = P->GetNarrativeAbilitySystemComponent();
	return !ASC || ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);
}
AActor* USovNarrativeCueComponent::ResolveSpeaker(FSovQueuedCue& Request) const
{
	AActor* Actor = Request.Cue && Request.Cue->bPlayerSpeaker ? (Controller ? Controller->GetPawn() : nullptr) : Request.Speaker.Get();
	if (!Actor && Request.SpeakerGuid.IsValid())
	{ if (auto* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>()) { Actor = Save->LookupActorByGUID(Request.SpeakerGuid); Request.Speaker = Actor; } }
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld()) { return nullptr; }
	const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor));
	return ASC && ASC->IsDead() ? nullptr : Actor;
}
bool USovNarrativeCueComponent::RequestCue(USovNarrativeCue* Cue, AActor* Speaker, FString& Error)
{
	Error.Reset();
	if (bMutation || !ResolveOwner() || !IsValid(Cue) || !Cue->Validate(Error) || !MatchesContext(Cue)) { return false; }
	if (CurrentBark == Cue || CurrentConversation == Cue || Pending.ContainsByPredicate([Cue](const auto& Item) { return Item.Cue == Cue; }))
	{ Error = TEXT("This cue is already queued or playing."); return false; }
	if (NextAllowed.FindRef(Cue->CueId) > GetWorld()->GetTimeSeconds()) { Error = TEXT("The cue is cooling down."); return false; }
	if (Pending.Num() >= 64) { Error = TEXT("The speech queue is full."); return false; }
	FSovQueuedCue Request; Request.Cue = Cue; Request.Speaker = Speaker; Request.RemainingContextSeconds = Cue->ContextLifetimeSeconds;
	Request.Witness = Controller->GetCampaignState()->GetActiveProtagonist();
	Request.Mission = Controller->GetCampaignState()->GetActiveMission()->MissionId;
	if (IsValid(Speaker) && Speaker->Implements<UNarrativeSavableActor>()) { Request.SpeakerGuid = INarrativeSavableActor::Execute_GetActorGUID(Speaker); }
	if (!Cue->Dialogue && !ResolveSpeaker(Request)) { Error = TEXT("The bark's speaker is unavailable."); return false; }
	if (Cue->bCritical && !Cue->Dialogue && !Cue->bPlayerSpeaker && !Request.SpeakerGuid.IsValid())
	{ Error = TEXT("A recoverable critical bark needs a stable speaker identity."); return false; }
	Pending.Add(Request);
	return true;
}
void USovNarrativeCueComponent::RememberUnheard(USovNarrativeCue* Cue, const FSovQueuedCue* Context)
{
	if (!Cue || !Cue->bCritical || !Cue->bRecordUnheardSummary || Cue->RecordSummary.IsEmpty() || UnheardRecordContexts.Num() >= 256) { return; }
	FSovQueuedCue Record = Context ? *Context : (Cue==CurrentConversation ? ConversationRequest : CurrentRequest); Record.Cue = Cue;
	if (!Record.Witness.IsValid() && Controller && Controller->GetCampaignState()) { Record.Witness = Controller->GetCampaignState()->GetActiveProtagonist(); }
	if (!Record.Witness.IsValid()) { return; }
	if (!UnheardRecordContexts.ContainsByPredicate([&](const auto& Existing) { return Existing.Cue == Cue && Existing.Witness == Record.Witness; }))
	{ UnheardRecordContexts.Add(Record); UnheardRecords.AddUnique(Cue); }
}
TArray<USovNarrativeCue*> USovNarrativeCueComponent::GetUnheardRecords() const
{
	TArray<USovNarrativeCue*> Result;
	const auto* PC = Cast<ASovPlayerController>(GetOwner()); const auto* State = PC ? PC->GetCampaignState() : nullptr;
	if (!State || !State->IsStateValid()) { return Result; }
	for (const auto& Record : UnheardRecordContexts)
	{
		const auto* Cue = Record.Cue.Get();
		if (IsValid(Cue) && Cue->bRecordUnheardSummary && !Cue->RecordSummary.IsEmpty() && Record.Witness == State->GetActiveProtagonist()
			&& State->HasKnowledge(Record.Witness,Cue->RequiredKnowledge)) { Result.AddUnique(Record.Cue); }
	}
	return Result;
}
bool USovNarrativeCueComponent::ReplayUnheardRecord(USovNarrativeCue* Cue, FString& Error)
{
	if (!ResolveOwner() || !GetUnheardRecords().Contains(Cue) || !MatchesContext(Cue) || IsCombatRequired())
	{ Error = TEXT("This record cannot be replayed in the current protagonist and mission context."); return false; }
	const auto* Record = UnheardRecordContexts.FindByPredicate([&](const auto& Item) { return Item.Cue == Cue && Item.Witness == Controller->GetCampaignState()->GetActiveProtagonist(); });
	if (!Record) { return false; }
	FSovQueuedCue Copy = *Record;
	return RequestCue(Cue, ResolveSpeaker(Copy), Error);
}
void USovNarrativeCueComponent::RetireHeardRecord(USovNarrativeCue* Cue, FGameplayTag Witness)
{
	UnheardRecordContexts.RemoveAll([&](const auto& Record) { return Record.Cue == Cue && Record.Witness == Witness; });
	if (!UnheardRecordContexts.ContainsByPredicate([Cue](const auto& Record) { return Record.Cue == Cue; })) { UnheardRecords.Remove(Cue); }
}
void USovNarrativeCueComponent::SetBarkSubtitleHold(USovNarrativeCue* Cue, UObject* Owner, bool bHold)
{
	if (!bHold && SubtitleHoldOwner.Get() == Owner) { SubtitleHoldOwner.Reset(); bSubtitleHold = false; }
	else if (bHold && Cue && Cue == CurrentBark && IsValid(Owner)) { SubtitleHoldOwner = Owner; bSubtitleHold = true; }
}
void USovNarrativeCueComponent::StopBark(bool bInterrupted, bool bPreserveCritical)
{
	USovNarrativeCue* Finished = CurrentBark; CurrentBark = nullptr; const uint64 ExpectedEpoch=++Epoch;
	TSharedPtr<FStreamableHandle> RetiredLoad = MoveTemp(BarkLoad);
	BarkLoadEpoch = 0; BarkVariantIndex = INDEX_NONE; bBarkAudioAttempted = false;
	SubtitleHoldOwner.Reset(); bSubtitleHold = false;
	const FSovQueuedCue InterruptedRequest = CurrentRequest;
	UAudioComponent* Audio = BarkAudio; BarkAudio = nullptr;
	bBarkUsesControllerOutput = false;
	if (RetiredLoad) { RetiredLoad->CancelHandle(); }
	if (IsValid(Audio)) { Audio->Stop(); Audio->DestroyComponent(); }
	if (Epoch!=ExpectedEpoch || bOwnerEndingPlay) { return; }
	if (Finished)
	{
		if (bInterrupted && bPreserveCritical)
		{
			RememberUnheard(Finished, &InterruptedRequest);
			if (Finished->bCritical && Pending.Num() < 64
				&& !Pending.ContainsByPredicate([Finished](const auto& Item) { return Item.Cue == Finished; }))
			{ Pending.Add(InterruptedRequest); }
		}
		else if (!bInterrupted) { RetireHeardRecord(Finished, InterruptedRequest.Witness); }
		OnCueEnded.Broadcast(Finished, bInterrupted);
	}
}
bool USovNarrativeCueComponent::ConfigureControllerOutput(UAudioComponent* Audio, USoundClass* Class, float Volume)
{
	if (!IsValid(Audio) || !IsValid(Class) || !FMath::IsFinite(Volume) || Volume < 0.f || Volume > 1.f
		|| Class->Properties.OutputTarget != EAudioOutputTarget::ControllerFallbackToSpeaker) { return false; }
	// Never mutate a shared sound/class asset or guess a proprietary device ID. The engine owns fallback routing.
	Audio->SoundClassOverride = Class;
	Audio->SetVolumeMultiplier(Volume);
	return true;
}
bool USovNarrativeCueComponent::StartRequest(FSovQueuedCue Request)
{
	USovNarrativeCue* Cue = Request.Cue;
	const uint64 ExpectedEpoch = ++Epoch;
	if (!MatchesRequestContext(Request) || !IsValid(Tales) || (Cue->Dialogue && Tales->IsInDialogue())) { return false; }
	AActor* Speaker = ResolveSpeaker(Request);
	if (!Cue->Dialogue && !Speaker) { return false; }
	CurrentRequest = Request;
	if (Cue->Dialogue)
	{
		TWeakObjectPtr<UTalesComponent> StartingTales=Tales;
		CurrentConversation = Cue; ConversationRequest = Request;
		FDialoguePlayParams Params; Params.Priority = static_cast<int32>(Cue->Priority);
		TGuardValue<bool> Starting(bStartingConversation, true); bCompletedDuringStart = false;
		const bool bStarted=StartingTales->BeginDialogue(Cue->Dialogue, Params);
		if (bOwnerEndingPlay || !StartingTales.IsValid() || Tales!=StartingTales.Get()) { return false; }
		if (!bStarted || (Epoch != ExpectedEpoch && !(bCompletedDuringStart && Epoch==ExpectedEpoch+1))
			|| (!bCompletedDuringStart && !StartingTales->GetCurrentDialogue()))
		{ if (Epoch==ExpectedEpoch) { CurrentConversation = nullptr; } return false; }
		if (!bCompletedDuringStart && (!OwnedDialogue || !OwnedDialogue->CanSuspendPlayback()))
		{ Tales->ExitDialogue(EExitDialogueReason::EDR_PlayerExited); return false; }
	}
	else
	{
        TRACE_CPUPROFILER_EVENT_SCOPE(SovNarrativeCue_StartBark);
        const int32 Count = FMath::Max(0, RepetitionCounts.FindRef(Cue->CueId));
        BarkVariantIndex = Count % Cue->BarkVariants.Num();
        const auto& Variant = Cue->BarkVariants[BarkVariantIndex];
        CurrentBark = Cue; BarkLoadEpoch = ExpectedEpoch; bBarkAudioAttempted = false;
        BarkEndsAt = GetWorld()->GetTimeSeconds() + FMath::Max(2.f, Variant.CaptionSeconds);
        // Caption is available synchronously. Native voice/class residency never flushes async loading.
        OnCueStarted.Broadcast(Cue, Speaker, Variant.Caption, FMath::Max(2.f, Variant.CaptionSeconds));
        if (ExpectedEpoch != Epoch || bOwnerEndingPlay) { return false; }
        TArray<FSoftObjectPath> Paths;
        if (!Variant.Sound.IsNull()) { Paths.Add(Variant.Sound.ToSoftObjectPath()); }
        if (!Cue->ControllerAudioClass.IsNull()) { Paths.AddUnique(Cue->ControllerAudioClass.ToSoftObjectPath()); }
        if (!Paths.IsEmpty())
        {
            const auto Requested = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
            if (ExpectedEpoch != Epoch || bOwnerEndingPlay) { if (Requested) { Requested->CancelHandle(); } return false; }
            BarkLoad = Requested;
        }
        if (!BarkLoad || BarkLoad->HasLoadCompleted()) { StartResidentBarkAudio(); }
        if (ExpectedEpoch != Epoch || bOwnerEndingPlay) { return false; }
	}
	int32& Count = RepetitionCounts.FindOrAdd(Cue->CueId); Count = FMath::Clamp(Count, 0, 999999) + 1;
	NextAllowed.Add(Cue->CueId, GetWorld()->GetTimeSeconds() + SovNarrativeCuePolicy::Cooldown(Cue->CooldownSeconds, static_cast<unsigned>(Count - 1)));
	USovDiagnosticsSubsystem::Record(GetWorld(), ESovDiagnosticKind::Cinematic, Cue->CueId, Cue->SpeakerId, static_cast<float>(Cue->Priority), 0.f, true);
	return true;
}
void USovNarrativeCueComponent::StartResidentBarkAudio()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SovNarrativeCue_ResidentVoice);
    if (bBarkAudioAttempted || !CurrentBark || BarkLoadEpoch != Epoch || bOwnerEndingPlay
        || !CurrentBark->BarkVariants.IsValidIndex(BarkVariantIndex) || !MatchesRequestContext(CurrentRequest)) { return; }
    bBarkAudioAttempted = true;
    const uint64 Expected = Epoch;
    USovNarrativeCue* Cue = CurrentBark;
    AActor* Speaker = ResolveSpeaker(CurrentRequest);
    USoundBase* Sound = Cue->BarkVariants[BarkVariantIndex].Sound.Get();
    USoundClass* ControllerClass = Cue->ControllerAudioClass.Get();
    if (!Sound || !Speaker) { return; } // Authored caption remains the fallback on an unavailable voice asset/device.
    BarkAudio = NewObject<UAudioComponent>(GetOwner());
    BarkAudio->bAutoActivate = false; BarkAudio->bAutoDestroy = false;
    BarkAudio->bStopWhenOwnerDestroyed = true; BarkAudio->bIsUISound = false;
    bBarkUsesControllerOutput = ControllerClass && ConfigureControllerOutput(BarkAudio, ControllerClass,
        USovGameUserSettings::Get() ? USovGameUserSettings::Get()->GetSettingsSnapshot().ControllerAudioVolume : 1.f);
    BarkAudio->SetSound(Sound); BarkAudio->SetWorldLocation(Speaker->GetActorLocation()); BarkAudio->RegisterComponent();
    if (Expected != Epoch || bOwnerEndingPlay || !IsValid(BarkAudio)) { return; }
    BarkAudio->Play();
    if (Expected != Epoch || bOwnerEndingPlay) { return; }
    const float VoiceDuration = Sound->GetDuration();
    const float Duration = FMath::IsFinite(VoiceDuration) && VoiceDuration > 0.f && VoiceDuration <= 30.f
        ? FMath::Max(Cue->BarkVariants[BarkVariantIndex].CaptionSeconds, VoiceDuration) : Cue->BarkVariants[BarkVariantIndex].CaptionSeconds;
    BarkEndsAt = GetWorld()->GetTimeSeconds() + FMath::Max(2.f,Duration);
    OnCueAudioReady.Broadcast(Cue,FMath::Max(2.f,Duration));
}
void USovNarrativeCueComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(Delta, Type, TickFunction);
	if (bMutation || !ResolveOwner() || !GetWorld() || GetWorld()->IsPaused() || !FMath::IsFinite(Delta) || Delta <= 0.f) { return; }
	TGuardValue<bool> Mutation(bMutation, true);
	if (bBarkUsesControllerOutput && IsValid(BarkAudio))
	{
		if (const auto* Settings = USovGameUserSettings::Get())
		{ BarkAudio->SetVolumeMultiplier(Settings->GetSettingsSnapshot().ControllerAudioVolume); }
	}
	if (CurrentBark && !MatchesRequestContext(CurrentRequest)) { StopBark(true); }
	if (!ResolveOwner()) { return; }
	if (CurrentConversation && OwnedDialogue && !MatchesRequestContext(ConversationRequest))
	{ OwnedDialogue->SetPreserveOnInterruption(false); Tales->ExitDialogue(EExitDialogueReason::EDR_PlayerExited); }
	if (!ResolveOwner()) { return; }
	if (CurrentBark && BarkLoad && BarkLoad->HasLoadCompleted()) { StartResidentBarkAudio(); }
	if (!ResolveOwner()) { return; }
	const bool Combat = IsCombatRequired();
	if (CurrentBark && !ResolveSpeaker(CurrentRequest)) { StopBark(true); }
	if (!ResolveOwner()) { return; }
	if (CurrentBark && GetWorld()->GetTimeSeconds() >= BarkEndsAt && !(bSubtitleHold && SubtitleHoldOwner.IsValid())) { StopBark(false); }
	if (!ResolveOwner()) { return; }
	if (OwnedDialogue && Tales->GetCurrentDialogue() == OwnedDialogue && CurrentConversation)
	{
		if (Combat && CurrentConversation->Priority == ESovNarrativeCuePriority::Ambient && !CurrentConversation->bCritical)
		{ Tales->ExitDialogue(EExitDialogueReason::EDR_PlayerExited); }
		else if (Combat || CurrentBark)
		{ if (OwnedDialogue->SetPlaybackSuspended(true)) { RememberUnheard(CurrentConversation); } }
		else if (OwnedDialogue->IsPlaybackSuspended()) { OwnedDialogue->SetPlaybackSuspended(false); }
	}
	if (!ResolveOwner()) { return; }
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		auto& Request = Pending[Index];
		if (!IsValid(Request.Cue) || !MatchesRequestContext(Request)) { Pending.RemoveAt(Index); continue; }
		if (!Request.Cue->bCritical)
		{
			Request.RemainingContextSeconds -= Delta;
			if (Request.RemainingContextSeconds <= 0.f) { Pending.RemoveAt(Index); }
		}
	}
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const auto* Cue = Pending[Index].Cue;
		if ((Combat && !SovNarrativeCuePolicy::MayPlayInCombat(static_cast<unsigned>(Cue->Priority), Cue->Dialogue != nullptr))
			|| NextAllowed.FindRef(Cue->CueId) > GetWorld()->GetTimeSeconds() || (Cue->Dialogue && Tales->IsInDialogue())) { continue; }
		if (CurrentBark && !SovNarrativeCuePolicy::MayInterrupt(static_cast<unsigned>(Cue->Priority), static_cast<unsigned>(CurrentBark->Priority))) { continue; }
		if (Tales->IsInDialogue() && (!OwnedDialogue || Tales->GetCurrentDialogue() != OwnedDialogue)) { continue; }
		if (CurrentConversation && OwnedDialogue && !OwnedDialogue->IsPlaybackSuspended()
			&& !SovNarrativeCuePolicy::MayInterrupt(static_cast<unsigned>(Cue->Priority), static_cast<unsigned>(CurrentConversation->Priority))) { continue; }
		if (Best == INDEX_NONE || Cue->Priority < Pending[Best].Cue->Priority) { Best = Index; }
	}
	if (Best == INDEX_NONE) { return; }
	FSovQueuedCue Request = Pending[Best]; Pending.RemoveAt(Best);
	const uint64 BeforeSuspend=Epoch;
	if (OwnedDialogue && !OwnedDialogue->IsPlaybackSuspended() && !OwnedDialogue->SetPlaybackSuspended(true))
	{ if (Epoch==BeforeSuspend && ResolveOwner() && Pending.Num()<64) { Pending.Add(Request); } return; }
	if (Epoch!=BeforeSuspend || !ResolveOwner()) { return; }
	if (CurrentBark)
	{
		const uint64 BeforeStop=Epoch; StopBark(true);
		if (Epoch!=BeforeStop+1) { return; }
	}
	if (!ResolveOwner()) { return; }
	const uint64 ExpectedStartEpoch=Epoch+1;
	if (!StartRequest(Request) && Epoch==ExpectedStartEpoch && ResolveOwner() && Request.Cue && Request.Cue->bCritical)
	{
		RememberUnheard(Request.Cue, &Request);
		if (Pending.Num()<64 && !Pending.ContainsByPredicate([&Request](const auto& Item) { return Item.Cue==Request.Cue; })) { Pending.Add(Request); }
	}
}
void USovNarrativeCueComponent::HandleDialogueBegan(UDialogue* Dialogue)
{
	if (bStartingConversation && CurrentConversation && Dialogue && Dialogue->GetClass() == CurrentConversation->Dialogue
		&& Tales && Tales->GetCurrentDialogue() == Dialogue)
	{ OwnedDialogue = Dialogue; OwnedDialogue->SetPreserveOnInterruption(CurrentConversation->bCritical); }
	else if (CurrentBark && IsValid(Dialogue))
	{
		// External Narrative conversations also own the speech channel. This notification runs
		// before their first line starts, so stop our bark before it can overlap that voice.
		StopBark(true);
	}
}
void USovNarrativeCueComponent::HandleDialogueFinished(UDialogue* Dialogue, bool bStartingNew, EExitDialogueReason Reason)
{
	if (Dialogue != OwnedDialogue) { return; }
	++Epoch; USovNarrativeCue* Finished = CurrentConversation; const FSovQueuedCue FinishedRequest=ConversationRequest;
	CurrentConversation = nullptr; OwnedDialogue = nullptr;
	const bool Interrupted = bStartingNew || Reason != EExitDialogueReason::EDR_NoLines;
	if (bStartingConversation && !Interrupted) { bCompletedDuringStart = true; }
	if (Interrupted) { RememberUnheard(Finished, &FinishedRequest); }
	else { RetireHeardRecord(Finished, FinishedRequest.Witness); }
	if (Finished) { OnCueEnded.Broadcast(Finished, Interrupted); }
}
void USovNarrativeCueComponent::PrepareForSave_Implementation()
{
	InFlightCriticalSave = FSovQueuedCue();
	if (CurrentBark && CurrentBark->bCritical) { InFlightCriticalSave = CurrentRequest; }
}
void USovNarrativeCueComponent::Load_Implementation()
{
	TGuardValue<bool> Mutation(bMutation,true);
	++Epoch; StopBark(true, false);
	UDialogue* OldDialogue=OwnedDialogue;
	OwnedDialogue = nullptr; CurrentConversation = nullptr; NextAllowed.Reset();
	if (IsValid(OldDialogue) && IsValid(Tales) && Tales->GetCurrentDialogue() == OldDialogue)
	{ OldDialogue->SetPreserveOnInterruption(false); Tales->ExitDialogue(EExitDialogueReason::EDR_PlayerExited); }
	if (bOwnerEndingPlay) { return; }
	if (Pending.Num() > 64 || UnheardRecords.Num() > 256 || UnheardRecordContexts.Num() > 256 || RepetitionCounts.Num() > 4096)
	{ Pending.Reset(); UnheardRecords.Reset(); UnheardRecordContexts.Reset(); RepetitionCounts.Reset(); return; }
	TSet<USovNarrativeCue*> Seen;
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		FString Error; const auto& Item = Pending[Index];
		if (!IsValid(Item.Cue) || !Item.Cue->Validate(Error) || !FMath::IsFinite(Item.RemainingContextSeconds) || Item.RemainingContextSeconds < 0.f
			|| Item.RemainingContextSeconds>120.f || Seen.Contains(Item.Cue))
		{ Pending.RemoveAt(Index); }
		else { Seen.Add(Item.Cue); }
	}
	FString InFlightError;
	if (InFlightCriticalSave.Cue && InFlightCriticalSave.Cue->bCritical && Pending.Num() < 64
		&& InFlightCriticalSave.Cue->Validate(InFlightError)
		&& FMath::IsFinite(InFlightCriticalSave.RemainingContextSeconds) && InFlightCriticalSave.RemainingContextSeconds>=0.f && InFlightCriticalSave.RemainingContextSeconds<=120.f
		&& !Pending.ContainsByPredicate([this](const auto& Item) { return Item.Cue==InFlightCriticalSave.Cue; })) { Pending.Add(InFlightCriticalSave); }
	UnheardRecords.RemoveAll([](const auto& Cue)
	{
		FString Error;
		return !IsValid(Cue) || !Cue->bCritical || !Cue->bRecordUnheardSummary || Cue->RecordSummary.IsEmpty() || !Cue->Validate(Error);
	});
    UnheardRecordContexts.RemoveAll([](const auto& Record)
    {
        FString Error;
        return !IsValid(Record.Cue) || !Record.Witness.IsValid() || !Record.Cue->bCritical
            || !Record.Cue->bRecordUnheardSummary || !Record.Cue->Validate(Error);
    });
	for (auto& Pair : RepetitionCounts) { Pair.Value = FMath::Clamp(Pair.Value, 0, 1000000); }
	InFlightCriticalSave = FSovQueuedCue();
}
void USovNarrativeCueComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	++Epoch; bOwnerEndingPlay=true; bMutation = true; StopBark(true, false);
	if (Tales)
	{ Tales->OnDialogueFinished.RemoveDynamic(this, &ThisClass::HandleDialogueFinished); Tales->OnDialogueBegan.RemoveDynamic(this, &ThisClass::HandleDialogueBegan); }
	Tales = nullptr; Controller = nullptr; OwnedDialogue = nullptr; CurrentConversation = nullptr;
	Super::EndPlay(Reason);
}
