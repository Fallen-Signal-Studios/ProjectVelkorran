// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignStateComponent.h"

#include "Campaign/SovCampaignPolicy.h"
#include "Companions/SovCoActionAnchor.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Sovereign/SovGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Tales/TalesComponent.h"

USovCampaignStateComponent::USovCampaignStateComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USovCampaignStateComponent::HasAuthorityOwner() const
{
	return IsValid(GetOwner()) && GetOwner()->HasAuthority() && bStateValid;
}

bool USovCampaignStateComponent::DoesCurrentPawnMatch(FGameplayTag Protagonist) const
{
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const ASovPlayerCharacterBase* Pawn = PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
	return IsValid(Pawn) && Pawn->GetProtagonistIdentityTag() == Protagonist;
}

bool USovCampaignStateComponent::IsBeatComplete(FName MissionId, FName BeatId) const
{
	const FSovCampaignMissionRecord* Record = Missions.Find(MissionId);
	return bStateValid && Record && Record->CompletedBeats.Contains(BeatId);
}

bool USovCampaignStateComponent::IsMissionComplete(FName MissionId) const
{
	const FSovCampaignMissionRecord* Record = Missions.Find(MissionId);
	return bStateValid && Record && Record->bSucceeded;
}

bool USovCampaignStateComponent::CanEnterMission(const USovCampaignDefinition* Definition) const
{
	FString Error;
	if (!bStateValid || !IsValid(Definition) || !Definition->ValidateDefinition(Error)) { return false; }
	if (!ActiveMission) { return Missions.IsEmpty() && MissionDefinitions.IsEmpty(); }
	if (const TObjectPtr<USovCampaignDefinition>* Known = MissionDefinitions.Find(Definition->MissionId);
		Known && Known->Get() != Definition) { return false; }
	return ActiveMission == Definition || (IsMissionComplete(ActiveMission->MissionId)
		&& ActiveMission->AllowedSuccessorMissions.Contains(Definition->MissionId));
}

ESovCampaignResult USovCampaignStateComponent::BeginMission(USovCampaignDefinition* Definition)
{
	if (!HasAuthorityOwner()) { return ESovCampaignResult::NotAuthority; }
	if (bMutating) { return ESovCampaignResult::Busy; }
	if (!CanEnterMission(Definition) || !DoesCurrentPawnMatch(Definition->Protagonist)) { return ESovCampaignResult::Invalid; }
	if (ActiveMission == Definition) { return ESovCampaignResult::AlreadyApplied; }
	TGuardValue<bool> Mutation(bMutating, true);
	ActiveMission = Definition;
	Missions.FindOrAdd(Definition->MissionId);
	MissionDefinitions.Add(Definition->MissionId, Definition);
	OnMissionChanged.Broadcast(Definition->MissionId, IsMissionComplete(Definition->MissionId));
	return ESovCampaignResult::Applied;
}

bool USovCampaignStateComponent::HasKnowledge(FGameplayTag Protagonist, const FGameplayTagContainer& Required) const
{
	if (!bStateValid) { return false; }
	if (Required.IsEmpty()) { return true; }
	const FSovCampaignKnowledgeRecord* Record = CharacterKnowledge.Find(Protagonist);
	return bStateValid && Record && Record->Knowledge.HasAllExact(Required);
}

FGameplayTag USovCampaignStateComponent::GetStateValue(FGameplayTag Key) const
{
	const FGameplayTag* Value = StateValues.Find(Key);
	return bStateValid && Value ? *Value : FGameplayTag();
}

bool USovCampaignStateComponent::StateWritesValid(const TArray<FSovCampaignStateWrite>& Writes) const
{
	for (const FSovCampaignStateWrite& Write : Writes)
	{
		if (!Write.Key.IsValid() || !Write.Value.IsValid()) { return false; }
		const FGameplayTag* Existing = StateValues.Find(Write.Key);
		if (ProtectedStateKeys.HasTagExact(Write.Key) && (!Existing || *Existing != Write.Value)) { return false; }
	}
	return true;
}

ESovCampaignResult USovCampaignStateComponent::CompleteBeat(FName BeatId, bool bSkipPresentation)
{
	return CompleteBeatInternal(BeatId, bSkipPresentation, nullptr);
}

ESovCampaignResult USovCampaignStateComponent::CompleteCoAction(ASovCoActionAnchor* Source)
{
	return IsValid(Source) ? CompleteBeatInternal(Source->CompletionBeat, false, Source) : ESovCampaignResult::Invalid;
}

ESovCampaignResult USovCampaignStateComponent::CompleteBeatInternal(FName BeatId, bool bSkipPresentation, ASovCoActionAnchor* CoActionSource)
{
	if (!HasAuthorityOwner()) { return ESovCampaignResult::NotAuthority; }
	if (bMutating) { return ESovCampaignResult::Busy; }
	FString DefinitionError;
	if (!IsValid(ActiveMission) || !ActiveMission->ValidateDefinition(DefinitionError)
		|| !DoesCurrentPawnMatch(ActiveMission->Protagonist)) { return ESovCampaignResult::Invalid; }
	const FSovCampaignBeatDefinition* Beat = ActiveMission->FindBeat(BeatId);
	if (!Beat) { return ESovCampaignResult::Invalid; }
	bool bPrerequisitesMet = true;
	for (FName Prior : Beat->PrerequisiteBeats) { bPrerequisitesMet &= IsBeatComplete(ActiveMission->MissionId, Prior); }
	for (const FSovCampaignStateWrite& Required : Beat->RequiredState)
	{ bPrerequisitesMet &= GetStateValue(Required.Key) == Required.Value; }
	const auto Policy = SovCampaignPolicy::CompleteBeat(true, true, IsBeatComplete(ActiveMission->MissionId, BeatId),
		bPrerequisitesMet, HasKnowledge(ActiveMission->Protagonist, Beat->RequiredKnowledge), StateWritesValid(Beat->StateWrites),
		bSkipPresentation, !Beat->CinematicId.IsNone(), ViewedCinematics.Contains(Beat->CinematicId), Beat->bInteractiveChoice);
	switch (Policy)
	{
	case SovCampaignPolicy::Result::Duplicate: return ESovCampaignResult::AlreadyApplied;
	case SovCampaignPolicy::Result::Prerequisite: return ESovCampaignResult::PrerequisiteMissing;
	case SovCampaignPolicy::Result::Knowledge: return ESovCampaignResult::KnowledgeMissing;
	case SovCampaignPolicy::Result::Protected: return ESovCampaignResult::ProtectedStateConflict;
	case SovCampaignPolicy::Result::NotViewed: return ESovCampaignResult::SkipUnavailable;
	case SovCampaignPolicy::Result::Allowed: break;
	default: return ESovCampaignResult::Invalid;
	}
	if (Beat->bRequiresCoActionProof)
	{
		if (!IsValid(CoActionSource) || CoActionSource->AnchorId != Beat->RequiredCoActionAnchorId
			|| CoActionSource->RequiredCompanionId != Beat->RequiredCompanionId
			|| !CoActionSource->ConsumeReceipt(this)) { return ESovCampaignResult::Invalid; }
	}
	else if (CoActionSource) { return ESovCampaignResult::Invalid; }
	FSovCampaignJournalEntry Entry;
	if (CoActionSource)
	{
		Entry.CoActionRequestId = CoActionSource->ReceiptRequestId;
		Entry.CoActionCompanionId = CoActionSource->RequiredCompanionId;
		Entry.CoActionAnchorId = CoActionSource->AnchorId;
	}
	Entry.EventId = FGuid::NewGuid();
	Entry.Sequence = Journal.Num() + 1;
	Entry.MissionId = ActiveMission->MissionId;
	Entry.BeatId = BeatId;
	Entry.Protagonist = ActiveMission->Protagonist;
	Entry.bPresentationSkipped = bSkipPresentation;
	UNarrativeDataTask* Task = Beat->CompletionTask;
	bool bMissionJustSucceeded = false;
	TGuardValue<bool> Mutation(bMutating, true);
	{
		for (const FSovCampaignStateWrite& Write : Beat->StateWrites)
		{
			StateValues.Add(Write.Key, Write.Value);
			if (Write.bCanonProtected) { ProtectedStateKeys.AddTag(Write.Key); }
		}
		CharacterKnowledge.FindOrAdd(Entry.Protagonist).Knowledge.AppendTags(Beat->GrantedKnowledge);
		FSovCampaignMissionRecord& Record = Missions.FindOrAdd(Entry.MissionId);
		Record.CompletedBeats.AddUnique(BeatId);
		bool bAllMandatoryComplete = true;
		for (const FSovCampaignBeatDefinition& Required : ActiveMission->Beats)
		{ if (!Required.bOptional && !Record.CompletedBeats.Contains(Required.BeatId)) { bAllMandatoryComplete = false; } }
		bMissionJustSucceeded = bAllMandatoryComplete && !Record.bSucceeded;
		Record.bSucceeded = bAllMandatoryComplete;
		Journal.Add(Entry);
	}
	// Keep the transaction guard through authored notifications: callback-driven commands return Busy.
	// They may schedule a later command after the complete commit/notification sequence.
	if (Task)
	{
		if (UTalesComponent* Tales = GetOwner()->FindComponentByClass<UTalesComponent>())
		{ Tales->CompleteNarrativeDataTask(Task, Entry.BeatId.ToString(), 1); }
	}
	OnBeatCommitted.Broadcast(Entry);
	if (bMissionJustSucceeded) { OnMissionChanged.Broadcast(Entry.MissionId, true); }
	return ESovCampaignResult::Applied;
}

bool USovCampaignStateComponent::RecordCinematicViewed(FName BeatId)
{
	if (!HasAuthorityOwner() || bMutating || !ActiveMission || !DoesCurrentPawnMatch(ActiveMission->Protagonist)) { return false; }
	const FSovCampaignBeatDefinition* Beat = ActiveMission->FindBeat(BeatId);
	if (!Beat || Beat->CinematicId.IsNone()) { return false; }
	for (FName Prior : Beat->PrerequisiteBeats) { if (!IsBeatComplete(ActiveMission->MissionId, Prior)) { return false; } }
	for (const FSovCampaignStateWrite& Required : Beat->RequiredState)
	{ if (GetStateValue(Required.Key) != Required.Value) { return false; } }
	if (!HasKnowledge(ActiveMission->Protagonist, Beat->RequiredKnowledge)) { return false; }
	ViewedCinematics.AddUnique(Beat->CinematicId);
	return true;
}

bool USovCampaignStateComponent::KnowsEvidence(FName EvidenceId, FGameplayTag Protagonist) const
{
	return bStateValid && Evidence.ContainsByPredicate([=](const FSovEvidenceAcquisition& Item)
	{ return Item.EvidenceId == EvidenceId && Item.Protagonist == Protagonist; });
}

ESovCampaignResult USovCampaignStateComponent::AcquireEvidence(USovEvidenceSourceComponent* Source)
{
	if (!HasAuthorityOwner()) { return ESovCampaignResult::NotAuthority; }
	if (bMutating) { return ESovCampaignResult::Busy; }
	if (!IsValid(Source) || !IsValid(Source->GetOwner()) || !IsValid(ActiveMission)
		|| !DoesCurrentPawnMatch(ActiveMission->Protagonist) || Source->GetWorld() != GetWorld()
		|| Source->EvidenceId.IsNone() || !Source->SourceId.IsValid()
		|| Source->AcquisitionMission != ActiveMission->MissionId
		|| !Source->AllowedProtagonists.HasTagExact(ActiveMission->Protagonist)
		|| !FMath::IsFinite(Source->InteractionRange) || Source->InteractionRange <= 0.f)
	{ return ESovCampaignResult::Invalid; }
	for (const FSovEvidenceAcquisition& Existing : Evidence)
	{
		if (Existing.SourceId == Source->SourceId && (Existing.EvidenceId != Source->EvidenceId
			|| Existing.MissionId != Source->AcquisitionMission)) { return ESovCampaignResult::Invalid; }
	}
	if (KnowsEvidence(Source->EvidenceId, ActiveMission->Protagonist)) { return ESovCampaignResult::AlreadyApplied; }
	if (!Source->RequiredCompletedBeat.IsNone() && !IsBeatComplete(ActiveMission->MissionId, Source->RequiredCompletedBeat))
	{ return ESovCampaignResult::PrerequisiteMissing; }
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!IsValid(Pawn) || FVector::DistSquared(Pawn->GetActorLocation(), Source->GetOwner()->GetActorLocation())
		> FMath::Square(Source->InteractionRange)) { return ESovCampaignResult::Invalid; }
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovEvidenceVisibility), false, Pawn);
	TArray<AActor*> Attachments;
	Pawn->GetAttachedActors(Attachments, true, true);
	Query.AddIgnoredActors(Attachments);
	if (const ASovPlayerCharacterBase* Character = Cast<ASovPlayerCharacterBase>(Pawn))
	{
		if (AActor* Visual = Character->GetCharacterVisual())
		{
			Query.AddIgnoredActor(Visual);
			Attachments.Reset(); Visual->GetAttachedActors(Attachments, true, true); Query.AddIgnoredActors(Attachments);
		}
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(), Source->GetOwner()->GetActorLocation(), ECC_Visibility, Query))
	{
		AActor* HitOwner = Hit.GetActor();
		TSet<AActor*> Visited;
		for (int32 Depth = 0; HitOwner && HitOwner != Source->GetOwner() && Depth < 8 && !Visited.Contains(HitOwner); ++Depth)
		{
			Visited.Add(HitOwner);
			HitOwner = HitOwner->GetAttachParentActor() ? HitOwner->GetAttachParentActor() : HitOwner->GetOwner();
		}
		if (HitOwner != Source->GetOwner()) { return ESovCampaignResult::Invalid; }
	}
	FSovEvidenceAcquisition Acquisition;
	Acquisition.EvidenceId = Source->EvidenceId;
	Acquisition.SourceId = Source->SourceId;
	Acquisition.Protagonist = ActiveMission->Protagonist;
	Acquisition.MissionId = ActiveMission->MissionId;
	Acquisition.AcquisitionBeat = Source->RequiredCompletedBeat;
	Acquisition.GrantedKnowledge = Source->GrantedKnowledge;
	Acquisition.AfterJournalSequence = Journal.Num();
	TGuardValue<bool> Mutation(bMutating, true);
	Evidence.Add(Acquisition);
	CharacterKnowledge.FindOrAdd(Acquisition.Protagonist).Knowledge.AppendTags(Source->GrantedKnowledge);
	OnEvidenceRecorded.Broadcast(Acquisition);
	return ESovCampaignResult::Applied;
}

void USovCampaignStateComponent::PrepareForSave_Implementation()
{
	// All durable records are already SaveGame fields. No secondary save object or replay is required.
}

bool USovCampaignStateComponent::ValidateSavedState() const
{
	if (SavedSchemaVersion != 1 || Missions.Num() != MissionDefinitions.Num()) { return false; }
	if (!ActiveMission)
	{
		return Missions.IsEmpty() && Journal.IsEmpty() && Evidence.IsEmpty() && CharacterKnowledge.IsEmpty()
			&& StateValues.IsEmpty() && ProtectedStateKeys.IsEmpty() && ViewedCinematics.IsEmpty();
	}
	const auto& Tags = FSovGameplayTags::Get();
	const auto IsHero = [&Tags](FGameplayTag Tag) { return Tag == Tags.Character_Player_Tarrik || Tag == Tags.Character_Player_Selene; };
	TSet<FName> ValidCinematics;
	for (const auto& Pair : MissionDefinitions)
	{
		FString Error;
		if (!Pair.Value || Pair.Key != Pair.Value->MissionId || !Pair.Value->ValidateDefinition(Error)
			|| !Missions.Contains(Pair.Key)) { return false; }
		for (const auto& Beat : Pair.Value->Beats) { if (!Beat.CinematicId.IsNone()) { ValidCinematics.Add(Beat.CinematicId); } }
	}
	const auto* ActiveDefinition = MissionDefinitions.Find(ActiveMission->MissionId);
	if (!ActiveDefinition || ActiveDefinition->Get() != ActiveMission) { return false; }
	TSet<FName> Viewed;
	for (const FName Id : ViewedCinematics)
	{
		if (!ValidCinematics.Contains(Id) || Viewed.Contains(Id)) { return false; }
		Viewed.Add(Id);
	}
	TMap<FName, FSovCampaignMissionRecord> Replay;
	TMap<FGameplayTag, FGameplayTagContainer> Knowledge;
	TMap<FGameplayTag, FGameplayTag> Facts;
	FGameplayTagContainer Protected;
	TSet<FGuid> Events;
	TSet<FGuid> CoActionRequests;
	TMap<FGuid, FSovEvidenceAcquisition> Sources;
	TMap<FGameplayTag, TSet<FName>> EvidenceByHero;
	int32 EvidenceIndex = 0;
	FName LastMission;
	const auto MissionSucceeded = [&Replay, this](FName Id)
	{
		const FSovCampaignMissionRecord* Record = Replay.Find(Id);
		const auto* Definition = MissionDefinitions.Find(Id);
		if (!Record || !Definition || !Definition->Get()) { return false; }
		for (const auto& Beat : (*Definition)->Beats)
		{ if (!Beat.bOptional && !Record->CompletedBeats.Contains(Beat.BeatId)) { return false; } }
		return true;
	};
	for (int32 Position = 0; Position <= Journal.Num(); ++Position)
	{
		while (EvidenceIndex < Evidence.Num() && Evidence[EvidenceIndex].AfterJournalSequence == Position)
		{
			const FSovEvidenceAcquisition& Item = Evidence[EvidenceIndex++];
			const auto* Definition = MissionDefinitions.Find(Item.MissionId);
			const auto* Progress = Replay.Find(Item.MissionId);
			if (Item.EvidenceId.IsNone() || !Item.SourceId.IsValid() || !IsHero(Item.Protagonist)
				|| !Definition || !Definition->Get() || (*Definition)->Protagonist != Item.Protagonist
				|| EvidenceByHero.FindOrAdd(Item.Protagonist).Contains(Item.EvidenceId)) { return false; }
			const FName NextMission = Position < Journal.Num() ? Journal[Position].MissionId : ActiveMission->MissionId;
			if (LastMission.IsNone())
			{
				if (Item.MissionId != NextMission) { return false; }
			}
			else if (Item.MissionId != LastMission)
			{
				if (Item.MissionId != NextMission || !MissionSucceeded(LastMission)
					|| !MissionDefinitions.FindChecked(LastMission)->AllowedSuccessorMissions.Contains(Item.MissionId)) { return false; }
			}
			if (!Item.AcquisitionBeat.IsNone() && (!Progress || !Progress->CompletedBeats.Contains(Item.AcquisitionBeat))) { return false; }
			if (const auto* Prior = Sources.Find(Item.SourceId); Prior && (Prior->EvidenceId != Item.EvidenceId
				|| Prior->MissionId != Item.MissionId || Prior->AcquisitionBeat != Item.AcquisitionBeat)) { return false; }
			Sources.Add(Item.SourceId, Item);
			EvidenceByHero.FindOrAdd(Item.Protagonist).Add(Item.EvidenceId);
			Knowledge.FindOrAdd(Item.Protagonist).AppendTags(Item.GrantedKnowledge);
		}
		if (Position == Journal.Num()) { break; }
		const FSovCampaignJournalEntry& Entry = Journal[Position];
		const auto* DefinitionPtr = MissionDefinitions.Find(Entry.MissionId);
		const USovCampaignDefinition* Definition = DefinitionPtr ? DefinitionPtr->Get() : nullptr;
		const FSovCampaignBeatDefinition* Beat = Definition ? Definition->FindBeat(Entry.BeatId) : nullptr;
		if (!Entry.EventId.IsValid() || Events.Contains(Entry.EventId) || Entry.Sequence != Position + 1
			|| !Beat || Entry.Protagonist != Definition->Protagonist) { return false; }
		if (Beat->bRequiresCoActionProof)
		{
			if (!Entry.CoActionRequestId.IsValid() || CoActionRequests.Contains(Entry.CoActionRequestId)
				|| Entry.CoActionCompanionId != Beat->RequiredCompanionId
				|| Entry.CoActionAnchorId != Beat->RequiredCoActionAnchorId || Entry.bPresentationSkipped) { return false; }
			CoActionRequests.Add(Entry.CoActionRequestId);
		}
		else if (Entry.CoActionRequestId.IsValid() || !Entry.CoActionCompanionId.IsNone() || !Entry.CoActionAnchorId.IsNone()) { return false; }
		if (!LastMission.IsNone() && LastMission != Entry.MissionId)
		{
			if (Replay.Contains(Entry.MissionId) || !MissionSucceeded(LastMission)
				|| !MissionDefinitions.FindChecked(LastMission)->AllowedSuccessorMissions.Contains(Entry.MissionId)) { return false; }
		}
		LastMission = Entry.MissionId;
		FSovCampaignMissionRecord& Progress = Replay.FindOrAdd(Entry.MissionId);
		if (Progress.CompletedBeats.Contains(Entry.BeatId)) { return false; }
		for (const FName Prior : Beat->PrerequisiteBeats) { if (!Progress.CompletedBeats.Contains(Prior)) { return false; } }
		for (const auto& Required : Beat->RequiredState)
		{ const FGameplayTag* Value = Facts.Find(Required.Key); if (!Value || *Value != Required.Value) { return false; } }
		if (!Knowledge.FindOrAdd(Entry.Protagonist).HasAllExact(Beat->RequiredKnowledge)) { return false; }
		if (Entry.bPresentationSkipped && (Beat->CinematicId.IsNone() || Beat->bInteractiveChoice || !Viewed.Contains(Beat->CinematicId))) { return false; }
		for (const auto& Write : Beat->StateWrites)
		{
			if (Protected.HasTagExact(Write.Key))
			{ const FGameplayTag* Existing = Facts.Find(Write.Key); if (!Existing || *Existing != Write.Value) { return false; } }
			Facts.Add(Write.Key, Write.Value);
			if (Write.bCanonProtected) { Protected.AddTag(Write.Key); }
		}
		Knowledge.FindOrAdd(Entry.Protagonist).AppendTags(Beat->GrantedKnowledge);
		Progress.CompletedBeats.Add(Entry.BeatId);
		Events.Add(Entry.EventId);
	}
	if (EvidenceIndex != Evidence.Num()) { return false; }
	if (!LastMission.IsNone() && LastMission != ActiveMission->MissionId)
	{
		if (!MissionSucceeded(LastMission) || !MissionDefinitions.FindChecked(LastMission)->AllowedSuccessorMissions.Contains(ActiveMission->MissionId)) { return false; }
	}
	for (const auto& Pair : Missions)
	{
		const auto* Replayed = Replay.Find(Pair.Key);
		const TArray<FName> Empty;
		const TArray<FName>& Completed = Replayed ? Replayed->CompletedBeats : Empty;
		if (!Replayed && Pair.Key != ActiveMission->MissionId) { return false; }
		if (Pair.Value.CompletedBeats.Num() != Completed.Num() || Pair.Value.bSucceeded != MissionSucceeded(Pair.Key)) { return false; }
		TSet<FName> Seen;
		for (const FName Id : Pair.Value.CompletedBeats)
		{ if (!Completed.Contains(Id) || Seen.Contains(Id)) { return false; } Seen.Add(Id); }
	}
	if (StateValues.Num() != Facts.Num() || ProtectedStateKeys.Num() != Protected.Num() || !ProtectedStateKeys.HasAllExact(Protected)) { return false; }
	for (const auto& Pair : StateValues)
	{ const FGameplayTag* Expected = Facts.Find(Pair.Key); if (!Expected || *Expected != Pair.Value) { return false; } }
	for (const auto& Pair : CharacterKnowledge)
	{
		const FGameplayTagContainer* Expected = Knowledge.Find(Pair.Key);
		if (!IsHero(Pair.Key) || (!Expected && !Pair.Value.Knowledge.IsEmpty())
			|| (Expected && (Expected->Num() != Pair.Value.Knowledge.Num() || !Expected->HasAllExact(Pair.Value.Knowledge)))) { return false; }
	}
	for (const auto& Pair : Knowledge)
	{
		const auto* Saved = CharacterKnowledge.Find(Pair.Key);
		if (!Pair.Value.IsEmpty() && (!Saved || !Saved->Knowledge.HasAllExact(Pair.Value))) { return false; }
	}
	return true;
}

void USovCampaignStateComponent::Load_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutating) { return; }
	TGuardValue<bool> Mutation(bMutating, true);
	bStateValid = ValidateSavedState();
	OnCampaignStateRestored.Broadcast(bStateValid);
}
