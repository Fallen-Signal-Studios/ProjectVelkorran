// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovConvergenceCompanionState.h"
#include "Companions/SovCompanionComponent.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovAurelionMissionDefinition.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerState.h"
#include "Framework/SovPlayerController.h"
#include "Resonance/SovResonanceComponent.h"
#include "AI/NPCDefinition.h"
#include "AI/NarrativeNPCController.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"

namespace
{
AActor* ResolveRecoveryAnchor(UWorld* World, FName Tag)
{
	if (!World || Tag.IsNone()) { return nullptr; }
	AActor* Found = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{ if (It->ActorHasTag(Tag)) { if (Found) { return nullptr; } Found = *It; } }
	return Found;
}

bool RestoreDepartureHold(USovCampaignStateComponent* State, const USovCampaignDefinition* Mission,
	ASovProtagonistCompanionCharacter* Companion, ASovPlayerCharacterBase* Leader, FString& Reason)
{
	if (!State || !State->IsStateValid() || !Mission
		|| !Mission->IsA<USovAurelionContraryWitnessMissionDefinition>()
		|| Mission->MissionId != TEXT("M13_ContraryWitness")
		|| !State->IsMissionComplete(Mission->MissionId)
		|| !State->IsBeatComplete(Mission->MissionId, TEXT("SeparateDepartures"))) { return true; }
	if (!IsValid(Companion) || !IsValid(Leader))
	{ Reason = TEXT("The completed departure has no valid companion or leader."); return false; }
	auto* Commands = Companion->GetCompanionComponent();
	if (!Commands)
	{ Reason = TEXT("The completed departure companion has no command owner."); return false; }
	if (Commands->HasAcceptedHoldPosition(Companion)) { return true; }
	if (!Commands->RequestCommand(Leader, ESovCompanionCommand::HoldPosition, Companion, Reason)) { return false; }
	if (!Commands->HasAcceptedHoldPosition(Companion))
	{ Reason = TEXT("The completed departure companion did not accept its standing position."); return false; }
	return true;
}
}

bool USovConvergenceCompanionState::RequiresCompanion(const USovCampaignDefinition* Mission) const
{
	if (!Mission || Mission->ProtagonistCompanions.IsEmpty()) { return false; }
	if (Mission->CompanionActivationBeat.IsNone()) { return true; }
	const auto* State = GetOwner() ? GetOwner()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
	return State && State->IsStateValid() && State->GetActiveMission() == Mission
		&& State->IsBeatComplete(Mission->MissionId, Mission->CompanionActivationBeat);
}

bool USovConvergenceCompanionState::SavedMissionRequiresCompanion(const USovCampaignDefinition* Mission,
	const TArray<uint8>* CampaignBytes, bool& bRequired, FString& Reason) const
{
	bRequired = Mission && !Mission->ProtagonistCompanions.IsEmpty();
	if (!bRequired || Mission->CompanionActivationBeat.IsNone()) { return true; }
	// Full loads stage companions before the controller record is applied. Read the validated
	// saved campaign journal, never the previous live world's convergence state.
	if (!CampaignBytes || !USovCampaignStateComponent::ValidateSerializedSave(*CampaignBytes, Reason))
	{ if (Reason.IsEmpty()) { Reason = TEXT("A gated companion restore requires its matching campaign journal."); } return false; }
	auto* Candidate = NewObject<USovCampaignStateComponent>();
	FMemoryReader Reader(*CampaignBytes); FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true;
	Candidate->Serialize(Archive);
	if (Archive.IsError() || Candidate->GetActiveMission() != Mission)
	{ Reason = TEXT("The companion restore journal names another mission."); return false; }
	bRequired = Candidate->IsBeatComplete(Mission->MissionId, Mission->CompanionActivationBeat);
	return true;
}

void USovConvergenceCompanionState::DestroyOwnedProxy(ASovProtagonistCompanionCharacter* Proxy)
{
	if (!IsValid(Proxy)) { return; }
	AController* Controller = Proxy->GetController(); Proxy->Destroy(); if (IsValid(Controller)) { Controller->Destroy(); }
}
void USovConvergenceCompanionState::RollbackStaged()
{
	if (bHasPreviousIncomingSnapshot)
	{
		auto* PC = Cast<APlayerController>(GetOwner()); auto* PS = PC ? PC->GetPlayerState<ASovPlayerState>() : nullptr;
		if (PS) { PS->StoreProtagonistSnapshot(PreviousIncomingSnapshot); }
		bHasPreviousIncomingSnapshot = false; PreviousIncomingSnapshot = FSovProtagonistSnapshot();
	}
	auto* Previous = Staged.Get(); Staged = nullptr; IncomingProxy = nullptr; StagedMission = nullptr;
	bRestoreActorRecord = false; bRecordApplied = false; PendingSnapshot = FSovCompanionProxySnapshot();
	DestroyOwnedProxy(Previous);
}
bool USovConvergenceCompanionState::StageHandoff(USovCampaignDefinition* Mission, FGameplayTag Incoming,
	ASovPlayerCharacterBase* Outgoing, FString& Reason, FName HandoffBeat)
{
	Reason.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Mission || !Outgoing || HasStagedProxy())
	{ Reason = TEXT("Companion handoff ownership is unavailable."); return false; }
	const auto* OutProfile = Mission->FindCompanionProfile(Outgoing->GetProtagonistIdentityTag());
	const auto* InProfile = Mission->FindCompanionProfile(Incoming);
	if (!OutProfile || !InProfile) { Reason = TEXT("Both protagonist companion profiles must be authored."); return false; }
	const auto* State = GetOwner()->FindComponentByClass<USovCampaignStateComponent>();
	const auto* Beat = Mission->FindBeat(HandoffBeat);
	if (!State || !State->IsStateValid() || State->GetActiveMission() != Mission || !Beat
		|| Beat->HandoffToProtagonist != Incoming || Beat->RequiredProtagonist != Outgoing->GetProtagonistIdentityTag())
	{ Reason = TEXT("Companion staging requires the current authored protagonist handoff."); return false; }
	const ESovObjectiveState ObjectiveState = State->GetObjectiveState(Mission->MissionId, HandoffBeat);
	if (ObjectiveState != ESovObjectiveState::Available && ObjectiveState != ESovObjectiveState::Active)
	{ Reason = TEXT("The authored protagonist handoff is not actionable."); return false; }
	const bool bAlreadyTogether = RequiresCompanion(Mission);
	const bool bActivatingCompanion = !bAlreadyTogether && !Mission->CompanionActivationBeat.IsNone()
		&& HandoffBeat == Mission->CompanionActivationBeat && !Beat->bIsolatedPerspectiveCut;
	if (!bAlreadyTogether)
	{
		if (IsValid(Active)) { Reason = TEXT("A protagonist companion is present before the authored meeting."); return false; }
		if (Beat->bIsolatedPerspectiveCut && !Mission->CompanionActivationBeat.IsNone())
		{
			// The incoming player uses their own saved kit (or the normal first-entry kit).
			// No partner resources, companion actor or resonance membership cross a solo cut.
			return true;
		}
		if (!bActivatingCompanion) { Reason = TEXT("The protagonist partnership has not been established."); return false; }
	}
	else if (Beat->bIsolatedPerspectiveCut)
	{ Reason = TEXT("An isolated perspective cut cannot dissolve an established partnership."); return false; }
	FSovCompanionProxySnapshot IncomingSnapshot;
	if (!bActivatingCompanion)
	{
		for (TActorIterator<ASovProtagonistCompanionCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetCompanionIdentity() == Incoming && It->GetCompanionComponent()->CompanionId == InProfile->CompanionId && It->IsAlive())
			{
				if (*It != Active || It->GetOwner() != GetOwner()) { Reason = TEXT("The incoming protagonist proxy is not owned by this campaign controller."); return false; }
				if (IncomingProxy) { IncomingProxy = nullptr; Reason = TEXT("More than one incoming protagonist proxy is alive."); return false; }
				IncomingProxy = *It;
			}
		}
		if (!IncomingProxy) { Reason = TEXT("The incoming protagonist's actual living companion must be present."); return false; }
		if (!IncomingProxy->CaptureProxySnapshot(Mission->MissionId, IncomingSnapshot, Reason)) { IncomingProxy = nullptr; return false; }
	}
	UClass* Class = OutProfile->CompanionClass.LoadSynchronous(); UNPCDefinition* Definition = OutProfile->CompanionDefinition.LoadSynchronous();
	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract) || !Definition)
	{ IncomingProxy = nullptr; Reason = TEXT("The outgoing companion class or NPC definition is missing."); return false; }
	Staged = GetWorld()->SpawnActorDeferred<ASovProtagonistCompanionCharacter>(Class, Outgoing->GetActorTransform(), GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Staged || !Staged->PrepareProxy(Outgoing->GetProtagonistIdentityTag(), OutProfile->CompanionId,
		Outgoing->GetNarrativeAbilitySystemComponent(), OutProfile->CuratedCompanionAbilities, Reason))
	{ RollbackStaged(); return false; }
	StagedMission = Mission; Staged->SetProxyStaged(true);
	Staged->GetCompanionComponent()->RecoveryAnchor = ResolveRecoveryAnchor(GetWorld(), OutProfile->EntryAnchorTag);
	if (!Staged->GetCompanionComponent()->RecoveryAnchor) { Reason = TEXT("The outgoing companion recovery anchor is missing or duplicated."); RollbackStaged(); return false; }
	auto* ExpectedStage = Staged.Get();
	ExpectedStage->SetNPCDefinition(Definition); ExpectedStage->FinishSpawning(Outgoing->GetActorTransform());
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage || StagedMission != Mission) { Reason = TEXT("Companion ownership changed during spawn."); return false; }
	ExpectedStage->EnsureEncounterController();
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage) { Reason = TEXT("Companion ownership changed during AI initialization."); return false; }
	if (auto* Controller = Cast<ANarrativeNPCController>(ExpectedStage->GetController()); Controller && Controller->GetActivityComponent()) { Controller->GetActivityComponent()->Deactivate(); }
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage) { Reason = TEXT("Companion ownership changed while staging its activity."); return false; }
	// The inactive player's durable kit stays unchanged; actual companion combat resources carry into control.
	auto* PC = Cast<APlayerController>(GetOwner()); auto* PS = PC ? PC->GetPlayerState<ASovPlayerState>() : nullptr;
	FSovProtagonistSnapshot PlayerSnapshot;
	if (!PS || !PS->FindProtagonistSnapshot(Incoming, PlayerSnapshot))
	{ Reason = TEXT("Convergence requires the previously learned incoming player kit snapshot."); RollbackStaged(); return false; }
	// At the first shared handoff the incoming hero is restored from the kit retained
	// during their solo approach; only subsequent swaps carry a live proxy's resources.
	if (bActivatingCompanion) { return true; }
	PreviousIncomingSnapshot = PlayerSnapshot; bHasPreviousIncomingSnapshot = true;
	PlayerSnapshot.Resources = IncomingSnapshot.Resources;
	if (!PS->StoreProtagonistSnapshot(PlayerSnapshot)) { Reason = TEXT("Incoming protagonist resources could not be retained."); RollbackStaged(); return false; }
	return true;
}
bool USovConvergenceCompanionState::StageSnapshot(const FSovCompanionProxySnapshot& Snapshot, USovCampaignDefinition* Mission, FString& Reason, bool bRestoreRecord)
{
	const auto* Profile = Mission ? Mission->FindCompanionProfile(Snapshot.Identity) : nullptr;
	if (!Profile || Snapshot.MissionId != Mission->MissionId || Snapshot.CompanionId != Profile->CompanionId || HasStagedProxy()
		|| Snapshot.ActorRecord.bDestroyed || Snapshot.ActorRecord.Transform.ContainsNaN()
		|| Snapshot.ActorRecord.ActorSoftClass.ToSoftObjectPath() != Profile->CompanionClass.ToSoftObjectPath())
	{ Reason = TEXT("The saved companion does not match this mission's explicit native profile."); return false; }
	UClass* Class = Profile->CompanionClass.LoadSynchronous(); UNPCDefinition* Definition = Profile->CompanionDefinition.LoadSynchronous();
	if (!Class || !Definition || Class->HasAnyClassFlags(CLASS_Abstract)) { Reason = TEXT("Saved companion content is unavailable."); return false; }
	for (TActorIterator<ASovProtagonistCompanionCharacter> It(GetWorld()); It; ++It)
	{ if (*It != Active && It->GetCompanionComponent()->CompanionId == Profile->CompanionId) { Reason = TEXT("A duplicate saved companion already exists in the map."); return false; } }
	Staged = GetWorld()->SpawnActorDeferred<ASovProtagonistCompanionCharacter>(Class, Snapshot.ActorRecord.Transform, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Staged || !Staged->PrepareProxyFromSnapshot(Snapshot, Profile->CuratedCompanionAbilities, Reason)) { RollbackStaged(); return false; }
	PendingSnapshot = Snapshot; StagedMission = Mission; bRestoreActorRecord = bRestoreRecord; bRecordApplied = false;
	Staged->GetCompanionComponent()->RecoveryAnchor = ResolveRecoveryAnchor(GetWorld(), Profile->EntryAnchorTag);
	if (!Staged->GetCompanionComponent()->RecoveryAnchor) { Reason = TEXT("The saved companion recovery anchor is missing or duplicated."); RollbackStaged(); return false; }
	auto* ExpectedStage = Staged.Get();
	ExpectedStage->SetProxyStaged(true); ExpectedStage->SetNPCDefinition(Definition);
	ExpectedStage->FinishSpawning(Snapshot.ActorRecord.Transform);
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage || StagedMission != Mission) { Reason = TEXT("Saved companion ownership changed during spawn."); return false; }
	ExpectedStage->EnsureEncounterController();
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage) { Reason = TEXT("Saved companion ownership changed during AI initialization."); return false; }
	if (auto* Controller = Cast<ANarrativeNPCController>(ExpectedStage->GetController()); Controller && Controller->GetActivityComponent()) { Controller->GetActivityComponent()->Deactivate(); }
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage) { Reason = TEXT("Saved companion ownership changed while staging its activity."); return false; }
	return true;
}
bool USovConvergenceCompanionState::StageSavedRecord(const TArray<uint8>& Bytes, USovCampaignDefinition* Mission, FGameplayTag Lead, FString& Reason,
	const TArray<uint8>* CampaignBytes)
{
	Reason.Reset();
	if (Bytes.IsEmpty() || Bytes.Num() > 16 * 1024 * 1024) { Reason = TEXT("Companion save record is missing or too large."); return false; }
	auto* Candidate = NewObject<USovConvergenceCompanionState>();
	FMemoryReader Reader(Bytes); FObjectAndNameAsStringProxyArchive Archive(Reader, true); Archive.ArIsSaveGame = true; Candidate->Serialize(Archive);
	if (Archive.IsError()) { Reason = TEXT("Companion save data could not be deserialized."); return false; }
	bool bRequired = false;
	if (!SavedMissionRequiresCompanion(Mission, CampaignBytes, bRequired, Reason)) { return false; }
	if (!Candidate->bHasSavedCompanion)
	{
		if (bRequired) { Reason = TEXT("A convergence save is missing its required protagonist companion record."); return false; }
		return true;
	}
	if (!bRequired) { Reason = TEXT("A saved convergence companion is not permitted before this mission's shared entry."); return false; }
	if (Candidate->SavedCompanion.Identity == Lead) { Reason = TEXT("A saved companion duplicates the controlled protagonist."); return false; }
	return StageSnapshot(Candidate->SavedCompanion, Mission, Reason);
}
bool USovConvergenceCompanionState::PollStaged(FString& Reason)
{
	Reason.Reset(); if (!Staged) { return true; }
	if (Staged->IsActorBeingDestroyed()) { Reason = TEXT("The staged protagonist companion was destroyed."); return false; }
	auto* ExpectedStage = Staged.Get();
	if (!ExpectedStage->CompleteProxyInitialization()) { return false; }
	if (!IsValid(ExpectedStage) || Staged != ExpectedStage) { Reason = TEXT("Companion ownership changed while applying its kit."); return false; }
	if (bRestoreActorRecord && !bRecordApplied)
	{
		auto* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
		const FNarrativeActorRecord Record = PendingSnapshot.ActorRecord;
		if (!Save || !Save->LoadActorFromRecord(ExpectedStage, Record)) { Reason = TEXT("The saved companion actor record could not restore."); return false; }
		if (!IsValid(ExpectedStage) || Staged != ExpectedStage) { Reason = TEXT("Companion ownership changed during actor restore."); return false; }
		bRecordApplied = true;
	}
	return true;
}
bool USovConvergenceCompanionState::CommitStaged(ASovPlayerCharacterBase* Leader, FString& Reason)
{
	Reason.Reset();
	if (!Staged)
	{
		auto* State = GetOwner()->FindComponentByClass<USovCampaignStateComponent>();
		auto* Mission = State ? State->GetActiveMission() : nullptr;
		if (!IsValid(Active))
		{ if (RequiresCompanion(Mission)) { Reason = TEXT("The committed shared route is missing its protagonist companion."); return false; } return true; }
		if (!RequiresCompanion(Mission))
		{ auto* Previous = Active.Get(); Active = nullptr; DestroyOwnedProxy(Previous); bHasSavedCompanion = false; return true; }
		if (!Leader || Active->GetCompanionIdentity() == Leader->GetProtagonistIdentityTag())
		{ Reason = TEXT("The established companion duplicates or has lost its controlled protagonist."); return false; }
		auto* Resonance = Leader->FindComponentByClass<USovResonanceComponent>();
		if (!Active->GetCompanionComponent()->SetLeader(Leader, Reason)
			|| (Mission->bAllowJointResonance && (!Resonance || !Resonance->RegisterPartner(Active->GetCompanionComponent(), Reason))))
		{ return false; }
		return RestoreDepartureHold(State, Mission, Active, Leader, Reason);
	}
	if (!IsValid(Leader) || !Leader->IsCharacterReady() || !StagedMission || !PollStaged(Reason)) { return false; }
	auto* State = GetOwner()->FindComponentByClass<USovCampaignStateComponent>();
	if (!State || State->GetActiveMission() != StagedMission || !RequiresCompanion(StagedMission))
	{ Reason = TEXT("The staged companion lost its established campaign partnership."); return false; }
	auto* ExpectedStage = Staged.Get();
	const auto StillOwnsStage = [this, ExpectedStage, State, Leader]()
	{ return IsValid(ExpectedStage) && Staged == ExpectedStage && IsValid(Leader) && IsValid(State)
		&& State->GetActiveMission() == StagedMission && RequiresCompanion(StagedMission); };
	auto* Controller = Cast<ANarrativeNPCController>(Staged->GetController());
	if (!Controller || !Controller->GetActivityComponent()) { Reason = TEXT("The companion has no Narrative activity owner."); return false; }
	Controller->GetActivityComponent()->Activate();
	if (!StillOwnsStage()) { Reason = TEXT("Companion ownership changed during activity activation."); return false; }
	if (!ExpectedStage->GetCompanionComponent()->SetLeader(Leader, Reason)) { return false; }
	if (!StillOwnsStage()) { Reason = TEXT("Companion ownership changed while assigning its leader."); return false; }
	auto* Resonance = Leader->FindComponentByClass<USovResonanceComponent>();
	if (StagedMission->bAllowJointResonance && (!Resonance || !Resonance->RegisterPartner(ExpectedStage->GetCompanionComponent(), Reason)))
	{ if (StillOwnsStage()) { ExpectedStage->GetCompanionComponent()->CancelContextCommand(); } return false; }
	if (!StillOwnsStage()) { Reason = TEXT("Companion ownership changed while registering the paired action."); return false; }
	// SetLeader installs Regroup. Restore the completed departure's self-hold while
	// staged movement is still disabled, before Regroup can move the saved pose.
	if (!RestoreDepartureHold(State, StagedMission, ExpectedStage, Leader, Reason))
	{ if (StillOwnsStage()) { ExpectedStage->GetCompanionComponent()->CancelContextCommand(); } return false; }
	if (!StillOwnsStage()) { Reason = TEXT("Companion ownership changed while restoring the departure hold."); return false; }
	ASovProtagonistCompanionCharacter* Retiring = IncomingProxy;
	ASovProtagonistCompanionCharacter* PreviousActive = Active;
	Active = Staged; Staged = nullptr; IncomingProxy = nullptr; StagedMission = nullptr;
	bHasPreviousIncomingSnapshot = false; PreviousIncomingSnapshot = FSovProtagonistSnapshot();
	Active->SetProxyStaged(false);
	DestroyOwnedProxy(Retiring);
	if (PreviousActive != Retiring && PreviousActive != Active) { DestroyOwnedProxy(PreviousActive); }
	bRestoreActorRecord = false; bRecordApplied = false; return true;
}
void USovConvergenceCompanionState::PrepareForSave_Implementation()
{
	bSaveCaptureValid = !HasStagedProxy();
	if (!bSaveCaptureValid) { return; }
	auto* State = GetOwner() ? GetOwner()->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
	if (!IsValid(Active))
	{
		if (State && RequiresCompanion(State->GetActiveMission())) { bSaveCaptureValid = false; return; }
		bHasSavedCompanion = false; SavedCompanion = FSovCompanionProxySnapshot(); return;
	}
	FString Reason;
	bSaveCaptureValid = State && RequiresCompanion(State->GetActiveMission())
		&& Active->CaptureProxySnapshot(State->GetActiveMission()->MissionId, SavedCompanion, Reason);
	bHasSavedCompanion = bSaveCaptureValid;
}
void USovConvergenceCompanionState::Serialize(FArchive& Ar)
{
	if (Ar.IsSaveGame() && Ar.IsSaving() && !bSaveCaptureValid) { Ar.SetError(); return; }
	Super::Serialize(Ar);
}
void USovConvergenceCompanionState::Load_Implementation()
{
	// Full map load is already staged by the controller; encounter rollback reuses the same owner while idle.
	auto* PC = Cast<ASovPlayerController>(GetOwner());
	if (!PC || PC->GetCampaignTransitionState() != ESovCampaignTransitionState::Idle || HasStagedProxy()) { return; }
	bEncounterRestorePending = true; EncounterRestoreError.Reset();
	const bool bRequired = RequiresCompanion(PC->GetCampaignState()->GetActiveMission());
	if (bRequired != bHasSavedCompanion)
	{ EncounterRestoreError = TEXT("Encounter restore companion membership disagrees with the restored campaign partnership."); return; }
	if (!bHasSavedCompanion)
	{
		auto* Previous = Active.Get(); Active = nullptr; DestroyOwnedProxy(Previous);
		bEncounterRestorePending = false; return;
	}
	if (IsValid(Active)) { Active->GetCompanionComponent()->CancelCoAction(); Active->GetCompanionComponent()->CancelContextCommand(); Active->SetProxyStaged(true); }
	IncomingProxy = Active;
	if (!StageSnapshot(SavedCompanion, PC->GetCampaignState()->GetActiveMission(), EncounterRestoreError)) { return; }
}
bool USovConvergenceCompanionState::FinishEncounterRestore(ASovPlayerCharacterBase* Leader, FString& Reason)
{
	Reason = EncounterRestoreError;
	if (!bEncounterRestorePending) { return true; }
	if (!Reason.IsEmpty() || !PollStaged(Reason)) { return false; }
	if (!CommitStaged(Leader, Reason)) { return false; }
	bEncounterRestorePending = false; return true;
}
void USovConvergenceCompanionState::EndPlay(const EEndPlayReason::Type Reason)
{
	RollbackStaged();
	auto* Previous = Active.Get(); Active = nullptr; DestroyOwnedProxy(Previous);
	Super::EndPlay(Reason);
}

bool USovConvergenceCompanionState::StageInitialCompanion(USovCampaignDefinition* Mission, FGameplayTag Lead, FString& Reason)
{
	Reason.Reset();
	if (!RequiresCompanion(Mission) || HasStagedProxy()) { return true; }
	const FSovCampaignCompanionProfile* Profile = Mission->ProtagonistCompanions.FindByPredicate(
		[Lead](const FSovCampaignCompanionProfile& Item) { return Item.Protagonist != Lead; });
	auto* PC = Cast<APlayerController>(GetOwner()); auto* PS = PC ? PC->GetPlayerState<ASovPlayerState>() : nullptr;
	FSovProtagonistSnapshot Kit;
	if (!Profile || Profile->EntryAnchorTag.IsNone() || !PS || !PS->FindProtagonistSnapshot(Profile->Protagonist, Kit))
	{ Reason = TEXT("Initial convergence requires the inactive protagonist's previously played kit and explicit companion entry anchor."); return false; }
	if (!Profile->CuratedCompanionAbilities.IsEmpty() && Kit.GrantedAbilities.IsEmpty())
	{ Reason = TEXT("This older protagonist snapshot has no unlocked-kit evidence; capture that protagonist once before convergence."); return false; }
	AActor* Anchor = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(Profile->EntryAnchorTag))
		{ if (Anchor) { Reason = TEXT("The initial companion entry anchor tag is duplicated."); return false; } Anchor = *It; }
	}
	if (!Anchor) { Reason = TEXT("The initial companion entry anchor is missing from this map."); return false; }
	FSovCompanionProxySnapshot Snapshot; Snapshot.MissionId = Mission->MissionId; Snapshot.Identity = Profile->Protagonist;
	Snapshot.CompanionId = Profile->CompanionId; Snapshot.Resources = Kit.Resources;
	Snapshot.ActorRecord.ActorName = Profile->CompanionId; Snapshot.ActorRecord.ActorGUID = FGuid::NewGuid();
	Snapshot.ActorRecord.ActorSoftClass = Profile->CompanionClass.ToSoftObjectPath(); Snapshot.ActorRecord.Transform = Anchor->GetActorTransform();
	for (const auto& Class : Profile->CuratedCompanionAbilities)
	{
		const auto* Found = Kit.GrantedAbilities.FindByPredicate([Class](const FSovProtagonistAbilitySnapshot& Item) { return Item.AbilityClass == Class; });
		if (Found) { FSovCompanionKitGrant Grant; Grant.Ability = Found->AbilityClass; Grant.Level = Found->Level; Snapshot.Grants.Add(Grant); }
	}
	return StageSnapshot(Snapshot, Mission, Reason, false);
}
