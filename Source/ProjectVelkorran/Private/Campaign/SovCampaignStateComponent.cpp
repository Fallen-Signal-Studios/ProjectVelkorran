// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignStateComponent.h"
#include "Save/SovSaveSubsystem.h"
#include "Settings/SovGameUserSettings.h"
#include "Engine/GameInstance.h"

#include "Campaign/SovCampaignPolicy.h"
#include "Companions/SovCoActionAnchor.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Cinematics/SovCampaignCinematicComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Sovereign/SovGameplayTags.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Tales/TalesComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

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
	for (FName Required : Definition->RequiredPriorConsequenceIds)
	{ FSovConsequenceRecord Record; if (!FindConsequence(Required, NAME_None, Record)) { return false; } }
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
	if (!CanEnterMission(Definition) || !DoesCurrentPawnMatch(ActiveMission == Definition ? GetActiveProtagonist() : Definition->Protagonist)) { return ESovCampaignResult::Invalid; }
	if (ActiveMission == Definition) { return ESovCampaignResult::AlreadyApplied; }
	TGuardValue<bool> Mutation(bMutating, true);
	ActiveMission = Definition;
	ActiveProtagonist = Definition->Protagonist;
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

ESovCampaignResult USovCampaignStateComponent::CompleteBeatInternal(FName BeatId, bool bSkipPresentation, ASovCoActionAnchor* CoActionSource, const FGuid& HandoffRequestId, USovCampaignCinematicComponent* CinematicSource)
{
	if (!HasAuthorityOwner()) { return ESovCampaignResult::NotAuthority; }
	if (bMutating) { return ESovCampaignResult::Busy; }
	FString DefinitionError;
	if (!IsValid(ActiveMission) || !ActiveMission->ValidateDefinition(DefinitionError)
		|| (!HandoffRequestId.IsValid() && !DoesCurrentPawnMatch(GetActiveProtagonist()))) { return ESovCampaignResult::Invalid; }
	const FSovCampaignBeatDefinition* Beat = ActiveMission->FindBeat(BeatId);
	if (!Beat || (Beat->RequiredProtagonist.IsValid() && Beat->RequiredProtagonist != GetActiveProtagonist())) { return ESovCampaignResult::Invalid; }
	if (Beat->bRequiresCinematicProof)
	{
		if (!IsValid(CinematicSource) || !CinematicSource->HasCommitReceipt(this, BeatId, bSkipPresentation)) { return ESovCampaignResult::Invalid; }
	}
	else if (CinematicSource) { return ESovCampaignResult::Invalid; }
	if (Beat->HandoffToProtagonist.IsValid())
	{
		if (!HandoffRequestId.IsValid() || bSkipPresentation || CoActionSource
			|| !DoesCurrentPawnMatch(Beat->HandoffToProtagonist)) { return ESovCampaignResult::Invalid; }
	}
	else if (HandoffRequestId.IsValid()) { return ESovCampaignResult::Invalid; }
	bool bPrerequisitesMet = true;
	for (FName Prior : Beat->PrerequisiteBeats) { bPrerequisitesMet &= IsBeatComplete(ActiveMission->MissionId, Prior); }
	for (const FSovCampaignStateWrite& Required : Beat->RequiredState)
	{ bPrerequisitesMet &= GetStateValue(Required.Key) == Required.Value; }
	const auto Policy = SovCampaignPolicy::CompleteBeat(true, true, IsBeatComplete(ActiveMission->MissionId, BeatId),
		bPrerequisitesMet, HasKnowledge(GetActiveProtagonist(), Beat->RequiredKnowledge), StateWritesValid(Beat->StateWrites),
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
			|| CoActionSource->RequiredCompanionId != Beat->RequiredCompanionId) { return ESovCampaignResult::Invalid; }
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
	Entry.Protagonist = GetActiveProtagonist();
	if (HandoffRequestId.IsValid())
	{
		Entry.HandoffRequestId = HandoffRequestId; Entry.HandoffToProtagonist = Beat->HandoffToProtagonist;
		Entry.HandoffAnchorId = Beat->RequiredHandoffAnchorId;
	}
	Entry.bPresentationSkipped = bSkipPresentation;
	if (CinematicSource) { Entry.CinematicSessionId = CinematicSource->SessionId; }
	double PlaySeconds = 0.0;
	if (const auto* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const auto* Save = GI->GetSubsystem<USovSaveSubsystem>()) { PlaySeconds = Save->GetCampaignPlaySeconds(); }
	}
	for (const auto& Definition : Beat->Consequences)
	{
		FSovConsequenceRecord Existing;
		if (FindConsequence(Definition.ConsequenceId, NAME_None, Existing)) { return ESovCampaignResult::ProtectedStateConflict; }
		FSovConsequenceRecord Record; Record.Definition = Definition; Record.MissionId = Entry.MissionId; Record.BeatId = Entry.BeatId;
		Record.ResolvedInstigatorId = Definition.InstigatorId.IsNone() ? NarrativeIdentity(Entry.Protagonist) : Definition.InstigatorId;
		Record.Sequence = Entry.Sequence; Record.PlaySeconds = PlaySeconds;
		Entry.Consequences.Add(Record);
	}
	Entry.RelationshipMemories = Beat->RelationshipMemories;
	for (const auto& Memory : Entry.RelationshipMemories)
	{
		if (Journal.ContainsByPredicate([&Memory](const auto& Existing)
			{ return Existing.RelationshipMemories.ContainsByPredicate([&Memory](const auto& Previous) { return Previous.MemoryId == Memory.MemoryId; }); }))
		{ return ESovCampaignResult::ProtectedStateConflict; }
		FSovConsequenceRecord Prior;
		const auto* Own = Entry.Consequences.FindByPredicate([&Memory](const auto& Record) { return Record.Definition.ConsequenceId == Memory.ConsequenceId; });
		if (!Own && !FindConsequence(Memory.ConsequenceId, NAME_None, Prior)) { return ESovCampaignResult::PrerequisiteMissing; }
		const auto& Fact = Own ? *Own : Prior;
		if (Memory.LearnedThrough == ESovKnowledgeMethod::Witnessed && Memory.HolderId != Fact.ResolvedInstigatorId
			&& !Fact.Definition.WitnessIds.Contains(Memory.HolderId)) { return ESovCampaignResult::KnowledgeMissing; }
	}
	TArray<FSovEvidenceAcquisition> CriticalAcquisitions;
	for (const auto& Definition : Beat->CriticalEvidence)
	{
		if (KnowsEvidence(Definition->EvidenceId, Entry.Protagonist)) { continue; }
		FSovEvidenceAcquisition Record; Record.Definition = Definition; Record.EvidenceId = Definition->EvidenceId;
		Record.SourceId = FGuid::NewGuid(); Record.CriticalBeatEventId = Entry.EventId; Record.Protagonist = Entry.Protagonist;
		Record.MissionId = Entry.MissionId; Record.AcquisitionBeat = Entry.BeatId; Record.AfterJournalSequence = Entry.Sequence;
		Record.SourceLocationId = Entry.BeatId; Record.CustodianId = Definition->OriginalCustodian;
		if (!ValidateEvidenceStep(Record, Evidence)) { return ESovCampaignResult::Invalid; }
		CriticalAcquisitions.Add(Record);
	}
	// A failed narrative precondition must not consume the physical co-action proof.
	if (CoActionSource && !CoActionSource->ConsumeReceipt(this)) { return ESovCampaignResult::Invalid; }
	if (CinematicSource && !CinematicSource->ConsumeCommitReceipt(this, BeatId, bSkipPresentation)) { return ESovCampaignResult::Invalid; }
	UNarrativeDataTask* Task = Beat->CompletionTask;
	bool bMissionJustSucceeded = false;
	TGuardValue<bool> Mutation(bMutating, true);
	{
		if (CinematicSource && !bSkipPresentation) { ViewedCinematics.AddUnique(Beat->CinematicId); }
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
		Evidence.Append(CriticalAcquisitions);
		if (Entry.HandoffToProtagonist.IsValid()) { ActiveProtagonist = Entry.HandoffToProtagonist; }
	}
	// Keep the transaction guard through authored notifications: callback-driven commands return Busy.
	// They may schedule a later command after the complete commit/notification sequence.
	if (Task)
	{
		if (UTalesComponent* Tales = GetOwner()->FindComponentByClass<UTalesComponent>())
		{ Tales->CompleteNarrativeDataTask(Task, Entry.BeatId.ToString(), 1); }
	}
	for (const auto& Acquisition : CriticalAcquisitions) { OnEvidenceRecorded.Broadcast(Acquisition); }
	OnBeatCommitted.Broadcast(Entry);
	if (bMissionJustSucceeded) { OnMissionChanged.Broadcast(Entry.MissionId, true); }
	if (bMissionJustSucceeded)
	{ if (USovGameUserSettings* Settings = USovGameUserSettings::Get()) { Settings->UnlockSovereignFromCampaign(this); } }
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (USovSaveSubsystem* Slots = GetWorld()->GetGameInstance()->GetSubsystem<USovSaveSubsystem>())
		{
			if (Beat->bCanonGate || bMissionJustSucceeded)
			{ Slots->QueueAutosave(ESovSaveBoundary::CanonGate, Entry.BeatId); }
			for (const auto& Next : ActiveMission->Beats)
			{
				if (!Next.bInteractiveChoice || IsBeatComplete(ActiveMission->MissionId, Next.BeatId)) { continue; }
				bool bReady = true;
				for (FName Prior : Next.PrerequisiteBeats) { bReady &= IsBeatComplete(ActiveMission->MissionId, Prior); }
				if (bReady) { Slots->QueueAutosave(ESovSaveBoundary::BeforeChoice, Next.BeatId); }
			}
		}
	}
	return ESovCampaignResult::Applied;
}

ESovCampaignResult USovCampaignStateComponent::CompleteCinematic(USovCampaignCinematicComponent* Source, bool bSkipped)
{
	return IsValid(Source) ? CompleteBeatInternal(Source->BeatId, bSkipped, nullptr, FGuid(), Source) : ESovCampaignResult::Invalid;
}

bool USovCampaignStateComponent::CanSkipCinematic(FName BeatId) const
{
	const auto* Beat = ActiveMission ? ActiveMission->FindBeat(BeatId) : nullptr;
	return bStateValid && !bMutating && Beat && !Beat->bInteractiveChoice && !Beat->CinematicId.IsNone()
		&& ViewedCinematics.Contains(Beat->CinematicId) && DoesCurrentPawnMatch(GetActiveProtagonist());
}

bool USovCampaignStateComponent::RecordCinematicViewed(FName BeatId)
{
	if (!HasAuthorityOwner() || bMutating || !ActiveMission || !DoesCurrentPawnMatch(GetActiveProtagonist())) { return false; }
	const FSovCampaignBeatDefinition* Beat = ActiveMission->FindBeat(BeatId);
	if (!Beat || Beat->CinematicId.IsNone() || Beat->bRequiresCinematicProof) { return false; }
	for (FName Prior : Beat->PrerequisiteBeats) { if (!IsBeatComplete(ActiveMission->MissionId, Prior)) { return false; } }
	for (const FSovCampaignStateWrite& Required : Beat->RequiredState)
	{ if (GetStateValue(Required.Key) != Required.Value) { return false; } }
	if (!HasKnowledge(GetActiveProtagonist(), Beat->RequiredKnowledge)) { return false; }
	ViewedCinematics.AddUnique(Beat->CinematicId);
	return true;
}

bool USovCampaignStateComponent::KnowsEvidence(FName EvidenceId, FGameplayTag Protagonist) const
{
	return GetEvidenceStage(EvidenceId, Protagonist) != ESovEvidenceStage::Unknown;
}

ESovCampaignResult USovCampaignStateComponent::AcquireEvidence(USovEvidenceSourceComponent* Source)
{
	if (!HasAuthorityOwner()) { return ESovCampaignResult::NotAuthority; }
	if (bMutating) { return ESovCampaignResult::Busy; }
	FString SourceError;
	if (!IsValid(Source) || !Source->ValidateConfiguration(SourceError) || !IsValid(Source->GetOwner()) || !IsValid(ActiveMission)
		|| !DoesCurrentPawnMatch(GetActiveProtagonist()) || Source->GetWorld() != GetWorld()
		|| Source->EvidenceId.IsNone() || !Source->SourceId.IsValid()
		|| Source->AcquisitionMission != ActiveMission->MissionId
		|| !Source->AllowedProtagonists.HasTagExact(GetActiveProtagonist())
		|| !FMath::IsFinite(Source->InteractionRange) || Source->InteractionRange <= 0.f || Source->InteractionRange > 5000.f
		|| !Source->ValidateAcquisitionMode(Cast<APlayerController>(GetOwner())))
	{ return ESovCampaignResult::Invalid; }
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) { continue; }
		TInlineComponentArray<USovEvidenceSourceComponent*> Components; It->GetComponents(Components);
		for (const auto* Candidate : Components)
		{ if (Candidate != Source && Candidate->SourceId == Source->SourceId) { return ESovCampaignResult::Invalid; } }
	}
	for (const FSovEvidenceAcquisition& Existing : Evidence)
	{
		if (Existing.SourceId == Source->SourceId && (Existing.EvidenceId != Source->EvidenceId
			|| Existing.MissionId != Source->AcquisitionMission)) { return ESovCampaignResult::Invalid; }
	}
	const auto CurrentStage = GetEvidenceStage(Source->EvidenceId, GetActiveProtagonist());
	if (CurrentStage >= Source->RequestedStage && Source->RequestedStage != ESovEvidenceStage::Distributed) { return ESovCampaignResult::AlreadyApplied; }
	if (Source->RequestedStage == ESovEvidenceStage::Distributed && HasEvidenceCopy(Source->EvidenceId, Source->CopyDestination)) { return ESovCampaignResult::AlreadyApplied; }
	if (Source->Definition && Source->Definition->bCriticalPath && CurrentStage == ESovEvidenceStage::Unknown) { return ESovCampaignResult::PrerequisiteMissing; }
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
	Acquisition.Definition = Source->Definition; Acquisition.Stage = Source->RequestedStage;
	Acquisition.SourceLocationId = Source->SourceLocationId; Acquisition.CustodianId = Source->CustodianId;
	Acquisition.SupportingEvidenceId = Source->SupportingEvidenceId; Acquisition.CopyDestination = Source->CopyDestination;
	Acquisition.Publicity = Source->Publicity; Acquisition.WitnessIds = Source->WitnessIds;
	Acquisition.SourceId = Source->SourceId;
	Acquisition.Protagonist = GetActiveProtagonist();
	Acquisition.MissionId = ActiveMission->MissionId;
	Acquisition.AcquisitionBeat = Source->RequiredCompletedBeat;
	Acquisition.GrantedKnowledge = Source->GrantedKnowledge;
	Acquisition.AfterJournalSequence = Journal.Num();
	if (!ValidateEvidenceStep(Acquisition, Evidence)) { return ESovCampaignResult::PrerequisiteMissing; }
	TGuardValue<bool> Mutation(bMutating, true);
	Evidence.Add(Acquisition);
	CharacterKnowledge.FindOrAdd(Acquisition.Protagonist).Knowledge.AppendTags(Source->GrantedKnowledge);
	for (const auto Hero : { FSovGameplayTags::Get().Character_Player_Tarrik, FSovGameplayTags::Get().Character_Player_Selene })
	{
		if (Acquisition.CopyDestination == NarrativeIdentity(Hero) || Acquisition.WitnessIds.Contains(NarrativeIdentity(Hero)))
		{ CharacterKnowledge.FindOrAdd(Hero).Knowledge.AppendTags(Acquisition.GrantedKnowledge); }
	}
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
			&& StateValues.IsEmpty() && ProtectedStateKeys.IsEmpty() && ViewedCinematics.IsEmpty() && !ActiveProtagonist.IsValid();
	}
	const auto& Tags = FSovGameplayTags::Get();
	const auto IsHero = [&Tags](FGameplayTag Tag) { return Tag == Tags.Character_Player_Tarrik || Tag == Tags.Character_Player_Selene; };
	TSet<FName> ValidCinematics, ManagedCinematics, ManagedViewed;
	for (const auto& Pair : MissionDefinitions)
	{
		FString Error;
		if (!Pair.Value || Pair.Key != Pair.Value->MissionId || !Pair.Value->ValidateDefinition(Error)
			|| !Missions.Contains(Pair.Key)) { return false; }
		for (const auto& Beat : Pair.Value->Beats)
        {
            if (!Beat.CinematicId.IsNone()) { ValidCinematics.Add(Beat.CinematicId); }
            if (Beat.bRequiresCinematicProof) { ManagedCinematics.Add(Beat.CinematicId); }
        }
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
	TMap<FName, FSovConsequenceRecord> SeenConsequences;
	TSet<FName> SeenMemories;
	TSet<FGuid> CoActionRequests;
	TSet<FGuid> HandoffRequests;
	TSet<FGuid> CinematicSessions;
	TMap<FName, FGameplayTag> ReplayedLeads;
	const auto LeadFor = [&ReplayedLeads](const USovCampaignDefinition* Definition)
	{
		const FGameplayTag* Lead = ReplayedLeads.Find(Definition->MissionId);
		return Lead ? *Lead : Definition->Protagonist;
	};
	TMap<FGuid, FSovEvidenceAcquisition> Sources;
	TArray<FSovEvidenceAcquisition> ReplayedEvidence;
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
				|| !Definition || !Definition->Get()
				|| (!Item.CriticalBeatEventId.IsValid() && LeadFor(Definition->Get()) != Item.Protagonist)
				|| !ValidateEvidenceStep(Item, ReplayedEvidence)) { return false; }
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
			if (Item.CriticalBeatEventId.IsValid())
			{
				if (!Item.Definition || !Item.Definition->bCriticalPath || Position <= 0) { return false; }
				const auto& CriticalEntry = Journal[Position - 1];
				const auto* CriticalBeat = Definition->Get()->FindBeat(CriticalEntry.BeatId);
				if (!CriticalBeat || CriticalEntry.EventId != Item.CriticalBeatEventId || CriticalEntry.MissionId != Item.MissionId
					|| CriticalEntry.Protagonist != Item.Protagonist || Item.AcquisitionBeat != CriticalEntry.BeatId
					|| !CriticalBeat->CriticalEvidence.Contains(Item.Definition) || !Item.GrantedKnowledge.IsEmpty()
					|| Item.SourceLocationId != CriticalEntry.BeatId || Item.CustodianId != Item.Definition->OriginalCustodian) { return false; }
			}
			else if (Item.Definition && Item.Definition->bCriticalPath && Item.Stage == ESovEvidenceStage::Observed) { return false; }
			ReplayedEvidence.Add(Item);
			Knowledge.FindOrAdd(Item.Protagonist).AppendTags(Item.GrantedKnowledge);
			for (const auto Hero : { Tags.Character_Player_Tarrik, Tags.Character_Player_Selene })
			{
				if (Item.CopyDestination == NarrativeIdentity(Hero) || Item.WitnessIds.Contains(NarrativeIdentity(Hero)))
				{ Knowledge.FindOrAdd(Hero).AppendTags(Item.GrantedKnowledge); }
			}
		}
		if (Position == Journal.Num()) { break; }
		const FSovCampaignJournalEntry& Entry = Journal[Position];
		const auto* DefinitionPtr = MissionDefinitions.Find(Entry.MissionId);
		const USovCampaignDefinition* Definition = DefinitionPtr ? DefinitionPtr->Get() : nullptr;
		const FSovCampaignBeatDefinition* Beat = Definition ? Definition->FindBeat(Entry.BeatId) : nullptr;
		if (!Entry.EventId.IsValid() || Events.Contains(Entry.EventId) || Entry.Sequence != Position + 1
			|| !Beat || Entry.Protagonist != LeadFor(Definition)
			|| (Beat->RequiredProtagonist.IsValid() && Beat->RequiredProtagonist != Entry.Protagonist)) { return false; }
		for (FName Required : Definition->RequiredPriorConsequenceIds)
		{ if (!SeenConsequences.Contains(Required)) { return false; } }
		if (Entry.Consequences.Num() != Beat->Consequences.Num() || Entry.RelationshipMemories != Beat->RelationshipMemories) { return false; }
		for (int32 Index = 0; Index < Entry.Consequences.Num(); ++Index)
		{
			const auto& Record = Entry.Consequences[Index]; const auto& Authored = Beat->Consequences[Index];
			if (!(Record.Definition == Authored) || Record.MissionId != Entry.MissionId || Record.BeatId != Entry.BeatId
				|| Record.Sequence != Entry.Sequence || !FMath::IsFinite(Record.PlaySeconds) || Record.PlaySeconds < 0.0
				|| Record.ResolvedInstigatorId != (Authored.InstigatorId.IsNone() ? NarrativeIdentity(Entry.Protagonist) : Authored.InstigatorId)
				|| SeenConsequences.Contains(Authored.ConsequenceId)) { return false; }
			SeenConsequences.Add(Authored.ConsequenceId, Record);
		}
		for (const auto& Memory : Entry.RelationshipMemories)
		{
			const auto* Fact = SeenConsequences.Find(Memory.ConsequenceId);
			if (!Fact || SeenMemories.Contains(Memory.MemoryId)
				|| (Memory.LearnedThrough == ESovKnowledgeMethod::Witnessed && Fact->ResolvedInstigatorId != Memory.HolderId
					&& !Fact->Definition.WitnessIds.Contains(Memory.HolderId))) { return false; }
			SeenMemories.Add(Memory.MemoryId);
		}
		if (Beat->bRequiresCinematicProof)
		{
			if (!Entry.CinematicSessionId.IsValid() || CinematicSessions.Contains(Entry.CinematicSessionId) || !Viewed.Contains(Beat->CinematicId)) { return false; }
			if (Entry.bPresentationSkipped && !ManagedViewed.Contains(Beat->CinematicId)) { return false; }
            if (!Entry.bPresentationSkipped) { ManagedViewed.Add(Beat->CinematicId); }
            CinematicSessions.Add(Entry.CinematicSessionId);
		}
		else if (Entry.CinematicSessionId.IsValid()) { return false; }
		if (Beat->HandoffToProtagonist.IsValid())
		{
			if (!Entry.HandoffRequestId.IsValid() || HandoffRequests.Contains(Entry.HandoffRequestId)
				|| Entry.HandoffToProtagonist != Beat->HandoffToProtagonist
				|| Entry.HandoffAnchorId != Beat->RequiredHandoffAnchorId || Entry.bPresentationSkipped) { return false; }
			HandoffRequests.Add(Entry.HandoffRequestId);
		}
		else if (Entry.HandoffRequestId.IsValid() || Entry.HandoffToProtagonist.IsValid() || !Entry.HandoffAnchorId.IsNone()) { return false; }
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
		if (Entry.HandoffToProtagonist.IsValid()) { ReplayedLeads.Add(Entry.MissionId, Entry.HandoffToProtagonist); }
	}
	for (FName Id : Viewed) { if (ManagedCinematics.Contains(Id) && !ManagedViewed.Contains(Id)) { return false; } }
    if (EvidenceIndex != Evidence.Num() || GetActiveProtagonist() != LeadFor(ActiveMission)) { return false; }
	for (FName Required : ActiveMission->RequiredPriorConsequenceIds)
	{ if (!SeenConsequences.Contains(Required)) { return false; } }
	for (const auto& Entry : Journal)
	{
		const auto* Beat = MissionDefinitions.FindChecked(Entry.MissionId)->FindBeat(Entry.BeatId);
		for (const auto& Critical : Beat->CriticalEvidence)
		{
			if (!Evidence.ContainsByPredicate([&Entry, Critical](const auto& Item)
				{ return Item.EvidenceId == Critical->EvidenceId && Item.AfterJournalSequence <= Entry.Sequence
					&& (Item.Protagonist == Entry.Protagonist || Item.WitnessIds.Contains(NarrativeIdentity(Entry.Protagonist))
						|| Item.CopyDestination == NarrativeIdentity(Entry.Protagonist)); })) { return false; }
		}
	}
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

FGameplayTag USovCampaignStateComponent::GetActiveProtagonist() const
{
	return ActiveProtagonist.IsValid() ? ActiveProtagonist : (ActiveMission ? ActiveMission->Protagonist : FGameplayTag());
}

ESovCampaignResult USovCampaignStateComponent::CompleteAuthoredHandoff(FName BeatId, const FGuid& RequestId)
{
	return RequestId.IsValid() ? CompleteBeatInternal(BeatId, false, nullptr, RequestId) : ESovCampaignResult::Invalid;
}

bool USovCampaignStateComponent::GetSerializedActiveProtagonist(const TArray<uint8>& Bytes, FGameplayTag& OutProtagonist, FString& OutError)
{
	OutProtagonist = FGameplayTag();
	if (Bytes.IsEmpty() || Bytes.Num() > 16 * 1024 * 1024) { OutError = TEXT("Campaign record is empty or exceeds its native size limit."); return false; }
	USovCampaignStateComponent* Candidate = NewObject<USovCampaignStateComponent>();
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true;
	Candidate->Serialize(Archive);
	if (Archive.IsError() || !Candidate->ValidateSavedState())
	{ OutError = TEXT("Campaign journal, evidence or protagonist handoff proof is invalid."); return false; }
	OutProtagonist = Candidate->GetActiveProtagonist(); OutError.Reset(); return true;
}

bool USovCampaignStateComponent::ValidateSerializedSave(const TArray<uint8>& Bytes, FString& OutError)
{
	FGameplayTag Lead; return GetSerializedActiveProtagonist(Bytes, Lead, OutError);
}

void USovCampaignStateComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaveGame() && Ar.IsSaving() && (bMutating || !bStateValid)) { Ar.SetError(); return; }
	Super::Serialize(Ar);
}
