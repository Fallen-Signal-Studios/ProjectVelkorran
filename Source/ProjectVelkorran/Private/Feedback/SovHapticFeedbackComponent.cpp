// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Feedback/SovHapticFeedbackComponent.h"
#include "Framework/SovPlayerController.h"
#include "Settings/SovGameUserSettings.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

USovHapticFeedbackComponent::USovHapticFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
	// A nonzero tick interval can stall its interval countdown while paused, despite bTickEvenWhenPaused.
	PrimaryComponentTick.TickInterval = 0.f;
	bAutoActivate = true;
}
void USovHapticFeedbackComponent::BeginPlay() { Super::BeginPlay(); RefreshSources(); }
double USovHapticFeedbackComponent::FeedbackTime() const { return FPlatformTime::Seconds(); }
FSovHapticSettings USovHapticFeedbackComponent::ReadFeedbackSettings() const
{
	if (const auto* Settings = USovGameUserSettings::Get()) { return Settings->GetHapticSettings(); }
	FSovHapticSettings Silent; Silent.Master = 0.f; return Silent;
}
bool USovHapticFeedbackComponent::HasCurrentPawn() const
{
	return BoundController.IsValid() && BoundPawn.IsValid() && BoundASC.IsValid()
		&& BoundController->GetPawn() == BoundPawn.Get() && BoundPawn->GetController() == BoundController.Get()
		&& BoundASC->GetAvatarActor() == BoundPawn.Get() && BoundASC->GetCharacterReadyEpoch() == BoundReadyEpoch;
}
bool USovHapticFeedbackComponent::CanOutput(ESovHapticChannel Channel) const
{
	const auto* PC = Cast<ANarrativePlayerController>(GetOwner());
	if (bEnding || !IsActive() || !PC || !PC->IsLocalController() || !PC->bForceFeedbackEnabled
		|| !GetWorld() || GetWorld()->IsPaused() || PC->IsActorBeingDestroyed()) { return false; }
	if (const auto* Campaign = Cast<ASovPlayerController>(PC))
	{ if (Campaign->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle) { return false; } }
	if (Channel == ESovHapticChannel::UI) { return true; }
	if (!HasCurrentPawn() || BoundASC->IsDead()) { return false; }
	if (Channel == ESovHapticChannel::Cinematic) { return true; }
	return !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored() && PC->CurrentSequences.IsEmpty();
}
void USovHapticFeedbackComponent::RefreshSources()
{
	if (bEnding) { return; }
	auto* PC = Cast<ANarrativePlayerController>(GetOwner());
	auto* ASC = PC ? Cast<UNarrativeAbilitySystemComponent>(PC->GetAbilitySystemComponent()) : nullptr;
	auto* Settings = USovGameUserSettings::Get();
	if (PC == BoundController.Get() && (PC ? PC->GetPawn() : nullptr) == BoundPawn.Get()
		&& ASC == BoundASC.Get() && (!ASC || ASC->GetCharacterReadyEpoch() == BoundReadyEpoch)
		&& Settings == BoundSettings.Get()) { return; }
	UnbindSources();
	if (!IsValid(PC) || !PC->IsLocalController()) { return; }
	BoundController = PC; BoundPawn = PC->GetPawn(); BoundSettings = Settings;
	if (IsValid(ASC) && ASC->GetAvatarActor() == BoundPawn.Get())
	{
		BoundASC = ASC; BoundReadyEpoch = ASC->GetCharacterReadyEpoch();
		ASC->OnDamageResolvedAsTarget.AddUniqueDynamic(this, &ThisClass::HandleDamage);
		ASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
		ASC->OnCharacterReadyEpochChanged.AddUniqueDynamic(this, &ThisClass::HandleReadiness);
	}
	BoundInteraction = PC->GetInteractionComponent();
	if (BoundInteraction.IsValid()) { BoundInteraction->GetOnBeginUseInteractable().AddUniqueDynamic(this, &ThisClass::HandleInteraction); }
	PC->OnLevelSequencePlay.AddUniqueDynamic(this, &ThisClass::HandleSequencePlay);
	PC->OnLevelSequenceStop.AddUniqueDynamic(this, &ThisClass::HandleSequenceStop);
	if (IsValid(Settings)) { Settings->OnHapticSettingsChanged.AddUniqueDynamic(this, &ThisClass::HandleSettings); }
}
void USovHapticFeedbackComponent::UnbindSources()
{
	CancelAllFeedback();
	if (BoundASC.IsValid())
	{
		BoundASC->OnDamageResolvedAsTarget.RemoveDynamic(this, &ThisClass::HandleDamage);
		BoundASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath);
		BoundASC->OnCharacterReadyEpochChanged.RemoveDynamic(this, &ThisClass::HandleReadiness);
	}
	if (BoundInteraction.IsValid()) { BoundInteraction->GetOnBeginUseInteractable().RemoveDynamic(this, &ThisClass::HandleInteraction); }
	if (BoundController.IsValid())
	{
		BoundController->OnLevelSequencePlay.RemoveDynamic(this, &ThisClass::HandleSequencePlay);
		BoundController->OnLevelSequenceStop.RemoveDynamic(this, &ThisClass::HandleSequenceStop);
	}
	if (BoundSettings.IsValid()) { BoundSettings->OnHapticSettingsChanged.RemoveDynamic(this, &ThisClass::HandleSettings); }
	BoundController.Reset(); BoundPawn.Reset(); BoundASC.Reset(); BoundInteraction.Reset(); BoundSettings.Reset(); BoundReadyEpoch = 0;
}
int64 USovHapticFeedbackComponent::PlayFeedback(ESovHapticChannel Channel, float Intensity, float Duration, int32 Priority)
{
	const FSovHapticSettings Settings = ReadFeedbackSettings();
	if (!CanOutput(Channel) || !Settings.IsValid() || Settings.Master <= 0.f || Settings.Scale(Channel) <= 0.f) { return 0; }
	if (Channel == ESovHapticChannel::Cinematic && (!FeedbackSequence.IsValid() || !BoundController.IsValid()
		|| !BoundController->CurrentSequences.Contains(FeedbackSequence))) { return 0; }
	const auto Id = Mixer.Play(static_cast<unsigned>(Channel), Intensity, Duration, Priority, FeedbackTime());
	if (Id && Channel == ESovHapticChannel::Cinematic) { SequenceReceipts.Add(static_cast<int64>(Id)); }
	FlushOutput(); return static_cast<int64>(Id);
}
bool USovHapticFeedbackComponent::CancelFeedback(int64 Receipt)
{
	if (Receipt <= 0 || !Mixer.Cancel(static_cast<uint64>(Receipt))) { return false; }
	SequenceReceipts.Remove(Receipt);
	FlushOutput(); return true;
}
void USovHapticFeedbackComponent::CancelAllFeedback()
{
	Mixer.Clear(); FeedbackSequence.Reset(); SequenceReceipts.Reset();
	for (unsigned Index = 0; Index < SovHapticPolicy::ChannelCount; ++Index) { SubmitChannel(static_cast<ESovHapticChannel>(Index), 0.f); }
}
void USovHapticFeedbackComponent::FlushOutput()
{
	const double Now = FeedbackTime();
	const FSovHapticSettings Settings = ReadFeedbackSettings();
	Mixer.Expire(Now);
	for (auto It = SequenceReceipts.CreateIterator(); It; ++It) { if (!Mixer.Contains(static_cast<uint64>(*It))) { It.RemoveCurrent(); } }
	for (unsigned Index = 0; Index < SovHapticPolicy::ChannelCount; ++Index)
	{
		const auto Channel = static_cast<ESovHapticChannel>(Index);
		if (!CanOutput(Channel) || !Settings.IsValid() || Settings.Master <= 0.f || Settings.Scale(Channel) <= 0.f)
		{ Mixer.CancelChannel(Index); SubmitChannel(Channel, 0.f); continue; }
		SubmitChannel(Channel, Mixer.Output(Index, Settings.Master, Settings.Scale(Channel), Now));
	}
}
void USovHapticFeedbackComponent::SubmitChannel(ESovHapticChannel Channel, float Intensity)
{
	const unsigned Index = static_cast<unsigned>(Channel);
	if (Index >= SovHapticPolicy::ChannelCount) { return; }
	auto* PC = Cast<APlayerController>(GetOwner());
	if (!IsValid(PC)) { OutputHandles[Index] = 0; return; }
	const auto Handle = OutputHandles[Index];
	if (Intensity <= 0.f || !FMath::IsFinite(Intensity))
	{
		OutputHandles[Index] = 0; // Retire before an output adapter can reenter.
		if (Handle) { PC->PlayDynamicForceFeedback(0.f, 0.f, true, true, true, true, EDynamicForceFeedbackAction::Stop, Handle); }
		return;
	}
	// Short leases guarantee hardware stops even if component tick is removed unexpectedly.
	// Renew before lease expiry; do not depend on Update resetting Unreal's action elapsed-time counter.
	const double Now = FeedbackTime();
	if (Handle && Now - LastOutputTimes[Index] < .04 && LastOutputIntensities[Index] == Intensity) { return; }
	if (Handle) { PC->PlayDynamicForceFeedback(0.f, 0.f, true, true, true, true, EDynamicForceFeedbackAction::Stop, Handle); }
	OutputHandles[Index] = PC->PlayDynamicForceFeedback(Intensity, .1f, true, true, true, true, EDynamicForceFeedbackAction::Start, 0);
	LastOutputTimes[Index] = Now;
	LastOutputIntensities[Index] = Intensity;
}
void USovHapticFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{ Super::TickComponent(DeltaTime, TickType, TickFunction); RefreshSources(); FlushOutput(); }
void USovHapticFeedbackComponent::Deactivate() { CancelAllFeedback(); Super::Deactivate(); }
void USovHapticFeedbackComponent::EndPlay(const EEndPlayReason::Type Reason) { bEnding = true; UnbindSources(); Super::EndPlay(Reason); }
void USovHapticFeedbackComponent::HandleSettings(const FSovHapticSettings& Settings) { FlushOutput(); }
void USovHapticFeedbackComponent::HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bDead)
{ if (bDead && ASC == BoundASC.Get() && Actor == BoundPawn.Get()) { CancelAllFeedback(); } }
void USovHapticFeedbackComponent::HandleReadiness(int32 ReadyEpoch) { CancelAllFeedback(); RefreshSources(); }
void USovHapticFeedbackComponent::HandleDamage(const FSovDamageResult& Result)
{
	if (!HasCurrentPawn() || Result.TargetActor != BoundPawn.Get()) { return; }
	if (Result.bPerfectDefense) { PlayFeedback(ESovHapticChannel::Combat, .75f, .09f, 80); }
	else if (Result.bGuardBroken || Result.bShieldBroken) { PlayFeedback(ESovHapticChannel::Combat, 1.f, .18f, 90); }
	else if (Result.AppliedHealthDamage > 0.f || Result.AppliedShieldDamage > 0.f)
	{ PlayFeedback(ESovHapticChannel::Combat, .45f, .1f, 40); }
}
void USovHapticFeedbackComponent::HandleInteraction(AActor* Actor, UNarrativeInteractableComponent* Interactable)
{
	if (HasCurrentPawn() && IsValid(Actor) && IsValid(Interactable) && Interactable->GetOwner() == Actor)
	{ PlayFeedback(ESovHapticChannel::Interaction, .5f, .06f, 20); }
}
void USovHapticFeedbackComponent::HandleSequencePlay(ANarrativeLevelSequenceActor* Sequence, const FNarrativeSequencePlaybackSettings& Settings)
{
	if (!IsValid(Sequence) || !BoundController.IsValid() || !BoundController->CurrentSequences.Contains(Sequence)) { return; }
	CancelAllFeedback();
	FeedbackSequence = Sequence;
	PlayFeedback(ESovHapticChannel::Cinematic, .2f, .08f, 10);
}
void USovHapticFeedbackComponent::HandleSequenceStop(ANarrativeLevelSequenceActor* Sequence, const FNarrativeSequencePlaybackSettings& Settings)
{
	if (FeedbackSequence.Get() != Sequence) { return; }
	const TSet<int64> Receipts = MoveTemp(SequenceReceipts); FeedbackSequence.Reset();
	for (int64 Receipt : Receipts) { Mixer.Cancel(static_cast<uint64>(Receipt)); }
	FlushOutput();
}
