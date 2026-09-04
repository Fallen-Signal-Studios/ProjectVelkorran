// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Framework/SovPlayerController.h"

#include "AI/NarrativeCharacterSubsystem.h"
#include "Campaign/SovCampaignDefinition.h"
#include "Campaign/SovCampaignPolicy.h"
#include "Campaign/SovCampaignStateComponent.h"
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

bool ASovPlayerController::ValidateMissionPawn(USovCampaignDefinition* Mission, FString& OutError)
{
	if (!IsValid(Mission) || !Mission->ValidateDefinition(OutError)) { return false; }
	UClass* PawnClass = Mission->PawnClass.LoadSynchronous();
	UPlayerDefinition* Definition = Mission->PlayerDefinition.LoadSynchronous();
	const ASovPlayerCharacterBase* Defaults = PawnClass ? Cast<ASovPlayerCharacterBase>(PawnClass->GetDefaultObject()) : nullptr;
	if (!Defaults || PawnClass->HasAnyClassFlags(CLASS_Abstract) || !Definition
		|| Defaults->GetProtagonistIdentityTag() != Mission->Protagonist
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

ASovPlayerCharacterBase* ASovPlayerController::SpawnCampaignPawn(USovCampaignDefinition* Mission, const FTransform& Transform)
{
	if (!GetWorld() || !Mission || Transform.ContainsNaN()) { return nullptr; }
	ASovPlayerCharacterBase* Pawn = GetWorld()->SpawnActorDeferred<ASovPlayerCharacterBase>(
		Mission->PawnClass.Get(), Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding);
	if (!Pawn) { return nullptr; }
	if (!Pawn->PrepareCampaignInitialization(Mission->PlayerDefinition.Get())) { Pawn->Destroy(); return nullptr; }
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
	ASovPlayerCharacterBase* Spawned = SpawnCampaignPawn(Destination, SpawnTransform);
	if (!Spawned || !StillOwnSource())
	{
		if (IsValid(Spawned) && GetPawn() != Spawned) { Spawned->Destroy(); }
		if (TransitionEpoch == ExpectedEpoch) { TransitionState = ESovCampaignTransitionState::Idle; }
		OutError = TEXT("Destination could not spawn or source changed; no combat state was replaced.");
		return false;
	}
	OriginSnapshot = Captured;
	bHasOriginSnapshot = PS->StoreProtagonistSnapshot(Captured);
	if (!bHasOriginSnapshot)
	{
		Spawned->Destroy();
		TransitionState = ESovCampaignTransitionState::Idle;
		OutError = TEXT("Origin snapshot could not be retained; handoff was not started.");
		return false;
	}
	OriginMission = CampaignState->GetActiveMission();
	PendingMission = Destination;
	PendingPawn = Spawned;
	bHasPendingRecords = false;
	bFromLevelTravel = true;
	SetTransitionInputLock(true);
	const bool bCleared = ClearOutgoingCombatState();
	if (!bCleared || TransitionEpoch != ExpectedEpoch || GetPawn() != Source || SourceASC != GetAbilitySystemComponent())
	{
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
	if (!HasAuthority() || GetNetMode() != NM_Standalone || !ValidateMissionPawn(Mission, OutError)) { return false; }
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
		if (!Save->LoadActorFromRecord(PS, ActorOnly))
		{ OutError = TEXT("Campaign PlayerState record could not be deserialized."); return false; }
	}
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
	if (TransitionEpoch != ExpectedEpoch) { return; }
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
	GetWorldTimerManager().ClearTimer(InitializationTimer);
	ASovPlayerCharacterBase* const RestoringPawn = PendingPawn;
	USovCampaignDefinition* const RestoringMission = PendingMission;
	UNarrativeAbilitySystemComponent* const RestoringASC = RestoringPawn->GetNarrativeAbilitySystemComponent();
	const auto StillRestoring = [this, ExpectedEpoch, RestoringPawn, RestoringMission, RestoringASC]()
	{
		return TransitionEpoch == ExpectedEpoch && IsValid(RestoringPawn) && PendingPawn == RestoringPawn
			&& GetPawn() == RestoringPawn && PendingMission == RestoringMission
			&& IsValid(RestoringASC) && GetAbilitySystemComponent() == RestoringASC
			&& RestoringASC->GetAvatarActor() == RestoringPawn;
	};
	ASovPlayerState* PS = GetPlayerState<ASovPlayerState>();
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	FSovProtagonistSnapshot Snapshot;
	FString Error;
	if (!PS || !Save) { FailCampaignInitialization(TEXT("Campaign save ownership disappeared.")); return; }
	const bool bHasSnapshot = PS->FindProtagonistSnapshot(PendingMission->Protagonist, Snapshot);
	if (bHasSnapshot)
	{
		if (!PS->RestoreProtagonistSnapshot(PendingPawn, Snapshot, !bFromLevelTravel, Error))
		{ FailCampaignInitialization(Error); return; }
	}
	else
	{
		// A full save for the same pawn must have the new schema; silently resetting it loses progress.
		if (bHasPendingRecords && !bFromLevelTravel)
		{ FailCampaignInitialization(TEXT("This save lacks a matching protagonist snapshot; migrate it before campaign loading.")); return; }
		if (USovTechniqueComponent* Techniques = Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent()))
		{
			if (!Techniques->InitializeNewProtagonist(RestoringMission->Protagonist))
			{ FailCampaignInitialization(TEXT("Technique profile could not initialize.")); return; }
		}
		else { FailCampaignInitialization(TEXT("Campaign PlayerState requires SovTechniqueComponent.")); return; }
		if (!StillRestoring()) { FailCampaignInitialization(TEXT("Ownership changed during Technique initialization.")); return; }
		PS->SetCampaignFactions(RestoringPawn->GetPlayerDefinition()->DefaultFactions);
		if (!StillRestoring()) { FailCampaignInitialization(TEXT("Ownership changed during faction initialization.")); return; }
		RestoringPawn->InitNewCharacter(RestoringPawn->GetPlayerDefinition());
		if (!StillRestoring()) { FailCampaignInitialization(TEXT("Ownership changed during initial equipment grant.")); return; }
		if (UNarrativeAbilitySystemComponent* ASC = RestoringASC)
		{
			ASC->SetNumericAttributeBase(UNarrativeAttributeSetBase::GetEchoAttribute(), FMath::Clamp(
				RestoringMission->EntryEchoReserve, 0.f, ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetMaxEchoAttribute())));
		}
	}
	if (!StillRestoring()) { FailCampaignInitialization(TEXT("Ownership changed during protagonist restore.")); return; }
	const USovTechniqueComponent* Techniques = Cast<USovTechniqueComponent>(PS->GetSkillTreeComponent());
	if (!Techniques || !Techniques->IsTechniqueStateValid())
	{ FailCampaignInitialization(TEXT("Technique snapshot failed validation.")); return; }
	if (bHasPendingRecords)
	{
		const FRotator DestinationRotation = GetControlRotation();
		if (!Save->LoadActorFromRecord(this, PendingRecords.ControllerData))
		{ FailCampaignInitialization(TEXT("Campaign controller record could not be deserialized.")); return; }
		if (!StillRestoring()) { FailCampaignInitialization(TEXT("Ownership changed during quest restore.")); return; }
		if (bFromLevelTravel) { SetControlRotation(DestinationRotation); }
		if (bFromLevelTravel && PendingTravelMission != PendingMission)
		{ FailCampaignInitialization(TEXT("Travel record does not name this map's mission.")); return; }
	}
	const ESovCampaignResult Started = CampaignState->BeginMission(PendingMission);
	if (Started != ESovCampaignResult::Applied && Started != ESovCampaignResult::AlreadyApplied)
	{ FailCampaignInitialization(TEXT("Restored campaign state rejected the destination mission.")); return; }
	if (!StillRestoring()) { FailCampaignInitialization(TEXT("Ownership changed during mission commit.")); return; }
	if (USovCorruptionComponent* Corruption = RestoringPawn->GetCorruptionComponent())
	{
		if (!Corruption->FinishCampaignRestore(RestoringMission, Error) || !StillRestoring())
		{ FailCampaignInitialization(Error.IsEmpty() ? TEXT("Corruption restore lost its campaign context.") : Error); return; }
	}
	if (!StillRestoring() || !RestoringPawn->CompleteCampaignDataInitialization(false) || !StillRestoring())
	{ FailCampaignInitialization(TEXT("Campaign readiness was invalidated while restoring.")); return; }
	PendingTravelMission = nullptr;
	PendingPawn = nullptr;
	PendingMission = nullptr;
	PendingRecords = FNarrativeSavePlayer();
	bHasPendingRecords = false;
	bHasOriginSnapshot = false;
	OriginSnapshot = FSovProtagonistSnapshot();
	OriginControllerRecord = FNarrativeActorRecord();
	OriginMission = nullptr;
	SetTransitionInputLock(false);
	RefreshGameplayReadiness();
	SetTransitionState(ESovCampaignTransitionState::Idle);
}

void ASovPlayerController::FailCampaignInitialization(const FString& Message)
{
	GetWorldTimerManager().ClearTimer(InitializationTimer);
	UE_LOG(LogTemp, Error, TEXT("Campaign initialization failed: %s"), *Message);
	if (bHasOriginSnapshot && TransitionState != ESovCampaignTransitionState::Recovering && IsValid(OriginMission)
		&& GetPawn() == PendingPawn && IsValid(PendingPawn))
	{
		++TransitionEpoch;
		if (!ClearOutgoingCombatState())
		{
			if (IsValid(PendingPawn)) { PendingPawn->FailCampaignInitialization(); }
			SetTransitionState(ESovCampaignTransitionState::Failed, TEXT("Recovery ownership changed during teardown."));
			return;
		}
		APawn* FailedPawn = PendingPawn;
		UnPossess();
		SetOwnedCharacter(nullptr);
		if (IsValid(FailedPawn)) { FailedPawn->Destroy(); }
		PendingMission = OriginMission;
		PendingPawn = SpawnCampaignPawn(OriginMission, OriginSnapshot.PawnRecord.Transform);
		if (PendingPawn)
		{
			TransitionState = ESovCampaignTransitionState::Recovering;
			PendingRecords = FNarrativeSavePlayer();
			PendingRecords.ControllerData = OriginControllerRecord;
			bHasPendingRecords = true;
			bFromLevelTravel = false;
			Possess(PendingPawn);
			InitializeCampaignPawn(PendingPawn);
			return;
		}
	}
	if (IsValid(PendingPawn)) { PendingPawn->FailCampaignInitialization(); }
	// Do not publish input on a half-restored pawn. The UI can offer the last disk checkpoint.
	SetTransitionState(ESovCampaignTransitionState::Failed, Message);
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
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!Save) { OutError = TEXT("Narrative save subsystem is unavailable."); return false; }
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
	const bool bSaved = Save->CreatePlayerOnlySaveInSlot(this, TravelSaveSlot());
	if (!bSaved || TransitionEpoch != ExpectedEpoch || GetPawn() != Source || PendingTravelMission != Destination)
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
	++TransitionEpoch;
	GetWorldTimerManager().ClearTimer(InitializationTimer);
	Super::EndPlay(EndPlayReason);
}
