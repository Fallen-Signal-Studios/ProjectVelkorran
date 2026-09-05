// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Framework/SovPlayerController.h"
#include "Framework/SovApplicationLifecycleComponent.h"
#include "Narrative/SovNarrativeCueComponent.h"
#include "Feedback/SovHapticFeedbackComponent.h"
#include "UI/SovFrontendComponent.h"
#include "UI/SovNativeGameplayHUD.h"
#include "UI/Dialogue/SovDialoguePresentationComponent.h"

#include "AI/NarrativeCharacterSubsystem.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Save/SovSaveSubsystem.h"
#include "Settings/SovGameUserSettings.h"
#include "Engine/GameInstance.h"
#include "Campaign/SovCampaignPolicy.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Character/PlayerDefinition.h"
#include "Character/CharacterAppearance.h"
#include "Components/SovCorruptionComponent.h"
#include "Framework/SovPlayerState.h"
#include "Framework/SovCampaignGameMode.h"
#include "GAS/AbilityConfiguration.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "NarrativeGameplayTags.h"
#include "SkillTrees/SkillTreeComponent.h"
#include "Progression/SovTechniqueComponent.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

ASovPlayerController::ASovPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CampaignState = CreateDefaultSubobject<USovCampaignStateComponent>(TEXT("SovCampaignState"));
	NarrativeCues = CreateDefaultSubobject<USovNarrativeCueComponent>(TEXT("SovNarrativeCues"));
	ConvergenceCompanionState = CreateDefaultSubobject<USovConvergenceCompanionState>(TEXT("SovConvergenceCompanionState"));
	HapticFeedback = CreateDefaultSubobject<USovHapticFeedbackComponent>(TEXT("HapticFeedback"));
	Frontend = CreateDefaultSubobject<USovFrontendComponent>(TEXT("NativeFrontend"));
	DialoguePresentation = CreateDefaultSubobject<USovDialoguePresentationComponent>(TEXT("DialoguePresentation"));
	ApplicationLifecycle = CreateDefaultSubobject<USovApplicationLifecycleComponent>(TEXT("ApplicationLifecycle"));
	GameplayHUDClass = USovNativeGameplayHUD::StaticClass();
}

void ASovPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureGameplayHUDCreated();
	if (Frontend) { Frontend->RefreshFrontend(); }
}
bool ASovPlayerController::OpenAccessibilitySettings()
{
	EnsureGameplayHUDCreated();
	return Frontend && Frontend->OpenAccessibilitySettings();
}

bool ASovPlayerController::CanReleaseSystemPause() const { return SystemPauseOwners.IsEmpty() && !bExternalPauseRequested; }
bool ASovPlayerController::IsGameplayAbilityInputSuppressed() const
{ return ApplicationLifecycle && ApplicationLifecycle->IsGameplayInterrupted(); }
bool ASovPlayerController::SetPause(bool bPause, FCanUnpause CanUnpauseDelegate)
{
	// A menu/Blueprint pause remains owned after a platform interruption ends.
	const bool bPreviousExternalPause = bExternalPauseRequested;
	bExternalPauseRequested = bPause;
	if (!bPause && !SystemPauseOwners.IsEmpty()) { return false; }
	const bool bSucceeded = Super::SetPause(bPause, CanUnpauseDelegate);
	if (bPause && !bSucceeded) { bExternalPauseRequested = bPreviousExternalPause; }
	return bSucceeded;
}
bool ASovPlayerController::AcquireSystemPause(FName Owner)
{
	if (Owner.IsNone() || !HasAuthority() || GetNetMode() != NM_Standalone || !GetWorld()) { return false; }
	if (SystemPauseOwners.Contains(Owner)) { return true; }
	if (SystemPauseOwners.IsEmpty() && GetWorld()->IsPaused()) { bExternalPauseRequested = true; }
	SystemPauseOwners.Add(Owner);
	if (Super::SetPause(true, FCanUnpause::CreateUObject(this, &ThisClass::CanReleaseSystemPause))) { return true; }
	SystemPauseOwners.Remove(Owner); return false;
}
void ASovPlayerController::ReleaseSystemPause(FName Owner)
{
	if (SystemPauseOwners.Remove(Owner) && CanReleaseSystemPause()) { Super::SetPause(false); }
}

FGuid ASovPlayerController::GetActorGUID_Implementation() const
{
	if (!CampaignControllerGuid.IsValid() && !IsTemplate())
	{ const_cast<ASovPlayerController*>(this)->CampaignControllerGuid = FGuid::NewGuid(); }
	return CampaignControllerGuid;
}

void ASovPlayerController::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	if (SavedGUID.IsValid()) { CampaignControllerGuid = SavedGUID; }
}

bool ASovPlayerController::ValidateMissionPawn(USovCampaignDefinition* Mission, FString& OutError, FGameplayTag Lead)
{
	if (!IsValid(Mission) || !Mission->ValidateDefinition(OutError)) { return false; }
	if (!Lead.IsValid()) { Lead = Mission->Protagonist; }
	if (!Mission->SupportsProtagonist(Lead)) { OutError = TEXT("Mission does not support the requested protagonist."); return false; }
	UClass* PawnClass = Mission->ResolvePawnClass(Lead).LoadSynchronous();
	UPlayerDefinition* Definition = Mission->ResolvePlayerDefinition(Lead).LoadSynchronous();
	const ASovPlayerCharacterBase* Defaults = PawnClass ? Cast<ASovPlayerCharacterBase>(PawnClass->GetDefaultObject()) : nullptr;
	if (!Defaults || PawnClass->HasAnyClassFlags(CLASS_Abstract) || !Definition
		|| Defaults->GetProtagonistIdentityTag() != Lead
		|| !Definition->AbilityConfiguration || !Definition->AbilityConfiguration->DefaultAttributes
		|| Definition->DefaultAppearance.IsNull() || !Definition->DefaultAppearance.LoadSynchronous())
	{
		OutError = TEXT("Mission needs a concrete matching protagonist class, player definition, default attributes and appearance.");
		return false;
	}
	return true;
}

bool ASovPlayerController::CanTransitionTo(USovCampaignDefinition* Destination, FString& OutError, bool bRequireDifferentProtagonist) const
{
	if (ApplicationLifecycle && ApplicationLifecycle->IsGameplayInterrupted())
	{ OutError = TEXT("Resume the game before changing mission."); return false; }
	const ASovPlayerCharacterBase* Source = Cast<ASovPlayerCharacterBase>(GetPawn());
	USovCampaignDefinition* Current = CampaignState ? CampaignState->GetActiveMission() : nullptr;
	const UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	const bool bAlive = ASC && ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f
		&& !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_IsDead);
	if (GetNetMode() != NM_Standalone || !SovCampaignPolicy::CanHandoff(HasAuthority(),
		TransitionState != ESovCampaignTransitionState::Idle, Source && Source->IsCharacterReady(), bAlive,
		Current && CampaignState->IsMissionComplete(Current->MissionId),
		Current && IsValid(Destination) && Current->AllowedSuccessorMissions.Contains(Destination->MissionId),
		Source && IsValid(Destination) && (!bRequireDifferentProtagonist || Source->GetProtagonistIdentityTag() != Destination->Protagonist)))
	{
		OutError = TEXT("Handoff requires standalone authority, an idle ready living protagonist, all mandatory beats and an authored successor identity.");
		return false;
	}
	const ASovPlayerState* CampaignPlayer = GetPlayerState<ASovPlayerState>();
	const USovTechniqueComponent* Techniques = CampaignPlayer ? Cast<USovTechniqueComponent>(CampaignPlayer->GetSkillTreeComponent()) : nullptr;
	if (Techniques && Techniques->IsTechniqueMutationInProgress())
	{ OutError = TEXT("Finish the Technique transaction before changing mission."); return false; }
	if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled))
	{ OutError = TEXT("Finish the current action or cinematic before handoff."); return false; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (It->GetEncounterState() == ESovEncounterState::Active || It->GetEncounterState() == ESovEncounterState::Restoring)
		{ OutError = TEXT("An encounter is active or restoring."); return false; }
	}
	return CampaignState->CanEnterMission(Destination) && ValidateMissionPawn(Destination, OutError);
}

ASovPlayerCharacterBase* ASovPlayerController::SpawnCampaignPawn(USovCampaignDefinition* Mission, const FTransform& Transform, FGameplayTag Lead)
{
	if (!GetWorld() || !Mission || Transform.ContainsNaN()) { return nullptr; }
	if (!Lead.IsValid()) { Lead = Mission->Protagonist; }
	UClass* Class = Mission->ResolvePawnClass(Lead).LoadSynchronous();
	UPlayerDefinition* Definition = Mission->ResolvePlayerDefinition(Lead).LoadSynchronous();
	if (!Class || !Definition) { return nullptr; }
	ASovPlayerCharacterBase* Pawn = GetWorld()->SpawnActorDeferred<ASovPlayerCharacterBase>(
		Class, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Pawn) { return nullptr; }
	if (!Pawn->PrepareCampaignInitialization(Definition)) { Pawn->Destroy(); return nullptr; }
	Pawn->FinishSpawning(Transform);
	if (!IsValid(Pawn)) { return nullptr; }
	if (UNarrativeCharacterSubsystem* Characters = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
	{ Characters->RegisterCharacter(Pawn); }
	return Pawn;
}

void ASovPlayerController::SetTransitionInputLock(bool bLock)
{
	if (bOwnInputLock == bLock) { return; }
	bOwnInputLock = bLock;
	SetIgnoreMoveInput(bLock);
	SetIgnoreLookInput(bLock);
	if (UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
	{
		if (bLock) { bWasSavingDisabled = Save->IsSavingDisabled(); Save->SetSavingDisabled(true); }
		else { Save->SetSavingDisabled(bWasSavingDisabled); }
	}
}

void ASovPlayerController::SetTransitionState(ESovCampaignTransitionState State, const FString& Message)
{
	TransitionState = State;
	OnCampaignTransitionChanged.Broadcast(State, Message);
}

bool ASovPlayerController::ClearOutgoingCombatState()
{
	if (HapticFeedback) { HapticFeedback->CancelAllFeedback(); }
	APawn* ExpectedPawn = GetPawn();
	UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!IsValid(ExpectedPawn) || !ASC || ASC->GetAvatarActor() != ExpectedPawn) { return false; }
	const uint64 ExpectedEpoch = TransitionEpoch;
	const auto StillOwnsAvatar = [this, ASC, ExpectedPawn, ExpectedEpoch]()
	{ return TransitionEpoch == ExpectedEpoch && IsValid(ExpectedPawn) && GetPawn() == ExpectedPawn && GetAbilitySystemComponent() == ASC && ASC->GetAvatarActor() == ExpectedPawn; };
	ReleaseHeldAbilityInputs();
	if (!StillOwnsAvatar()) { return false; }
	ASC->CancelAllAbilities();
	if (!StillOwnsAvatar()) { return false; }
	if (ASovPlayerState* PS = GetPlayerState<ASovPlayerState>())
	{ if (USkillTreeComponent* Skills = PS->GetSkillTreeComponent()) { Skills->ClearPurchasedPerksForRestore(); } }
	if (!StillOwnsAvatar()) { return false; }
	ASC->ClearAllAbilities();
	if (!StillOwnsAvatar()) { return false; }
	ASC->ClearTrackedDefaultAttributesEffect();
	if (!StillOwnsAvatar()) { return false; }
	ASC->ClearTrackedStartupEffects();
	if (!StillOwnsAvatar()) { return false; }
	ASC->RemoveActiveEffects(FGameplayEffectQuery());
	if (!StillOwnsAvatar()) { return false; }
	ASC->SetDefinitionOwnedTags(FGameplayTagContainer());
	if (!StillOwnsAvatar()) { return false; }
	ASC->ClearActorInfo();
	return true;
}

bool ASovPlayerController::HandoffToMission(USovCampaignDefinition* Destination, const FTransform& SpawnTransform, FString& OutError)
{
	OutError.Reset();
	if (!CanTransitionTo(Destination, OutError)) { return false; }
	if (!PrepareTransitionCheckpoint(Destination->MissionId, OutError)) { return false; }
	return StartPawnHandoff(Destination, Destination->Protagonist, SpawnTransform, NAME_None, FGuid(), OutError);
}

bool ASovPlayerController::PrepareTransitionCheckpoint(FName BoundaryId, FString& OutError)
{
	USovSaveSubsystem* Slots = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
	APawn* Source = GetPawn(); const uint64 ExpectedEpoch = TransitionEpoch;
	if (!Slots || (!Slots->ConsumeAcknowledgedBoundary(ESovSaveBoundary::LongTransition, BoundaryId)
		&& Slots->WriteCheckpoint(ESovSaveBoundary::LongTransition, BoundaryId, OutError) != ESovSaveResult::Success))
	{ if (OutError.IsEmpty()) { OutError = TEXT("A safe transition checkpoint could not be written."); } return false; }
	if (TransitionEpoch != ExpectedEpoch || GetPawn() != Source || TransitionState != ESovCampaignTransitionState::Idle)
	{ OutError = TEXT("Campaign ownership changed during the transition checkpoint."); return false; }
	return true;
}

bool ASovPlayerController::StartPawnHandoff(USovCampaignDefinition* Destination, FGameplayTag Lead, const FTransform& SpawnTransform, FName HandoffBeat, const FGuid& HandoffRequest, FString& OutError)
{
	ASovPlayerState* PS = GetPlayerState<ASovPlayerState>();
	ASovPlayerCharacterBase* Source = Cast<ASovPlayerCharacterBase>(GetPawn());
	UNarrativeAbilitySystemComponent* SourceASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!PS || !Save || !Source || !SourceASC) { OutError = TEXT("Campaign ownership is unavailable."); return false; }
	// Save callbacks and deferred actor BeginPlay are external code boundaries.
	// Latch before either can recursively start another handoff.
	const uint64 ExpectedEpoch = ++TransitionEpoch;
	TransitionState = ESovCampaignTransitionState::Switching;
	const auto StillOwnSource = [this, Source, SourceASC, ExpectedEpoch]()
	{
		return IsValid(this) && TransitionEpoch == ExpectedEpoch && IsValid(Source)
			&& GetPawn() == Source && SourceASC == GetAbilitySystemComponent()
			&& SourceASC->GetAvatarActor() == Source && Source->IsCharacterReady();
	};
	FSovProtagonistSnapshot Captured;
	if (!PS->CaptureProtagonistSnapshot(Source, Captured, OutError) || !StillOwnSource()
		|| !Save->CreateActorRecord(this, OriginControllerRecord) || !StillOwnSource())
	{
		if (TransitionEpoch == ExpectedEpoch) { TransitionState = ESovCampaignTransitionState::Idle; }
		if (OutError.IsEmpty()) { OutError = TEXT("Source changed during handoff preparation."); }
		return false;
	}
	ASovPlayerCharacterBase* Spawned = SpawnCampaignPawn(Destination, SpawnTransform, Lead);
	if (!Spawned || !StillOwnSource())
	{
		if (IsValid(Spawned) && GetPawn() != Spawned) { Spawned->Destroy(); }
		if (TransitionEpoch == ExpectedEpoch) { TransitionState = ESovCampaignTransitionState::Idle; }
		OutError = TEXT("Destination could not spawn or source changed; no combat state was replaced.");
		return false;
	}
	if (HandoffRequest.IsValid() && (!ConvergenceCompanionState->StageHandoff(Destination, Lead, Source, OutError) || !StillOwnSource()))
	{
		ConvergenceCompanionState->RollbackStaged(); Spawned->Destroy();
		if (TransitionEpoch == ExpectedEpoch) { TransitionState = ESovCampaignTransitionState::Idle; }
		return false;
	}
	OriginSnapshot = Captured;
	bHasOriginSnapshot = PS->StoreProtagonistSnapshot(Captured);
	if (!bHasOriginSnapshot)
	{
		ConvergenceCompanionState->RollbackStaged();
		Spawned->Destroy();
		TransitionState = ESovCampaignTransitionState::Idle;
		OutError = TEXT("Origin snapshot could not be retained; handoff was not started.");
		return false;
	}
	if (!HandoffRequest.IsValid() && !ConvergenceCompanionState->StageInitialCompanion(Destination, Lead, OutError))
	{
		ConvergenceCompanionState->RollbackStaged(); Spawned->Destroy(); bHasOriginSnapshot = false;
		TransitionState = ESovCampaignTransitionState::Idle; return false;
	}
	OriginMission = CampaignState->GetActiveMission();
	PendingMission = Destination;
	PendingProtagonist = Lead;
	PendingHandoffBeat = HandoffBeat; PendingHandoffRequest = HandoffRequest;
	PendingPawn = Spawned;
	bHasPendingRecords = false;
	bFromLevelTravel = true;
	SetTransitionInputLock(true);
	const bool bCleared = ClearOutgoingCombatState();
	if (!bCleared || TransitionEpoch != ExpectedEpoch || GetPawn() != Source || SourceASC != GetAbilitySystemComponent())
	{
		ConvergenceCompanionState->RollbackStaged();
		// Cancellation callbacks changed ownership: do not touch the replacement pawn.
		if (IsValid(Spawned) && GetPawn() != Spawned) { Spawned->Destroy(); }
		bHasOriginSnapshot = false;
		SetTransitionState(ESovCampaignTransitionState::Failed, TEXT("Possession changed during combat teardown."));
		OutError = TEXT("Possession changed during combat teardown.");
		return false;
	}
	UnPossess();
	SetOwnedCharacter(nullptr);
	Source->Destroy(); // A surviving old pawn can rebind the shared ASC through CachedController.
	Possess(Spawned);
	InitializeCampaignPawn(Spawned);
	return true;
}

bool ASovPlayerController::StageCampaignLoad(USovCampaignDefinition* Mission, const FNarrativeSavePlayer* Records,
	bool bFromTravel, FString& OutError)
{
	if (!HasAuthority() || GetNetMode() != NM_Standalone || !IsValid(Mission)) { return false; }
	PendingProtagonist = Mission->Protagonist;
	PendingHandoffBeat = NAME_None; PendingHandoffRequest.Invalidate();
	if (Records && Records->IsValid() && !bFromTravel)
	{
		const auto* StateRecord = Records->ControllerData.SavedComponents.FindByPredicate([this](const FNarrativeSaveComponent& Record)
		{ return Record.ComponentName == CampaignState->GetFName(); });
		if (!StateRecord || !USovCampaignStateComponent::GetSerializedActiveProtagonist(StateRecord->ByteData, PendingProtagonist, OutError)) { return false; }
	}
	if (!ValidateMissionPawn(Mission, OutError, PendingProtagonist)) { return false; }
	PendingMission = Mission;
	bFromLevelTravel = bFromTravel;
	bHasPendingRecords = Records && Records->IsValid();
	if (bHasPendingRecords)
	{
		PendingRecords = *Records;
		ASovPlayerState* PS = GetPlayerState<ASovPlayerState>();
		UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
		if (!PS || !Save) { OutError = TEXT("Campaign save ownership is unavailable."); return false; }
		FNarrativeActorRecord ActorOnly = Records->PlayerStateData;
		ActorOnly.SavedComponents.Reset(); // Never load source ASC or source skills onto a different protagonist.
		ActorOnly.Transform = FTransform::Identity;
		ActorOnly.bHasTransform = false;
		if (!Save->LoadActorFromRecord(PS, ActorOnly))
		{ OutError = TEXT("Campaign PlayerState record could not be deserialized."); return false; }
	}
	if (bHasPendingRecords && !bFromTravel)
	{
		const auto* CompanionRecord = Records->ControllerData.SavedComponents.FindByPredicate([this](const FNarrativeSaveComponent& Item)
		{ return Item.ComponentName == ConvergenceCompanionState->GetFName(); });
		if (CompanionRecord)
		{
			if (!ConvergenceCompanionState->StageSavedRecord(CompanionRecord->ByteData, Mission, PendingProtagonist, OutError)) { return false; }
		}
		else if (!ConvergenceCompanionState->StageInitialCompanion(Mission, PendingProtagonist, OutError)) { return false; }
	}
	else if (!ConvergenceCompanionState->StageInitialCompanion(Mission, PendingProtagonist, OutError)) { return false; }
	return true;
}

void ASovPlayerController::InitializeCampaignPawn(ASovPlayerCharacterBase* Pawn)
{
	if (!HasAuthority() || !IsValid(Pawn) || !PendingMission) { return; }
	PendingPawn = Pawn;
	if (TransitionState == ESovCampaignTransitionState::Idle)
	{ ++TransitionEpoch; TransitionState = ESovCampaignTransitionState::Initializing; }
	SetTransitionInputLock(true);
	InitializationDeadline = GetWorld()->GetTimeSeconds() + FMath::Clamp(InitializationTimeoutSeconds, 1.f, 120.f);
	const uint64 ExpectedEpoch = TransitionEpoch;
	GetWorldTimerManager().SetTimer(InitializationTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, ExpectedEpoch]() { PollCampaignInitialization(ExpectedEpoch); }), .05f, true);
	OnCampaignTransitionChanged.Broadcast(TransitionState, FString());
}

void ASovPlayerController::PollCampaignInitialization(uint64 ExpectedEpoch)
{
	if (!IsValid(this) || IsActorBeingDestroyed() || TransitionEpoch != ExpectedEpoch) { return; }
	if (!SovCampaignPolicy::MayCommitAsync(TransitionEpoch, ExpectedEpoch, IsValid(PendingPawn), GetPawn() == PendingPawn))
	{
		if (GetWorld()->GetTimeSeconds() >= InitializationDeadline) { FailCampaignInitialization(TEXT("Campaign possession did not complete.")); }
		return;
	}
	if (!PendingPawn->IsCampaignDataReadyToApply())
	{
		if (GetWorld()->GetTimeSeconds() >= InitializationDeadline) { FailCampaignInitialization(TEXT("Campaign character readiness timed out.")); }
		return;
	}
	ASovPlayerCharacterBase* const PollPawn = PendingPawn;
	USovCampaignDefinition* const PollMission = PendingMission;
	FString CompanionError;
	if (!ConvergenceCompanionState->PollStaged(CompanionError))
	{
		if (TransitionEpoch != ExpectedEpoch || PendingPawn != PollPawn || PendingMission != PollMission || GetPawn() != PollPawn) { return; }
		if (!CompanionError.IsEmpty() || GetWorld()->GetTimeSeconds() >= InitializationDeadline)
		{ FailCampaignInitialization(CompanionError.IsEmpty() ? TEXT("The protagonist companion initialization timed out.") : CompanionError); }
		return;
	}
	if (TransitionEpoch != ExpectedEpoch || PendingPawn != PollPawn || PendingMission != PollMission || !IsValid(PollPawn) || GetPawn() != PollPawn) { return; }
	GetWorldTimerManager().ClearTimer(InitializationTimer);
	ASovPlayerCharacterBase* const RestoringPawn = PendingPawn;
	USovCampaignDefinition* const RestoringMission = PendingMission;
	UNarrativeAbilitySystemComponent* const RestoringASC = RestoringPawn->GetNarrativeAbilitySystemComponent();
	ASovPlayerState* const RestoringPS = GetPlayerState<ASovPlayerState>();
	const auto StillRestoring = [this, ExpectedEpoch, RestoringPawn, RestoringMission, RestoringASC, RestoringPS]()
	{
		return IsValid(this) && !IsActorBeingDestroyed() && TransitionEpoch == ExpectedEpoch && IsValid(RestoringPawn) && PendingPawn == RestoringPawn
			&& GetPawn() == RestoringPawn && PendingMission == RestoringMission
			&& IsValid(RestoringPS) && GetPlayerState<ASovPlayerState>() == RestoringPS && RestoringPawn->GetPlayerState<ASovPlayerState>() == RestoringPS
			&& IsValid(RestoringASC) && GetAbilitySystemComponent() == RestoringASC
			&& RestoringASC->GetAvatarActor() == RestoringPawn;
	};
	const auto FailCurrentRestore = [this, ExpectedEpoch, RestoringPawn, RestoringMission, RestoringPS, RestoringASC](const FString& Reason)
	{
		// An old callback may report an error after possession or a new transition took ownership.
		if (IsValid(this) && !IsActorBeingDestroyed() && TransitionEpoch == ExpectedEpoch && PendingPawn == RestoringPawn
			&& PendingMission == RestoringMission && GetPawn() == RestoringPawn && GetPlayerState<ASovPlayerState>() == RestoringPS
			&& (!GetAbilitySystemComponent() || GetAbilitySystemComponent() == RestoringASC)
			&& (!IsValid(RestoringASC) || !RestoringASC->GetAvatarActor() || RestoringASC->GetAvatarActor() == RestoringPawn)) { FailCampaignInitialization(Reason); }
	};
	ASovPlayerState* PS = RestoringPS;
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	FSovProtagonistSnapshot Snapshot;
	FString Error;
	if (!PS || !Save) { FailCurrentRestore(TEXT("Campaign save ownership disappeared.")); return; }
	const bool bHasSnapshot = PS->FindProtagonistSnapshot(GetPendingProtagonist(), Snapshot);
	if (bHasSnapshot)
	{
		if (!PS->RestoreProtagonistSnapshot(PendingPawn, Snapshot, !bFromLevelTravel, Error))
		{ FailCurrentRestore(Error); return; }
	}
	else
	{
		// A full save for the same pawn must have the new schema; silently resetting it loses progress.
		if (bHasPendingRecords && !bFromLevelTravel)
		{ FailCurrentRestore(TEXT("This save lacks a matching protagonist snapshot; migrate it before campaign loading.")); return; }
		if (USovTechniqueComponent* Techniques = Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent()))
		{
			if (!Techniques->InitializeNewProtagonist(RestoringPawn->GetProtagonistIdentityTag()))
			{ FailCurrentRestore(TEXT("Technique profile could not initialize.")); return; }
		}
		else { FailCurrentRestore(TEXT("Campaign PlayerState requires SovTechniqueComponent.")); return; }
		if (!StillRestoring()) { FailCurrentRestore(TEXT("Ownership changed during Technique initialization.")); return; }
		PS->SetCampaignFactions(RestoringPawn->GetPlayerDefinition()->DefaultFactions);
		if (!StillRestoring()) { FailCurrentRestore(TEXT("Ownership changed during faction initialization.")); return; }
		RestoringPawn->InitNewCharacter(RestoringPawn->GetPlayerDefinition());
		if (!StillRestoring()) { FailCurrentRestore(TEXT("Ownership changed during initial equipment grant.")); return; }
		if (UNarrativeAbilitySystemComponent* ASC = RestoringASC)
		{
			ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), FMath::Clamp(
				RestoringMission->EntryEchoReserve, 0.f, ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxEchoAttribute())));
		}
	}
	if (!StillRestoring()) { FailCurrentRestore(TEXT("Ownership changed during protagonist restore.")); return; }
	const USovTechniqueComponent* Techniques = Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent());
	if (!Techniques || !Techniques->IsTechniqueStateValid())
	{ FailCurrentRestore(TEXT("Technique snapshot failed validation.")); return; }
	if (bHasPendingRecords)
	{
		const FRotator DestinationRotation = GetControlRotation();
		const FNarrativeActorRecord ControllerRecord = PendingRecords.ControllerData;
		if (!Save->LoadActorFromRecord(this, ControllerRecord))
		{ FailCurrentRestore(TEXT("Campaign controller record could not be deserialized.")); return; }
		if (!StillRestoring()) { FailCurrentRestore(TEXT("Ownership changed during quest restore.")); return; }
		if (bFromLevelTravel) { SetControlRotation(DestinationRotation); }
		if (bFromLevelTravel && PendingTravelMission != PendingMission)
		{ FailCurrentRestore(TEXT("Travel record does not name this map's mission.")); return; }
	}
	const ESovCampaignResult Started = PendingHandoffRequest.IsValid()
		? CampaignState->CompleteAuthoredHandoff(PendingHandoffBeat, PendingHandoffRequest)
		: CampaignState->BeginMission(PendingMission);
	if (Started != ESovCampaignResult::Applied && Started != ESovCampaignResult::AlreadyApplied)
	{ FailCurrentRestore(TEXT("Restored campaign state rejected the destination mission.")); return; }
	if (!StillRestoring()) { FailCurrentRestore(TEXT("Ownership changed during mission commit.")); return; }
	if (USovCorruptionComponent* Corruption = RestoringPawn->GetCorruptionComponent())
	{
		if (!Corruption->FinishCampaignRestore(RestoringMission, Error) || !StillRestoring())
		{ FailCurrentRestore(Error.IsEmpty() ? TEXT("Corruption restore lost its campaign context.") : Error); return; }
	}
	if (!StillRestoring() || !RestoringPawn->CompleteCampaignDataInitialization(false) || !StillRestoring())
	{ FailCurrentRestore(TEXT("Campaign readiness was invalidated while restoring.")); return; }
	if (!ConvergenceCompanionState->CommitStaged(RestoringPawn, Error) || !StillRestoring())
	{ FailCurrentRestore(Error.IsEmpty() ? TEXT("The protagonist companion could not commit.") : Error); return; }
	PendingTravelMission = nullptr;
	PendingPawn = nullptr;
	PendingMission = nullptr;
	PendingProtagonist = FGameplayTag(); PendingHandoffBeat = NAME_None; PendingHandoffRequest.Invalidate();
	PendingRecords = FNarrativeSavePlayer();
	bHasPendingRecords = false;
	bHasOriginSnapshot = false;
	OriginSnapshot = FSovProtagonistSnapshot();
	OriginControllerRecord = FNarrativeActorRecord();
	OriginMission = nullptr;
	const ESovCampaignTransitionState CompletingState = TransitionState;
	const auto StillCompleting = [this, ExpectedEpoch, RestoringPawn, RestoringMission, RestoringASC, PS](ESovCampaignTransitionState ExpectedState)
	{
		return IsValid(this) && !IsActorBeingDestroyed() && TransitionEpoch == ExpectedEpoch && TransitionState == ExpectedState
			&& !PendingPawn && !PendingMission && IsValid(RestoringPawn) && RestoringPawn->IsCharacterReady()
			&& GetPawn() == RestoringPawn && RestoringPawn->GetController() == this && GetPlayerState<ASovPlayerState>() == PS
			&& IsValid(RestoringASC) && GetAbilitySystemComponent() == RestoringASC && RestoringASC->GetAvatarActor() == RestoringPawn
			&& CampaignState && CampaignState->GetActiveMission() == RestoringMission;
	};
	SetTransitionInputLock(false);
	if (!StillCompleting(CompletingState)) { return; }
	RefreshGameplayReadiness();
	if (!StillCompleting(CompletingState)) { return; }
	if (HapticFeedback) { HapticFeedback->RefreshSources(); }
	if (!StillCompleting(CompletingState)) { return; }
	SetTransitionState(ESovCampaignTransitionState::Idle);
	if (!StillCompleting(ESovCampaignTransitionState::Idle)) { return; }
	if (USovSaveSubsystem* Slots = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr)
	{
		Slots->NotifyCampaignReady(this, true);
		if (!StillCompleting(ESovCampaignTransitionState::Idle)) { return; }
		if (Started == ESovCampaignResult::Applied)
		{ Slots->QueueAutosave(ESovSaveBoundary::MissionStart, RestoringMission->MissionId); }
	}
	if (!StillCompleting(ESovCampaignTransitionState::Idle)) { return; }
	if (USovGameUserSettings* Settings = USovGameUserSettings::Get()) { Settings->UnlockSovereignFromCampaign(CampaignState); }
}

void ASovPlayerController::FailCampaignInitialization(const FString& Message)
{
	if (!IsValid(this) || IsActorBeingDestroyed() || bFailureInProgress) { return; }
	TGuardValue<bool> FailureGuard(bFailureInProgress, true);
	const uint64 FailureEpoch = ++TransitionEpoch;
	ASovPlayerCharacterBase* ExpectedPending = PendingPawn;
	USovCampaignDefinition* ExpectedMission = PendingMission;
	APawn* ExpectedPossession = GetPawn();
	ASovPlayerState* const ExpectedPS = GetPlayerState<ASovPlayerState>();
	const auto OwnsFailure = [this, FailureEpoch, &ExpectedPending, &ExpectedMission, &ExpectedPossession, ExpectedPS]()
	{
		return IsValid(this) && !IsActorBeingDestroyed() && TransitionEpoch == FailureEpoch
			&& PendingPawn == ExpectedPending && PendingMission == ExpectedMission && GetPawn() == ExpectedPossession
			&& GetPlayerState<ASovPlayerState>() == ExpectedPS;
	};
	const auto PublishFailure = [this, &OwnsFailure, &ExpectedPending](const FString& Reason)
	{
		if (!OwnsFailure()) { return; }
		if (IsValid(ExpectedPending)) { ExpectedPending->FailCampaignInitialization(); }
		if (!OwnsFailure()) { return; }
		SetTransitionState(ESovCampaignTransitionState::Failed, Reason);
		if (!OwnsFailure() || TransitionState != ESovCampaignTransitionState::Failed) { return; }
		if (USovSaveSubsystem* Slots = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr)
		{ Slots->NotifyCampaignReady(this, false); }
	};
	ConvergenceCompanionState->RollbackStaged();
	if (!OwnsFailure()) { return; }
	GetWorldTimerManager().ClearTimer(InitializationTimer);
	UE_LOG(LogTemp, Error, TEXT("Campaign initialization failed: %s"), *Message);
	if (bHasOriginSnapshot && TransitionState != ESovCampaignTransitionState::Recovering && IsValid(OriginMission)
		&& GetPawn() == PendingPawn && IsValid(PendingPawn))
	{
		const FSovProtagonistSnapshot RecoverySnapshot = OriginSnapshot;
		const FNarrativeActorRecord RecoveryController = OriginControllerRecord;
		USovCampaignDefinition* const RecoveryMission = OriginMission;
		if (!ClearOutgoingCombatState()) { PublishFailure(TEXT("Recovery ownership changed during teardown.")); return; }
		if (!OwnsFailure()) { return; }
		APawn* const FailedPawn = PendingPawn;
		ExpectedPossession = nullptr; UnPossess();
		if (!OwnsFailure()) { return; }
		SetOwnedCharacter(nullptr);
		if (!OwnsFailure()) { return; }
		if (IsValid(FailedPawn)) { FailedPawn->Destroy(); }
		if (!OwnsFailure()) { return; }
		PendingMission = RecoveryMission; ExpectedMission = RecoveryMission;
		PendingProtagonist = RecoverySnapshot.ProtagonistTag; PendingHandoffBeat = NAME_None; PendingHandoffRequest.Invalidate();
		ASovPlayerCharacterBase* const Spawned = SpawnCampaignPawn(RecoveryMission, RecoverySnapshot.PawnRecord.Transform, PendingProtagonist);
		if (!OwnsFailure()) { if (IsValid(Spawned) && GetPawn() != Spawned) { Spawned->Destroy(); } return; }
		PendingPawn = Spawned; ExpectedPending = Spawned;
		if (IsValid(Spawned))
		{
			TransitionState = ESovCampaignTransitionState::Recovering;
			const auto* CompanionRecord = RecoveryController.SavedComponents.FindByPredicate([this](const FNarrativeSaveComponent& Item)
			{ return Item.ComponentName == ConvergenceCompanionState->GetFName(); });
			FString CompanionError;
			const bool bStagedCompanion = CompanionRecord
				? ConvergenceCompanionState->StageSavedRecord(CompanionRecord->ByteData, RecoveryMission, PendingProtagonist, CompanionError)
				: ConvergenceCompanionState->StageInitialCompanion(RecoveryMission, PendingProtagonist, CompanionError);
			if (!OwnsFailure()) { return; }
			if (!bStagedCompanion)
			{
				ConvergenceCompanionState->RollbackStaged();
				if (!OwnsFailure()) { return; }
				Spawned->Destroy();
				if (!OwnsFailure()) { return; }
				PendingPawn = nullptr; ExpectedPending = nullptr;
				PublishFailure(CompanionError); return;
			}
			PendingRecords = FNarrativeSavePlayer(); PendingRecords.ControllerData = RecoveryController;
			bHasPendingRecords = true; bFromLevelTravel = false;
			ExpectedPossession = Spawned; Possess(Spawned);
			if (!OwnsFailure()) { return; }
			InitializeCampaignPawn(Spawned);
			return;
		}
	}
	// The failed operation cannot announce success or mutate a replacement transition from its callbacks.
	PublishFailure(Message);
}

bool ASovPlayerController::TravelToMission(USovCampaignDefinition* Destination, FString& OutError)
{
	OutError.Reset();
	if (!CanTransitionTo(Destination, OutError, false)) { return false; }
	const ASovCampaignGameMode* CampaignMode = GetWorld()->GetAuthGameMode<ASovCampaignGameMode>();
	if (!CampaignMode || CampaignMode->bUseSeamlessTravel)
	{ OutError = TEXT("Campaign travel requires SovCampaignGameMode with seamless travel disabled."); return false; }
	const FString MapPackage = Destination->Map.ToSoftObjectPath().GetLongPackageName();
	if (MapPackage.IsEmpty() || !FPackageName::DoesPackageExist(MapPackage))
	{ OutError = TEXT("Destination map is missing or not cooked."); return false; }
	if (!PrepareTransitionCheckpoint(Destination->MissionId, OutError)) { return false; }
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!Save) { OutError = TEXT("Narrative save subsystem is unavailable."); return false; }
	USovSaveSubsystem* Slots = GetGameInstance() ? GetGameInstance()->GetSubsystem<USovSaveSubsystem>() : nullptr;
	if (!Slots || !Slots->IsPlatformStorageOwnerAvailable() || Slots->IsPlatformStorageSuspended())
	{ OutError = TEXT("Reconnect the campaign's storage owner before travelling."); return false; }
	const FString TravelOwner = Slots->GetAccountNamespace();
	const int32 TravelUser = Slots->GetLocalSaveUserIndex();
	const FString OwnedTravelSlot = FString(TravelSaveSlot()) + TEXT("_") + TravelOwner;
	const uint64 ExpectedEpoch = ++TransitionEpoch;
	APawn* Source = GetPawn();
	TransitionState = ESovCampaignTransitionState::Travelling;
	PendingTravelMission = Destination;
	ASovPlayerState* PS = GetPlayerState<ASovPlayerState>();
	FSovProtagonistSnapshot Snapshot;
	if (!PS || !PS->CaptureProtagonistSnapshot(Cast<ASovPlayerCharacterBase>(Source), Snapshot, OutError)
		|| !PS->StoreProtagonistSnapshot(Snapshot) || TransitionEpoch != ExpectedEpoch || GetPawn() != Source)
	{
		if (TransitionEpoch == ExpectedEpoch)
		{ PendingTravelMission = nullptr; TransitionState = ESovCampaignTransitionState::Idle; }
		if (OutError.IsEmpty()) { OutError = TEXT("Current protagonist could not be retained for travel."); }
		return false;
	}
	const TWeakObjectPtr<USovSaveSubsystem> TravelStorage(Slots);
	const auto OwnsTravelStorage = [TravelStorage, TravelOwner, TravelUser]()
	{
		const auto* Current = TravelStorage.Get();
		return Current && Current->IsPlatformStorageOwnerAvailable() && !Current->IsPlatformStorageSuspended() && Current->GetAccountNamespace() == TravelOwner
			&& Current->GetLocalSaveUserIndex() == TravelUser;
	};
	const bool bSaved = OwnsTravelStorage() && Save->CreatePlayerOnlySaveInSlot(this, OwnedTravelSlot, TravelUser, OwnsTravelStorage);
	if (!bSaved || !OwnsTravelStorage() || TransitionEpoch != ExpectedEpoch || GetPawn() != Source || PendingTravelMission != Destination)
	{
		if (TransitionEpoch == ExpectedEpoch)
		{ PendingTravelMission = nullptr; TransitionState = ESovCampaignTransitionState::Idle; }
		OutError = TEXT("Travel save failed or ownership changed; no map travel was requested.");
		return false;
	}
	SetTransitionInputLock(true);
	// Our record is already committed; do not invoke Narrative's display-name LevelTransition slot path.
	if (!GetWorld()->ServerTravel(MapPackage + TEXT("?SovCampaignTransition=1"), true))
	{
		PendingTravelMission = nullptr;
		SetTransitionInputLock(false);
		SetTransitionState(ESovCampaignTransitionState::Idle);
		OutError = TEXT("Unreal rejected the destination travel request.");
		return false;
	}
	OnCampaignTransitionChanged.Broadcast(TransitionState, FString());
	return true;
}

void ASovPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HapticFeedback) { HapticFeedback->CancelAllFeedback(); }
	++TransitionEpoch;
	GetWorldTimerManager().ClearTimer(InitializationTimer);
	Super::EndPlay(EndPlayReason);
}

FGameplayTag ASovPlayerController::GetPendingProtagonist() const
{
	return PendingProtagonist.IsValid() ? PendingProtagonist : (PendingMission ? PendingMission->Protagonist : FGameplayTag());
}

bool ASovPlayerController::RequestAuthoredHandoff(ASovCampaignHandoffAnchor* Anchor, FString& OutError)
{
	FTransform Destination;
	if (!HasAuthority() || GetNetMode() != NM_Standalone || TransitionState != ESovCampaignTransitionState::Idle
		|| !IsValid(Anchor) || !Anchor->ValidateRequest(this, Destination, OutError)) { return false; }
	USovCampaignDefinition* Mission = CampaignState->GetActiveMission();
	const auto* Beat = Mission->FindBeat(Anchor->HandoffBeat);
	UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	ASovPlayerCharacterBase* Player = Cast<ASovPlayerCharacterBase>(GetPawn());
	if (!ASC || !Player || ASC->GetAvatarActor() != Player
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Busy)
		|| ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled))
	{ OutError = TEXT("Finish the active action before the authored handoff."); return false; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (It->GetEncounterState() == ESovEncounterState::Active || It->GetEncounterState() == ESovEncounterState::Restoring)
		{ OutError = TEXT("Authored combat handoffs require a completed encounter segment."); return false; }
	}
	if (!ValidateMissionPawn(Mission, OutError, Beat->HandoffToProtagonist)) { return false; }
	if (!PrepareTransitionCheckpoint(Beat->BeatId, OutError)) { return false; }
	return StartPawnHandoff(Mission, Beat->HandoffToProtagonist, Destination, Beat->BeatId, FGuid::NewGuid(), OutError);
}
