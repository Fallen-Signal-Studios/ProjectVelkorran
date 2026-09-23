// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterPolicy.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Campaign/SovEncounterCoordinationComponent.h"
#include "Companions/SovConvergenceCompanionState.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Character/PlayerDefinition.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Weapons/WeaponVisual.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovEchoComponent.h"
#include "Framework/SovPlayerState.h"
#include "Framework/SovPlayerController.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Save/SovSaveSubsystem.h"
#include "Recovery/SovFatalRecoveryComponent.h"
#include "Diagnostics/SovDiagnosticsSubsystem.h"
#include "Engine/GameInstance.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffect.h"
#include "Misc/SecureHash.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Sovereign/SovGameplayTags.h"
#include "UObject/UnrealType.h"

static_assert(static_cast<unsigned>(ESovEncounterState::Inactive) == 0u);
static_assert(static_cast<unsigned>(ESovEncounterState::Active) == 1u);
static_assert(static_cast<unsigned>(ESovEncounterState::Succeeded) == 2u);
static_assert(static_cast<unsigned>(ESovEncounterState::Failed) == 3u);
static_assert(static_cast<unsigned>(ESovEncounterState::Restoring) == 4u);

namespace
{
	bool IsPersistentCharacterPresentation(const AActor* Actor)
	{
		// These actors share combatant ownership with projectiles, but belong to
		// Narrative's character/equipment lifecycle, not an encounter attempt.
		return Actor && (Actor->IsA<ANarrativeCharacterVisual>() || Actor->IsA<AWeaponVisual>());
	}
	bool IsQuiescent(UAbilitySystemComponent* ASC, bool bOwnEntrySuspension = false)
	{
		if (!ASC) { return false; }
		const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
		const FSovGameplayTags& Sov = FSovGameplayTags::Get();
		const FGameplayTag Blockers[] = { Narrative.State_Busy, Narrative.State_Interacting,
			Narrative.State_SequencerControlled, Narrative.State_IsDead,
			Sov.State_Poise_Broken, Sov.State_Poise_Recovering, Sov.State_Guard_Broken };
		for (FGameplayTag Tag : Blockers)
		{
			if (Tag == Narrative.State_Busy && bOwnEntrySuspension)
			{ if (ASC->GetTagCount(Tag) != 1) { return false; } }
			else if (ASC->HasMatchingGameplayTag(Tag)) { return false; }
		}
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.IsActive()) { return false; }
		}
		for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
		{
			const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
			if (Effect && Effect->Spec.GetDuration() > 0.f) { return false; }
		}
		return true;
	}
	bool IsAttributedTo(const AActor* Actor, const ASovEncounterDirector* Director, const AActor* Player)
	{
		TSet<const AActor*> Visited;
		for (const AActor* Owner = Actor ? Actor->GetOwner() : nullptr; Owner && !Visited.Contains(Owner); Owner = Owner->GetOwner())
		{
			if (Owner == Player || !Director->FindParticipantId(Owner).IsNone()) { return true; }
			Visited.Add(Owner);
		}
		const APawn* Instigator = Actor ? Actor->GetInstigator() : nullptr;
		return Instigator && (Instigator == Player || !Director->FindParticipantId(Instigator).IsNone());
	}
}

ASovEncounterDirector::ASovEncounterDirector()
{
	Coordination = CreateDefaultSubobject<USovEncounterCoordinationComponent>(TEXT("SovCoordination"));
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

FGuid ASovEncounterDirector::GetActorGUID_Implementation() const
{
	FGuid Result;
	if (!EncounterId.IsNone())
	{
		FGuid::ParseExact(FMD5::HashAnsiString(*(TEXT("SovereignEncounter:") + EncounterId.ToString())), EGuidFormats::Digits, Result);
	}
	return Result;
}

void ASovEncounterDirector::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	if (SavedGUID != GetActorGUID_Implementation()) { bInvalidEncounterIdentity = true; }
}

void ASovEncounterDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) { return; }
	bInvalidEncounterIdentity = EncounterId.IsNone();
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->EncounterId == EncounterId) { bInvalidEncounterIdentity = true; }
	}
	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleActorSpawned));
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (IsValid(Participant.Character)) { Participant.Character->SetEncounterOwned(); }
	}
	if (bHoldParticipantsBeforeEntry && State == ESovEncounterState::Inactive && !bHasEntryCheckpoint)
	{ SetActorTickEnabled(true); RefreshPreEntryHold(); }
	if (bMaintainLoadedParticipantHold && State == ESovEncounterState::Failed)
	{ SetActorTickEnabled(true); RefreshLoadedParticipantHold(); }
}

void ASovEncounterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	++RestoreGeneration;
	ClearMassRepresentations(true);
	UnbindDeaths();
	ReleaseSuspensions();
	if (GetWorld()) { GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle); }
	Super::EndPlay(EndPlayReason);
}

void ASovEncounterDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASovEncounterDirector, State);
	DOREPLIFETIME(ASovEncounterDirector, AttemptId);
	DOREPLIFETIME(ASovEncounterDirector, Participants);
}

void ASovEncounterDirector::OnRep_State(ESovEncounterState Previous)
{
	OnEncounterStateChanged.Broadcast(Previous, State);
}

void ASovEncounterDirector::SetState(ESovEncounterState NewState)
{
	if (State == NewState) { return; }
	const ESovEncounterState Previous = State;
	State = NewState;
	RefreshMassProcessingState();
	USovDiagnosticsSubsystem::Record(GetWorld(), ESovDiagnosticKind::EncounterState,
		EncounterId, NAME_None, static_cast<float>(NewState), 0.f, NewState == ESovEncounterState::Succeeded);
	if (NewState == ESovEncounterState::Succeeded && GetGameInstance())
	{
		if (USovSaveSubsystem* Slots = GetGameInstance()->GetSubsystem<USovSaveSubsystem>())
		{ Slots->QueueAutosave(ESovSaveBoundary::ArenaExit, EncounterId); }
	}
	ForceNetUpdate();
	OnRep_State(Previous);
}

ASovNPCCharacterBase* ASovEncounterDirector::GetParticipant(FName ParticipantId) const
{
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (Participant.ParticipantId == ParticipantId) { return Participant.Character; }
	}
	return nullptr;
}

FName ASovEncounterDirector::FindParticipantId(const AActor* Actor) const
{
	if (!Actor) { return NAME_None; }
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (Participant.Character == Actor) { return Participant.ParticipantId; }
	}
	return NAME_None;
}

bool ASovEncounterDirector::RegisterParticipant(FName ParticipantId, ASovNPCCharacterBase* Character, bool bRequiredForVictory)
{
	if (!HasAuthority() || State != ESovEncounterState::Inactive || bHasEntryCheckpoint || bMutationInProgress
		|| ParticipantId.IsNone() || !IsValid(Character) || Character->GetWorld() != GetWorld()
		|| Participants.ContainsByPredicate([ParticipantId](const auto& P) { return P.ParticipantId == ParticipantId; }) || !FindParticipantId(Character).IsNone()) { return false; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->FindParticipantId(Character).IsNone()) { return false; }
	}
	FSovEncounterParticipant& Participant = Participants.AddDefaulted_GetRef();
	Participant.ParticipantId = ParticipantId;
	Participant.Character = Character;
	Participant.bRequiredForVictory = bRequiredForVictory;
	Character->SetEncounterOwned();
	if (bHoldParticipantsBeforeEntry) { SetActorTickEnabled(true); RefreshPreEntryHold(); }
	return true;
}

bool ASovEncounterDirector::CaptureNPC(const FSovEncounterParticipant& Participant, FSovEncounterNPCRecord& OutRecord, FString& Error) const
{
	ASovNPCCharacterBase* NPC = Participant.Character;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC);
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!IsValid(NPC) || !NPC->IsEncounterSnapshotReady() || !NPC->IsAlive() || !IsQuiescent(ASC, SuspendedASCs.Contains(ASC) && OwnedBusySuspensions.Contains(ASC))
		|| !NPC->GetNPCDefinition() || NPC->GetEncounterSpawnInfo().OwningSpawnerGUID.IsValid())
	{
		Error = FString::Printf(TEXT("Participant %s must be initialized, alive, idle, and owned by this encounter (not a settlement spawner)."), *Participant.ParticipantId.ToString());
		return false;
	}
	FSovEncounterNPCRecord Record;
	Record.ParticipantId = Participant.ParticipantId;
	Record.bRequiredForVictory = Participant.bRequiredForVictory;
	Record.bAllowMassRepresentation = Participant.bAllowMassRepresentation;
	Record.Definition = NPC->GetNPCDefinition();
	Record.SpawnInfo = NPC->GetEncounterSpawnInfo();
	Record.SpawnInfo.OwningSpawn.Reset();
	FMemoryWriter Writer(Record.SpawnInfoData);
	FObjectAndNameAsStringProxyArchive Archive(Writer, true);
	// Preserve the existing Narrative spawn structure, including override fields
	// that its SaveGame archive otherwise omits. No live spawner is serialized.
	FNPCSpawnInfo::StaticStruct()->SerializeItem(Archive, &Record.SpawnInfo, nullptr);
	Record.WieldEquipSlots = NPC->GetWeaponWieldState().EquipSlots;
	Record.WieldSlots = NPC->GetWeaponWieldState().WieldSlots;
	if (Archive.IsError())
	{ Error = FString::Printf(TEXT("Participant %s could not serialize its Narrative spawn metadata."), *Participant.ParticipantId.ToString()); return false; }
	if (!Save)
	{ Error = FString::Printf(TEXT("Participant %s has no current Narrative save subsystem."), *Participant.ParticipantId.ToString()); return false; }
	if (!Save->CreateActorRecord(NPC, Record.ActorRecord))
	{ Error = FString::Printf(TEXT("Participant %s failed Narrative actor/component record capture."), *Participant.ParticipantId.ToString()); return false; }
	Record.ActorTags = NPC->Tags;
	if (!USovEncounterSnapshotLibrary::CaptureResources(ASC, Record.Resources))
	{
		Error = FString::Printf(TEXT("Participant %s failed canonical combat-resource capture."), *Participant.ParticipantId.ToString());
		if (IsValid(ASC))
		{
			const auto AppendResource = [&](const TCHAR* Name, FGameplayAttribute Current, FGameplayAttribute Maximum)
			{
				Error += FString::Printf(TEXT(" %s=%.3f(base %.3f)/%.3f(base %.3f);"), Name,
					ASC->GetNumericAttribute(Current), ASC->GetNumericAttributeBase(Current),
					ASC->GetNumericAttribute(Maximum), ASC->GetNumericAttributeBase(Maximum));
			};
#define SOV_DESCRIBE_RESOURCE(Name) AppendResource(TEXT(#Name), UNarrativeAttributeSetBase::Get##Name##Attribute(), UNarrativeAttributeSetBase::GetMax##Name##Attribute());
			SOV_DESCRIBE_RESOURCE(Health)
			SOV_DESCRIBE_RESOURCE(Shield)
			SOV_DESCRIBE_RESOURCE(Stamina)
			SOV_DESCRIBE_RESOURCE(Poise)
			SOV_DESCRIBE_RESOURCE(Echo)
#undef SOV_DESCRIBE_RESOURCE
			for (const FActiveGameplayEffectHandle Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
			{
				const FActiveGameplayEffect* Active = ASC->GetActiveGameplayEffect(Handle);
				if (!Active || !Active->Spec.Def) { continue; }
				Error += FString::Printf(TEXT(" Effect=%s period=%.3f inhibited=%d;"),
					*Active->Spec.Def->GetPathName(), Active->Spec.GetPeriod(), Active->bIsInhibited ? 1 : 0);
				for (const FGameplayModifierInfo& Modifier : Active->Spec.Def->Modifiers)
				{
					Error += FString::Printf(TEXT(" %s op=%d magnitudeType=%d;"), *Modifier.Attribute.GetName(),
					static_cast<int32>(Modifier.ModifierOp), static_cast<int32>(Modifier.ModifierMagnitude.GetMagnitudeCalculationType()));
				}
			}
		}
		return false;
	}
	if (Record.Resources.Health <= 0.f)
	{ Error = FString::Printf(TEXT("Participant %s has no living health in its captured resources."), *Participant.ParticipantId.ToString()); return false; }
	if (const USovDismembermentComponent* Sever = NPC->FindComponentByClass<USovDismembermentComponent>())
	{
		Record.SeveredRegionMask = Sever->GetSeveredRegionMask();
	}
	if (const USovWeakPointComponent* Weak = NPC->FindComponentByClass<USovWeakPointComponent>())
	{
		Record.bHasWeakPoints = true;
		Record.WeakPoints = Weak->CaptureWeakPointState();
	}
	TArray<USovCommandLinkComponent*> Links;
	NPC->GetComponents(Links);
	for (const USovCommandLinkComponent* Link : Links)
	{
		FSovEncounterLinkRecord& LinkRecord = Record.Links.AddDefaulted_GetRef();
		LinkRecord.ComponentName = Link->GetFName();
		LinkRecord.State = Link->CaptureCommandLinkState();
		LinkRecord.SourceParticipantId = FindParticipantId(Link->GetCommandSource() ? Link->GetCommandSource() : NPC);
		if (LinkRecord.SourceParticipantId.IsNone() && LinkRecord.State.State != ESovCommandLinkState::Active)
		{
			const auto* Source = Cast<ANarrativeCharacter>(Link->GetCommandSource());
			// A severed link may outlive a defeated source between staged phases. Persist its
			// living owner as the inert restore anchor; never revive or register the corpse.
			if (!IsValid(Source) || !Source->IsAlive()) { LinkRecord.SourceParticipantId = Participant.ParticipantId; }
		}
		if (LinkRecord.SourceParticipantId.IsNone()) { Error = TEXT("Command source must be a registered participant."); return false; }
		for (const AActor* Linked : Link->GetLinkedActors())
		{
			const FName LinkedId = FindParticipantId(Linked);
			if (LinkedId.IsNone())
			{
				const auto* DeadRecipient = Cast<ANarrativeCharacter>(Linked);
				if (LinkRecord.State.State != ESovCommandLinkState::Active && DeadRecipient && !DeadRecipient->IsAlive()) { continue; }
				Error = TEXT("Every living linked actor must be registered with this encounter."); return false;
			}
			LinkRecord.LinkedParticipantIds.AddUnique(LinkedId);
		}
	}
	OutRecord = MoveTemp(Record);
	return true;
}

bool ASovEncounterDirector::CaptureEntryCheckpoint(ASovPlayerCharacterBase* Player, FString& Error)
{
	Error.Reset();
	if (bHoldParticipantsBeforeEntry) { RefreshPreEntryHold(); }
	if (!HasAuthority() || State != ESovEncounterState::Inactive || bHasEntryCheckpoint || bMutationInProgress || bInvalidEncounterIdentity
		|| EncounterId.IsNone() || Participants.IsEmpty() || !IsValid(Player) || !Player->IsCharacterReady() || !Player->IsAlive())
	{
		Error = TEXT("Entry capture requires a uniquely named inactive encounter, participants, and a ready living player.");
		return false;
	}
	if (!Coordination || !Coordination->ValidateComposition(Error) || !ValidateProtectionConfiguration(Error)) { return false; }
	ASovPlayerState* PS = Player->GetPlayerState<ASovPlayerState>();
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!PS || !Save || !IsQuiescent(PS->GetAbilitySystemComponent()) || !Player->GetController())
	{
		Error = TEXT("Entry capture requires a possessed campaign player with no active abilities or timed effects."); return false;
	}
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	FSovProtagonistSnapshot PlayerSnapshot;
	FNarrativeActorRecord ControllerSnapshot;
	TArray<FSovEncounterNPCRecord> NPCSnapshots;
	TSet<FName> IDs;
	TSet<const AActor*> Actors;
	if (!PS->CaptureProtagonistSnapshot(Player, PlayerSnapshot, Error)
		|| !Save->CreateActorRecord(Player->GetController(), ControllerSnapshot)) { return false; }
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (Participant.ParticipantId.IsNone() || IDs.Contains(Participant.ParticipantId) || !IsValid(Participant.Character)
			|| Actors.Contains(Participant.Character)) { Error = TEXT("Participant IDs and actors must be unique and valid."); return false; }
		for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->FindParticipantId(Participant.Character).IsNone())
			{
				Error = TEXT("A participant is already registered with another encounter."); return false;
			}
		}
		IDs.Add(Participant.ParticipantId);
		Actors.Add(Participant.Character);
		FSovEncounterNPCRecord Snapshot;
		if (!CaptureNPC(Participant, Snapshot, Error)) { return false; }
		NPCSnapshots.Add(MoveTemp(Snapshot));
	}
	EncounterPlayer = Player;
	EntryPlayer = MoveTemp(PlayerSnapshot);
	EntryController = MoveTemp(ControllerSnapshot);
	EntryParticipants = MoveTemp(NPCSnapshots);
	EntryProtectedParticipantIds = ProtectedParticipantIds;
	bHasEntryCheckpoint = true;
	SnapshotSchemaVersion = 1;
	for (const FSovEncounterParticipant& Participant : Participants) { Participant.Character->SetEncounterOwned(); SuspendActor(Participant.Character); }
	PS->StoreProtagonistSnapshot(EntryPlayer);
	return true;
}

ASovPlayerCharacterBase* ASovEncounterDirector::ResolvePlayer() const
{
	if (IsValid(EncounterPlayer)) { return EncounterPlayer; }
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ASovPlayerCharacterBase>(PC->GetPawn()) : nullptr;
}

bool ASovEncounterDirector::HasEncounterPlayer(const AActor* Actor) const
{
	return IsValid(Actor) && bHasEntryCheckpoint && Actor == ResolvePlayer();
}

bool ASovEncounterDirector::IsEncounterCombatant(const AActor* Actor) const
{
	return HasEncounterPlayer(Actor);
}

void ASovEncounterDirector::GetEncounterCombatants(TArray<ASovPlayerCharacterBase*>& OutCombatants) const
{
	OutCombatants.Reset();
	if (ASovPlayerCharacterBase* Player = ResolvePlayer(); IsEncounterCombatant(Player)) { OutCombatants.Add(Player); }
}

bool ASovEncounterDirector::IsEntryCheckpointQuiescentForSave(const ASovPlayerCharacterBase* Player) const
{
	FString Error;
	if (!HasAuthority() || State != ESovEncounterState::Inactive || bMutationInProgress || !MassPromotions.IsEmpty() || !MassParticipants.IsEmpty()
		|| !IsValid(Player) || Player != ResolvePlayer() || !Player->IsCharacterReady() || !Player->IsAlive()
		|| !ValidateEntry(Error) || Participants.Num() != EntryParticipants.Num()
		|| !IsQuiescent(Player->GetNarrativeAbilitySystemComponent())) { return false; }
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		const ASovNPCCharacterBase* NPC = Participant.Character;
		UAbilitySystemComponent* ASC = NPC ? NPC->GetAbilitySystemComponent() : nullptr;
		if (!IsValid(NPC) || !NPC->IsAlive() || !NPC->IsEncounterSnapshotReady()
			|| !SuspendedASCs.Contains(ASC) || !IsQuiescent(ASC, true)
			|| !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)) { return false; }
		const APawn* Pawn = Cast<APawn>(NPC);
		const AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
		if (AI && AI->GetBrainComponent() && AI->GetBrainComponent()->IsRunning()
			&& !AI->GetBrainComponent()->IsPaused()) { return false; }
	}
	return true;
}

bool ASovEncounterDirector::BeginEncounter()
{
	if (!HasAuthority() || bMutationInProgress || !SovEncounterPolicy::CanBegin(static_cast<unsigned>(State)) || !bHasEntryCheckpoint) { return false; }
	FString CompositionError;
	if (!Coordination || !Coordination->ValidateComposition(CompositionError) || !ValidateProtectionConfiguration(CompositionError)) { OnEncounterRestoreFailed.Broadcast(CompositionError); return false; }
	Coordination->InitializeCoordination();
	ASovPlayerCharacterBase* Player = ResolvePlayer();
	if (!IsValid(Player) || !Player->IsCharacterReady() || !Player->IsAlive()) { return false; }
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (!IsValid(Participant.Character) || !Participant.Character->IsEncounterSnapshotReady() || !Participant.Character->IsAlive()) { return false; }
	}
	// Commit the frozen entry through the same verified disk slots before releasing enemies.
	// Generic non-campaign encounters retain their existing in-memory checkpoint behavior.
	const ASovPlayerController* PC = Cast<ASovPlayerController>(Player->GetController());
	if (PC && PC->GetCampaignState()->GetActiveMission() && GetGameInstance())
	{
		USovSaveSubsystem* Slots = GetGameInstance()->GetSubsystem<USovSaveSubsystem>();
		FString Error;
		if (!Slots || Slots->EnsureCheckpointBoundary(ESovSaveBoundary::ArenaEntry, EncounterId, Error) != ESovSaveResult::Success)
		{ OnEncounterRestoreFailed.Broadcast(Error); return false; }
		if (State != ESovEncounterState::Inactive || ResolvePlayer() != Player || !Player->IsAlive()) { return false; }
	}
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	EncounterPlayer = Player;
	AttemptId = FGuid::NewGuid();
	const FGuid StartingAttempt = AttemptId;
	AController* const StartingController = Player->GetController();
	ASovPlayerState* const StartingPS = Player->GetPlayerState<ASovPlayerState>();
	UAbilitySystemComponent* const StartingASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
	const TArray<FSovEncounterParticipant> StartingParticipants = Participants;
	const auto OwnsStart = [this, Player, StartingAttempt, StartingController, StartingPS, StartingASC, &StartingParticipants]()
	{
		if (!IsValid(this) || IsActorBeingDestroyed() || State != ESovEncounterState::Inactive || AttemptId != StartingAttempt
			|| !IsValid(Player) || ResolvePlayer() != Player || !Player->IsCharacterReady() || !Player->IsAlive()
			|| !IsValid(StartingController) || Player->GetController() != StartingController || StartingController->GetPawn() != Player
			|| Player->GetPlayerState<ASovPlayerState>() != StartingPS || !IsValid(StartingASC) || StartingASC->GetAvatarActor() != Player
			|| UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player) != StartingASC || Participants.Num() != StartingParticipants.Num()) { return false; }
		for (const auto& Participant : StartingParticipants)
		{ if (!IsValid(Participant.Character) || !Participant.Character->IsAlive() || GetParticipant(Participant.ParticipantId) != Participant.Character) { return false; } }
		return true;
	};
	const auto RejectStaleStart = [this, StartingAttempt]()
	{
		if (IsValid(this) && !IsActorBeingDestroyed() && State == ESovEncounterState::Inactive && AttemptId == StartingAttempt)
		{ SetState(ESovEncounterState::Failed); }
		return false;
	};
	DefeatedParticipants.Reset();
	ClaimedAttemptRewards.Reset();
	BindDeaths();
	if (!OwnsStart() || !ReleaseSuspensions(OwnsStart) || !OwnsStart()) { return RejectStaleStart(); }
	if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->BeginEncounter(); }
	if (!OwnsStart()) { return RejectStaleStart(); }
	SetActorTickEnabled(!ProtectedParticipantIds.IsEmpty());
	SetState(ESovEncounterState::Active);
	if (!IsValid(this) || IsActorBeingDestroyed() || State != ESovEncounterState::Active || AttemptId != StartingAttempt
		|| !IsValid(Player) || !Player->IsAlive() || ResolvePlayer() != Player || !IsValid(StartingController)
		|| Player->GetController() != StartingController || StartingController->GetPawn() != Player
		|| !IsValid(StartingASC) || StartingASC->GetAvatarActor() != Player || Player->GetPlayerState<ASovPlayerState>() != StartingPS
		|| Participants.Num() != StartingParticipants.Num()) { return false; }
	for (const auto& Participant : StartingParticipants)
	{ if (!IsValid(Participant.Character) || GetParticipant(Participant.ParticipantId) != Participant.Character) { return false; } }
	return true;
}

bool ASovEncounterDirector::CompleteEncounter()
{
	if (!HasAuthority() || bMutationInProgress || !MassPromotions.IsEmpty() || !SovEncounterPolicy::CanResolve(static_cast<unsigned>(State))) { return false; }
	if (!AreProtectedParticipantsAlive()) { FailEncounter(); return false; }
	if (bAwaitingCampaignReceipt && bCompleteWhenRequiredParticipantsDefeated && !HasConfirmedRequiredDefeats()) { return false; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	const FGuid CompletingAttempt = AttemptId; const uint64 Generation = RestoreGeneration;
	ASovPlayerCharacterBase* const Player = ResolvePlayer();
	UnbindDeaths();
	if (Player)
	{
		if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->EndEncounter(CompletionEchoReserve); }
	}
	// Echo/resource callbacks may retire the world, load a checkpoint, or kill a protected NPC.
	const auto OwnsCompletion = [this, CompletingAttempt, Generation]()
	{
		return IsValid(this) && !IsActorBeingDestroyed() && State == ESovEncounterState::Active
			&& AttemptId == CompletingAttempt && RestoreGeneration == Generation;
	};
	if (!OwnsCompletion()) { return false; }
	const bool bProtectedAlive = AreProtectedParticipantsAlive() && (!Player || (IsValid(Player) && Player->IsAlive() && ResolvePlayer() == Player));
	// Summoned adds are never victory participants, so nothing else ends them: a won arena would keep live
	// hostiles through its safe-to-save boundary and handoff, and a failed one would let them fight on.
	if (!RetireAttemptCombatants(bProtectedAlive, OwnsCompletion)) { return false; }
	if (!AreProtectedParticipantsAlive() || (Player && (!IsValid(Player) || !Player->IsAlive() || ResolvePlayer() != Player)))
	{
		for (const FSovEncounterParticipant& Participant : Participants) { SuspendActor(Participant.Character); }
		SetState(ESovEncounterState::Failed); return false;
	}
	SetActorTickEnabled(false);
	SetState(ESovEncounterState::Succeeded);
	return true;
}

bool ASovEncounterDirector::FailEncounter()
{
	if (!HasAuthority() || bMutationInProgress || !SovEncounterPolicy::CanResolve(static_cast<unsigned>(State))) { return false; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	UnbindDeaths();
	if (ASovPlayerCharacterBase* Player = ResolvePlayer())
	{
		if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->EndEncounter(CompletionEchoReserve); }
	}
	for (const FSovEncounterParticipant& Participant : Participants) { SuspendActor(Participant.Character); }
	// Adds wait out a failure with the roster they fought beside; the retry's cleanup removes them.
	if (!RetireAttemptCombatants(false, [this]() { return IsValid(this) && !IsActorBeingDestroyed() && State == ESovEncounterState::Active; })) { return false; }
	SetActorTickEnabled(false);
	SetState(ESovEncounterState::Failed);
	return true;
}

bool ASovEncounterDirector::ClaimCompletionReward(FName RewardId)
{
	if (!HasAuthority() || !SovEncounterPolicy::CanClaimCompletionReward(static_cast<unsigned>(State), !RewardId.IsNone(), ClaimedCompletionRewards.Contains(RewardId))) { return false; }
	ClaimedCompletionRewards.Add(RewardId);
	return true;
}

bool ASovEncounterDirector::ClaimAttemptReward(FName RewardId)
{
	if (!HasAuthority() || bMutationInProgress || State != ESovEncounterState::Active || !AttemptId.IsValid()
		|| RewardId.IsNone() || ClaimedAttemptRewards.Contains(RewardId)) { return false; }
	ClaimedAttemptRewards.Add(RewardId);
	return true;
}

void ASovEncounterDirector::BindDeaths()
{
	UnbindDeaths();
	TArray<AActor*> Actors;
	Actors.Add(ResolvePlayer());
	for (const FSovEncounterParticipant& Participant : Participants) { Actors.Add(Participant.Character); }
	for (AActor* Actor : Actors)
	{
		if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor)))
		{
			ASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
			BoundDeathASCs.AddUnique(ASC);
		}
	}
}

void ASovEncounterDirector::UnbindDeaths()
{
	for (UNarrativeAbilitySystemComponent* ASC : BoundDeathASCs)
	{
		if (IsValid(ASC)) { ASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath); }
	}
	BoundDeathASCs.Reset();
}

void ASovEncounterDirector::HandleDeath(AActor* KilledActor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead)
{
	if (!HasAuthority() || !bIsDead || State != ESovEncounterState::Active
		|| !IsValid(KilledActor) || !IsValid(ASC) || !BoundDeathASCs.Contains(ASC)
		|| ASC->GetAvatarActor() != KilledActor || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(KilledActor) != ASC
		|| ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f) { return; }
	if (bMutationInProgress)
	{
		// Mutation guards are held across callback-rich work - releasing suspension, re-enabling
		// collision, resuming a brain - and OnDeathStateChanged fires exactly once. Dropping the
		// receipt here loses the defeat for good, and a required participant that dies inside a guard
		// leaves an encounter that can never complete. Keep it until the guard releases.
		const bool bAlreadyHeld = DeferredDefeats.ContainsByPredicate(
			[KilledActor](const FDeferredDefeat& Held) { return Held.Actor.Get() == KilledActor; });
		if (!bAlreadyHeld && DeferredDefeats.Num() < 64)
		{ DeferredDefeats.Add({ AttemptId, RestoreGeneration, KilledActor }); }
		return;
	}
	if (KilledActor == ResolvePlayer())
	{
		USovFatalRecoveryComponent* Recovery = ResolvePlayer()->GetRecoveryComponent();
		if (!Recovery || !Recovery->OwnsFatalRecovery()) { FailEncounter(); }
		return;
	}
	const FName DefeatedId = FindParticipantId(KilledActor);
	if (DefeatedId.IsNone()) { return; }
	if (ProtectedParticipantIds.Contains(DefeatedId)) { FailEncounter(); return; }
	DefeatedParticipants.Add(DefeatedId);
	EvaluateCompletionConditions();
}

bool ASovEncounterDirector::ValidateProtectionConfiguration(FString& Error) const
{
	if (bHasEntryCheckpoint && ProtectedParticipantIds.Num() != EntryProtectedParticipantIds.Num())
	{ Error = TEXT("Protected participant configuration changed after entry capture."); return false; }
	for (const FName Id : ProtectedParticipantIds)
	{
		const auto* Participant = Participants.FindByPredicate([Id](const auto& P) { return P.ParticipantId == Id; });
		if (Id.IsNone() || (bHasEntryCheckpoint && !EntryProtectedParticipantIds.Contains(Id))
			|| !Participant || Participant->bRequiredForVictory || Participant->bAllowMassRepresentation)
		{ Error = TEXT("Protected participants must be registered non-victory actors without Mass conversion."); return false; }
	}
	return true;
}
bool ASovEncounterDirector::AreProtectedParticipantsAlive() const
{
	FString Error;
	if (!ValidateProtectionConfiguration(Error)) { return false; }
	for (const FName Id : ProtectedParticipantIds)
	{
		const auto* NPC = GetParticipant(Id);
		const auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
		if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || !NPC->IsAlive() || !ASC
			|| ASC->GetAvatarActor() != NPC || !(ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f)
			|| IsParticipantMassRepresented(Id)) { return false; }
	}
	return true;
}
bool ASovEncounterDirector::AreOwnedParticipantsQuiescent(bool bAllowConfirmedDeaths) const
{
	if (Participants.IsEmpty() || !MassPromotions.IsEmpty() || !MassParticipants.IsEmpty()) { return false; }
	for (const auto& Participant : Participants)
	{
		if (bAllowConfirmedDeaths && DefeatedParticipants.Contains(Participant.ParticipantId)) { continue; }
		const auto* NPC = Participant.Character.Get();
		auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
		if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || !NPC->IsAlive() || !NPC->IsEncounterSnapshotReady()
			|| !ASC || ASC->GetAvatarActor() != NPC || !SuspendedASCs.Contains(ASC)
			|| !OwnedBusySuspensions.Contains(ASC) || !OwnedProtectionSuspensions.Contains(ASC)
			|| !IsQuiescent(ASC, true) || !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable)) { return false; }
		const auto* AI = Cast<AAIController>(NPC->GetController());
		if (AI && AI->GetBrainComponent() && AI->GetBrainComponent()->IsRunning() && !AI->GetBrainComponent()->IsPaused()) { return false; }
	}
	return true;
}

bool ASovEncounterDirector::HasConfirmedParticipantDefeat(FName ParticipantId) const
{
	if (!HasAuthority() || IsActorBeingDestroyed() || State != ESovEncounterState::Active
		|| !AttemptId.IsValid() || !DefeatedParticipants.Contains(ParticipantId)) { return false; }
	return bHasEntryCheckpoint
		? EntryParticipants.ContainsByPredicate([ParticipantId](const auto& Entry) { return Entry.ParticipantId == ParticipantId; })
		: Participants.ContainsByPredicate([ParticipantId](const auto& Entry) { return Entry.ParticipantId == ParticipantId; });
}

bool ASovEncounterDirector::HasConfirmedRequiredDefeats() const
{
	bool bHasRequired = false;
	// Frozen entry membership prevents runtime array edits from removing a surviving required enemy.
	if (bHasEntryCheckpoint)
	{
		for (const auto& Record : EntryParticipants)
		{
			if (!Record.bRequiredForVictory) { continue; }
			bHasRequired = true;
			if (!DefeatedParticipants.Contains(Record.ParticipantId)) { return false; }
		}
	}
	else
	{
		for (const auto& Participant : Participants)
		{
			if (!Participant.bRequiredForVictory) { continue; }
			bHasRequired = true;
			if (!DefeatedParticipants.Contains(Participant.ParticipantId)) { return false; }
		}
	}
	return bHasRequired;
}
bool ASovEncounterDirector::HasConfirmedVictory() const
{
	return HasAuthority() && !IsActorBeingDestroyed() && State == ESovEncounterState::Succeeded
		&& bHasEntryCheckpoint && AttemptId.IsValid() && bCompleteWhenRequiredParticipantsDefeated
		&& HasConfirmedRequiredDefeats() && AreProtectedParticipantsAlive();
}

void ASovEncounterDirector::DrainDeferredDefeats()
{
	if (bMutationInProgress || DeferredDefeats.IsEmpty()) { return; }
	TArray<FDeferredDefeat> Held;
	// Taken before replaying, because recording a defeat can complete or fail the encounter, and that
	// runs user code which may defer further deaths into this same list.
	Swap(Held, DeferredDefeats);
	for (const FDeferredDefeat& Entry : Held)
	{
		// A defeat belongs to the attempt and lifecycle that saw it. A retry or a restore starts a new
		// roster, and replaying an old receipt into it would credit a kill that attempt never made.
		if (State != ESovEncounterState::Active || Entry.Attempt != AttemptId || Entry.Generation != RestoreGeneration) { continue; }
		AActor* const Actor = Entry.Actor.Get();
		auto* const ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor));
		if (!IsValid(Actor) || !IsValid(ASC)) { continue; }
		HandleDeath(Actor, ASC, true);
	}
}

void ASovEncounterDirector::EvaluateCompletionConditions()
{
	if (!IsValid(this) || IsActorBeingDestroyed() || !HasAuthority()
		|| State != ESovEncounterState::Active || bMutationInProgress) { return; }
	if (!AreProtectedParticipantsAlive()) { FailEncounter(); return; }
	if (!MassPromotions.IsEmpty() || !bCompleteWhenRequiredParticipantsDefeated) { return; }
	bool bHasRequired = false;
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (!Participant.bRequiredForVictory) { continue; }
		bHasRequired = true;
		// A confirmed kill remains valid after corpse cleanup; arbitrary actor
		// destruction is never silently counted as a kill.
		if (!DefeatedParticipants.Contains(Participant.ParticipantId)) { return; }
	}
	if (bHasRequired) { CompleteEncounter(); }
}

void ASovEncounterDirector::SuspendActor(AActor* Actor, bool bSuspendAbilitySystem)
{
	if (!IsValid(Actor)) { return; }
	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (auto* ThreatController = Cast<ANarrativeNPCController>(Pawn->GetController()))
		{
			SuspendedThreatControllers.Add(ThreatController, Pawn);
			ThreatController->SetThreatMemorySuspended(this, true);
			if (!IsValid(Actor) || !IsValid(ThreatController) || ThreatController->GetPawn() != Pawn
				|| SuspendedThreatControllers.FindRef(ThreatController).Get() != Pawn) { return; }
		}
	}
	if (UAbilitySystemComponent* ASC = bSuspendAbilitySystem ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor) : nullptr)
	{
		if (!SuspendedASCs.Contains(ASC) || !OwnedBusySuspensions.Contains(ASC) || !OwnedProtectionSuspensions.Contains(ASC))
		{
			ASC->CancelAllAbilities();
			if (!IsValid(Actor) || !IsValid(ASC) || ASC->GetAvatarActor() != Actor) { return; }
			SuspendedASCs.AddUnique(ASC); SuspendedAvatars.Add(ASC, Actor);
			if (!OwnedBusySuspensions.Contains(ASC))
			{ OwnedBusySuspensions.Add(ASC); ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll); }
			if (!IsValid(Actor) || !IsValid(ASC) || ASC->GetAvatarActor() != Actor || !SuspendedASCs.Contains(ASC)) { return; }
			if (!OwnedProtectionSuspensions.Contains(ASC))
			{ OwnedProtectionSuspensions.Add(ASC); ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll); }
			if (!IsValid(Actor) || !IsValid(ASC) || ASC->GetAvatarActor() != Actor) { return; }
		}
	}
	const APawn* Pawn = Cast<APawn>(Actor);
	AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	if (AI)
	{
		AI->StopMovement();
		UBrainComponent* Brain = AI->GetBrainComponent();
		if (Brain && Brain->IsRunning() && !Brain->IsPaused())
		{
			PausedBrains.AddUnique(Brain);
			PausedBrainPawns.Add(Brain, AI->GetPawn());
			Brain->PauseLogic(TEXT("Encounter checkpoint restore"));
		}
	}
}

void ASovEncounterDirector::ReleaseSuspensions()
{
	ReleaseSuspensions([]() { return true; });
}

bool ASovEncounterDirector::ReleaseActorSuspension(AActor* Actor, TFunctionRef<bool()> CanContinue)
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!IsValid(Actor) || !IsValid(ASC) || ASC->GetAvatarActor() != Actor || !CanContinue()) { return false; }
	if (SuspendedAvatars.FindRef(ASC).Get() == Actor)
	{
		if (OwnedBusySuspensions.Remove(ASC)) { ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll); }
		if (!CanContinue() || !IsValid(ASC) || ASC->GetAvatarActor() != Actor) { return false; }
		if (OwnedProtectionSuspensions.Remove(ASC)) { ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll); }
		if (!CanContinue()) { return false; }
		SuspendedASCs.Remove(ASC); SuspendedAvatars.Remove(ASC);
	}
	APawn* Pawn = Cast<APawn>(Actor);
	auto* AI = Pawn ? Cast<ANarrativeNPCController>(Pawn->GetController()) : nullptr;
	if (AI && SuspendedThreatControllers.FindRef(AI).Get() == Pawn)
	{
		SuspendedThreatControllers.Remove(AI); AI->SetThreatMemorySuspended(this, false);
		if (!CanContinue()) { return false; }
	}
	UBrainComponent* Brain = IsValid(AI) && AI->GetPawn() == Pawn ? AI->GetBrainComponent() : nullptr;
	if (Brain && PausedBrainPawns.FindRef(Brain).Get() == Pawn)
	{
		PausedBrains.Remove(Brain); PausedBrainPawns.Remove(Brain);
		if (Brain->IsPaused()) { Brain->ResumeLogic(TEXT("Campaign Mass promotion complete")); }
	}
	return CanContinue();
}

bool ASovEncounterDirector::ReleaseSuspensions(TFunctionRef<bool()> CanContinue)
{
	const TArray<TObjectPtr<UAbilitySystemComponent>> ASCs = SuspendedASCs;
	const TArray<TObjectPtr<UBrainComponent>> Brains = PausedBrains;
	const auto ThreatControllers = SuspendedThreatControllers;
	for (UAbilitySystemComponent* ASC : ASCs)
	{
		if (!CanContinue()) { return false; }
		const TWeakObjectPtr<AActor> Avatar = SuspendedAvatars.FindRef(ASC);
		if (IsValid(ASC) && Avatar.IsValid() && ASC->GetAvatarActor() == Avatar.Get())
		{
			if (OwnedBusySuspensions.Remove(ASC)) { ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll); }
			if (!CanContinue()) { return false; }
			if (IsValid(ASC) && ASC->GetAvatarActor() == Avatar.Get() && OwnedProtectionSuspensions.Remove(ASC))
			{ ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll); }
			if (!CanContinue()) { return false; }
		}
		SuspendedASCs.Remove(ASC); SuspendedAvatars.Remove(ASC); OwnedBusySuspensions.Remove(ASC); OwnedProtectionSuspensions.Remove(ASC);
	}
	for (const auto& Pair : ThreatControllers)
	{
		if (!CanContinue()) { return false; }
		if (SuspendedThreatControllers.FindRef(Pair.Key) != Pair.Value) { continue; }
		SuspendedThreatControllers.Remove(Pair.Key);
		if (Pair.Key.IsValid() && Pair.Value.IsValid() && Pair.Key->GetPawn() == Pair.Value.Get())
		{ Pair.Key->SetThreatMemorySuspended(this, false); }
		if (!CanContinue()) { return false; }
	}
	for (UBrainComponent* Brain : Brains)
	{
		if (!CanContinue()) { return false; }
		const TWeakObjectPtr<APawn> Pawn = PausedBrainPawns.FindRef(Brain);
		PausedBrains.Remove(Brain); PausedBrainPawns.Remove(Brain);
		auto* AI = IsValid(Brain) ? Cast<AAIController>(Brain->GetOwner()) : nullptr;
		if (AI && Pawn.IsValid() && AI->GetPawn() == Pawn.Get() && Brain->IsPaused()) { Brain->ResumeLogic(TEXT("Encounter checkpoint ready")); }
		if (!CanContinue()) { return false; }
	}
	return true;
}

void ASovEncounterDirector::RemoveTimedEffects(UAbilitySystemComponent* ASC)
{
	RemoveTimedEffects(ASC, []() { return true; });
}
bool ASovEncounterDirector::RemoveTimedEffects(UAbilitySystemComponent* ASC, TFunctionRef<bool()> CanContinue)
{
	if (!ASC) { return false; }
	const TWeakObjectPtr<AActor> Avatar = ASC->GetAvatarActor();
	const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	for (const FActiveGameplayEffectHandle Handle : Handles)
	{
		if (!CanContinue() || !IsValid(ASC) || ASC->GetAvatarActor() != Avatar.Get()) { return false; }
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (Effect && Effect->Spec.GetDuration() > 0.f) { ASC->RemoveActiveGameplayEffect(Handle); }
		if (!CanContinue()) { return false; }
	}
	return true;
}

void ASovEncounterDirector::HandleActorSpawned(AActor* Actor)
{
	if ((State == ESovEncounterState::Active || State == ESovEncounterState::Failed) && IsAttributedTo(Actor, this, ResolvePlayer())) { RegisterAttemptActor(Actor); }
}

bool ASovEncounterDirector::RegisterAttemptActor(AActor* SpawnedActor)
{
	if (!HasAuthority() || (State != ESovEncounterState::Active && State != ESovEncounterState::Failed) || !IsValid(SpawnedActor) || SpawnedActor->GetWorld() != GetWorld()
		|| SpawnedActor == this || SpawnedActor == ResolvePlayer() || !FindParticipantId(SpawnedActor).IsNone()
		|| IsPersistentCharacterPresentation(SpawnedActor)
		|| !IsAttributedTo(SpawnedActor, this, ResolvePlayer())) { return false; }
	AttemptActors.AddUnique(SpawnedActor);
	return true;
}

bool ASovEncounterDirector::HasLiveAttemptCombatants() const
{
	for (const TObjectPtr<AActor>& Actor : AttemptActors)
	{
		const APawn* const Pawn = Cast<APawn>(Actor);
		// The same filter retirement uses, so the two can never disagree about what is still fighting.
		if (IsValid(Pawn) && !IsPersistentCharacterPresentation(Pawn)) { return true; }
	}
	return false;
}

bool ASovEncounterDirector::RetireAttemptCombatants(bool bDestroy, TFunctionRef<bool()> CanContinue)
{
	UNarrativeSaveSubsystem* const Save = GetWorld() ? GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>() : nullptr;
	// Only fighters. Projectiles and sustain drops keep their existing lifetime until the retry cleanup.
	const TArray<TObjectPtr<AActor>> Snapshot = AttemptActors;
	for (AActor* Actor : Snapshot)
	{
		if (!CanContinue()) { return false; }
		APawn* const Pawn = Cast<APawn>(Actor);
		if (!IsValid(Pawn) || IsPersistentCharacterPresentation(Pawn)) { continue; }
		if (!bDestroy) { SuspendActor(Pawn); continue; }
		AttemptActors.Remove(Pawn);
		if (Save && Pawn->Implements<UNarrativeSavableActor>()) { Save->RemoveSingleActor(Pawn); }
		if (!CanContinue()) { return false; }
		if (!IsValid(Pawn)) { continue; }
		AController* const Controller = Pawn->GetController();
		Pawn->Destroy();
		if (IsValid(Controller) && !Controller->GetPawn()) { Controller->Destroy(); }
	}
	return CanContinue();
}

void ASovEncounterDirector::CleanupAttemptActors()
{
	CleanupAttemptActors([]() { return true; });
}
bool ASovEncounterDirector::CleanupAttemptActors(TFunctionRef<bool()> CanContinue)
{
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	TArray<TObjectPtr<AActor>> ToDestroy = MoveTemp(AttemptActors);
	for (const FGuid& Guid : AttemptActorGuids)
	{
		if (Save)
		{
			if (AActor* Actor = Save->LookupActorByGUID(Guid)) { ToDestroy.AddUnique(Actor); }
		}
	}
	AttemptActorGuids.Reset();
	for (AActor* Actor : ToDestroy)
	{
		if (!CanContinue()) { return false; }
		// Also reject presentation already captured by an older attempt/save.
		if (!IsValid(Actor) || IsPersistentCharacterPresentation(Actor)) { continue; }
		if (Save && Actor->Implements<UNarrativeSavableActor>()) { Save->RemoveSingleActor(Actor); }
		if (!CanContinue()) { return false; }
		if (IsValid(Actor)) { Actor->Destroy(); }
		if (!CanContinue()) { return false; }
	}
	return true;
}

bool ASovEncounterDirector::ValidateEntry(FString& Error, bool bValidateLiveComposition) const
{
	// A failed-world load may retain dead or unresolved command-source actors. Validate
	// the saved entry now, then validate live formation/protection after reconstructing it.
	if (!Coordination || (bValidateLiveComposition && (!Coordination->ValidateComposition(Error) || !ValidateProtectionConfiguration(Error)))) { return false; }
	if (bInvalidEncounterIdentity || SnapshotSchemaVersion != 1 || !bHasEntryCheckpoint || !EntryPlayer.IsValid() || EntryParticipants.IsEmpty())
	{
		Error = TEXT("Missing, incompatible, or ambiguously named encounter checkpoint."); return false;
	}
	TSet<FName> IDs;
	TSet<FGuid> GUIDs;
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		UClass* Class = Record.ActorRecord.ActorSoftClass.LoadSynchronous();
		if (Record.ParticipantId.IsNone() || IDs.Contains(Record.ParticipantId) || !Record.ActorRecord.IsValid()
			|| !Record.ActorRecord.ActorGUID.IsValid() || GUIDs.Contains(Record.ActorRecord.ActorGUID)
			|| !Class || !Class->IsChildOf(ASovNPCCharacterBase::StaticClass()) || !Record.Definition.LoadSynchronous()
			|| !Record.Resources.IsValid() || Record.Resources.Health <= 0.f || Record.SpawnInfoData.IsEmpty())
		{
			Error = TEXT("A checkpoint participant identity, class, definition, or resource record is invalid."); return false;
		}
		if (EntryProtectedParticipantIds.Contains(Record.ParticipantId) && (Record.bRequiredForVictory || Record.bAllowMassRepresentation))
		{ Error = TEXT("Checkpoint protected participant has incompatible victory or representation policy."); return false; }
		IDs.Add(Record.ParticipantId);
		GUIDs.Add(Record.ActorRecord.ActorGUID);
	}
	for (FName ProtectedId : EntryProtectedParticipantIds)
	{ if (!IDs.Contains(ProtectedId)) { Error = TEXT("Protected participant is missing from the checkpoint."); return false; } }
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		for (const FSovEncounterLinkRecord& Link : Record.Links)
		{
			if (!IDs.Contains(Link.SourceParticipantId)) { Error = TEXT("Checkpoint command source no longer exists."); return false; }
			for (FName LinkedId : Link.LinkedParticipantIds)
			{
				if (!IDs.Contains(LinkedId)) { Error = TEXT("Checkpoint link participant no longer exists."); return false; }
			}
		}
	}
	return true;
}

bool ASovEncounterDirector::RetryEncounter(FString& Error)
{
	Error.Reset();
	if (!HasAuthority() || bMutationInProgress || !SovEncounterPolicy::CanRetry(static_cast<unsigned>(State), bHasEntryCheckpoint))
	{ Error = TEXT("Only an active or failed encounter with an entry checkpoint can retry."); return false; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	ASovPlayerCharacterBase* const Player = ResolvePlayer();
	AController* const Controller = IsValid(Player) ? Player->GetController() : nullptr;
	ASovPlayerState* const PS = IsValid(Player) ? Player->GetPlayerState<ASovPlayerState>() : nullptr;
	UAbilitySystemComponent* const PlayerASC = IsValid(Player) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player) : nullptr;
	const ESovEncounterState PreviousState = State; const FGuid RestoringAttempt = AttemptId; const uint64 PreviousGeneration = RestoreGeneration;
	const auto SamePlayer = [this, Player, Controller, PS, PlayerASC]()
	{
		return IsValid(this) && !IsActorBeingDestroyed() && IsValid(Player) && ResolvePlayer() == Player
			&& IsValid(Controller) && Player->GetController() == Controller && Controller->GetPawn() == Player
			&& IsValid(PS) && Player->GetPlayerState<ASovPlayerState>() == PS && IsValid(PlayerASC)
			&& UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player) == PlayerASC && PlayerASC->GetAvatarActor() == Player;
	};
	if (!SamePlayer() || !Player->IsCharacterReady() || !ValidateEntry(Error, false)) { return false; }
	if (!SamePlayer() || State != PreviousState || RestoreGeneration != PreviousGeneration || AttemptId != RestoringAttempt)
	{ Error = TEXT("Checkpoint ownership changed during validation."); return false; }
	UClass* const EntryClass = EntryPlayer.PawnClass.LoadSynchronous();
	UPlayerDefinition* const EntryDefinition = EntryPlayer.PlayerDefinition.LoadSynchronous();
	if (!SamePlayer() || State != PreviousState || RestoreGeneration != PreviousGeneration || AttemptId != RestoringAttempt
		|| Player->GetProtagonistIdentityTag() != EntryPlayer.ProtagonistTag || Player->GetClass() != EntryClass || Player->GetPlayerDefinition() != EntryDefinition)
	{ Error = TEXT("Retry requires the initialized entry protagonist class/definition; finish campaign handoff first."); return false; }
	const uint64 Generation = ++RestoreGeneration;
	const TArray<FSovEncounterNPCRecord> NPCRecords = EntryParticipants;
	const TArray<FSovEncounterParticipant> PreviousParticipants = Participants;
	EncounterPlayer = Player; RestoreController = Controller; RestorePlayerState = PS; RestorePlayerASC = PlayerASC;
	const auto OwnsPreparation = [this, Generation, RestoringAttempt, &SamePlayer]()
	{ return SamePlayer() && RestoreGeneration == Generation && AttemptId == RestoringAttempt && State == ESovEncounterState::Restoring; };
	const auto StopStalePreparation = [this, Generation, RestoringAttempt, &Error]()
	{
		Error = TEXT("Checkpoint ownership changed during retry preparation.");
		if (IsValid(this) && !IsActorBeingDestroyed() && RestoreGeneration == Generation && AttemptId == RestoringAttempt && State == ESovEncounterState::Restoring)
		{ ++RestoreGeneration; bPlayerAndControllerRestored = false; SetActorTickEnabled(false); SetState(ESovEncounterState::Failed); }
	};
	UnbindDeaths(); bPlayerAndControllerRestored = false;
	// Invalidate Mass receipts before the entry checkpoint destroys/replaces any NPC identity.
	ClearMassRepresentations(true);
	SetState(ESovEncounterState::Restoring);
	if (!OwnsPreparation()) { StopStalePreparation(); return false; }
	SuspendActor(Player);
	if (!OwnsPreparation()) { StopStalePreparation(); return false; }
	for (const auto& Participant : PreviousParticipants)
	{
		if (GetParticipant(Participant.ParticipantId) != Participant.Character) { StopStalePreparation(); return false; }
		SuspendActor(Participant.Character);
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
	}
	if (!RemoveTimedEffects(PlayerASC, OwnsPreparation) || !CleanupAttemptActors(OwnsPreparation)) { StopStalePreparation(); return false; }
	UNarrativeSaveSubsystem* const Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	for (const FSovEncounterNPCRecord& Record : NPCRecords)
	{
		ASovNPCCharacterBase* Old = GetParticipant(Record.ParticipantId);
		if (!IsValid(Old) && Save) { Old = Cast<ASovNPCCharacterBase>(Save->LookupActorByGUID(Record.ActorRecord.ActorGUID)); }
		if (IsValid(Old))
		{
			AController* const OldController = Old->GetController();
			Old->Destroy();
			if (!OwnsPreparation()) { StopStalePreparation(); return false; }
			if (IsValid(OldController) && (!OldController->GetPawn() || OldController->GetPawn() == Old)) { OldController->Destroy(); }
			if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		}
	}
	Participants.Reset(); RestoredParticipants.Reset();
	for (const FSovEncounterNPCRecord& Record : NPCRecords)
	{
		FNPCSpawnInfo SpawnInfo = Record.SpawnInfo;
		FMemoryReader Reader(Record.SpawnInfoData); FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		FNPCSpawnInfo::StaticStruct()->SerializeItem(Archive, &SpawnInfo, nullptr);
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		if (Archive.IsError()) { Error = TEXT("Narrative spawn override record is corrupt."); AbortRestore(Error); return false; }
		SpawnInfo.OwningSpawn.Reset();
		ASovNPCCharacterBase* const NPC = GetWorld()->SpawnActorDeferred<ASovNPCCharacterBase>(Record.ActorRecord.ActorSoftClass.Get(), Record.ActorRecord.Transform,
			this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		if (!IsValid(NPC)) { Error = TEXT("Could not spawn a checkpoint participant. Retry remains available."); AbortRestore(Error); return false; }
		// Register the exact replacement before BeginPlay; callbacks can now validate its participant identity.
		FSovEncounterParticipant Participant; Participant.ParticipantId = Record.ParticipantId; Participant.Character = NPC; Participant.bRequiredForVictory = Record.bRequiredForVictory;
		Participant.bAllowMassRepresentation = Record.bAllowMassRepresentation;
		Participants.Add(Participant);
		// Merge before initialization callbacks; preserve new class/constructor tags as well.
		for (const FName Tag : Record.ActorTags) { NPC->Tags.AddUnique(Tag); }
		NPC->PrepareForEncounterRestore(SpawnInfo, Record.ActorRecord.ActorGUID); NPC->SetNPCDefinition(Record.Definition.Get());
		NPC->FinishSpawning(Record.ActorRecord.Transform);
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { Error = TEXT("Checkpoint NPC changed during spawning."); AbortRestore(Error); return false; }
		NPC->EnsureEncounterController();
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { Error = TEXT("Checkpoint NPC changed during controller initialization."); AbortRestore(Error); return false; }
		if (Save) { Save->RefreshStableActorIdentity(NPC); }
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { Error = TEXT("Checkpoint NPC changed during save registration."); AbortRestore(Error); return false; }
		if (UNarrativeCharacterSubsystem* Characters = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>()) { Characters->RegisterCharacter(NPC); }
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { Error = TEXT("Checkpoint NPC changed during character registration."); AbortRestore(Error); return false; }
		SuspendActor(NPC);
		if (!OwnsPreparation()) { StopStalePreparation(); return false; }
	}
	RestoreStartedAt = GetWorld()->GetTimeSeconds(); SetActorTickEnabled(true); return true;
}

void ASovEncounterDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The guards are scoped, so there is no release hook to hang this on; the next tick is the first
	// moment they are reliably gone.
	DrainDeferredDefeats();
	if (bMaintainLoadedParticipantHold)
	{
		if (State != ESovEncounterState::Failed || RestoreGeneration != LoadedParticipantHoldGeneration)
		{ bMaintainLoadedParticipantHold = false; }
		else if (GetWorld() && GetWorld()->GetTimeSeconds() >= NextLoadedParticipantHoldAt)
		{ NextLoadedParticipantHoldAt = GetWorld()->GetTimeSeconds() + .1; RefreshLoadedParticipantHold(); }
	}
	if (bHoldParticipantsBeforeEntry && State == ESovEncounterState::Inactive && !bHasEntryCheckpoint
		&& GetWorld() && GetWorld()->GetTimeSeconds() >= NextPreEntryHoldAt)
	{ NextPreEntryHoldAt = GetWorld()->GetTimeSeconds() + .1; RefreshPreEntryHold(); }
	TickMassPromotions();
	// The final required death may arrive while an unrelated promotion holds
	// completion. Revisit its confirmed receipt after the promotion guard releases.
	EvaluateCompletionConditions();
	if (!IsValid(this) || IsActorBeingDestroyed() || !HasAuthority()
		|| State != ESovEncounterState::Restoring || bMutationInProgress) { return; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!Save || !IsValid(ResolvePlayer())) { AbortRestore(TEXT("Player or Narrative save subsystem disappeared during restore.")); return; }
	if (!RestoreController.IsValid() || RestoreController->GetPawn() != ResolvePlayer() || ResolvePlayer()->GetController() != RestoreController.Get()
		|| !RestorePlayerState.IsValid() || ResolvePlayer()->GetPlayerState<ASovPlayerState>() != RestorePlayerState.Get()
		|| !RestorePlayerASC.IsValid() || RestorePlayerASC->GetAvatarActor() != ResolvePlayer())
	{ ++RestoreGeneration; bPlayerAndControllerRestored = false; SetActorTickEnabled(false); SetState(ESovEncounterState::Failed); return; }
	const uint64 Generation = RestoreGeneration; const FGuid RestoringAttempt = AttemptId;
	ASovPlayerCharacterBase* const RestoringPlayer = ResolvePlayer();
	AController* const RestoringController = RestoringPlayer->GetController();
	const auto OwnsTick = [this, Generation, RestoringAttempt, RestoringPlayer, RestoringController]()
	{ return IsValid(this) && !IsActorBeingDestroyed() && State == ESovEncounterState::Restoring && RestoreGeneration == Generation
		&& AttemptId == RestoringAttempt && IsValid(RestoringPlayer) && ResolvePlayer() == RestoringPlayer
		&& IsValid(RestoringController) && RestoringPlayer->GetController() == RestoringController && RestoringController->GetPawn() == RestoringPlayer
		&& RestoreController.Get() == RestoringController && RestorePlayerState.IsValid() && RestoringPlayer->GetPlayerState<ASovPlayerState>() == RestorePlayerState.Get()
		&& RestorePlayerASC.IsValid() && RestorePlayerASC->GetAvatarActor() == RestoringPlayer; };
	const TArray<FSovEncounterNPCRecord> NPCRecords = EntryParticipants;
	for (const FSovEncounterNPCRecord& Record : NPCRecords)
	{
		ASovNPCCharacterBase* NPC = GetParticipant(Record.ParticipantId);
		if (!IsValid(NPC)) { AbortRestore(TEXT("A checkpoint participant disappeared during restore.")); return; }
		SuspendActor(NPC);
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC suspension changed checkpoint ownership.")); return; }
		if (RestoredParticipants.Contains(Record.ParticipantId)) { continue; }
		if (!NPC->IsEncounterSnapshotReady()) { continue; }
		USovWeakPointComponent* Weak = NPC->FindComponentByClass<USovWeakPointComponent>();
		USovDismembermentComponent* Sever = NPC->FindComponentByClass<USovDismembermentComponent>();
		if ((Record.bHasWeakPoints && (!Weak || !Weak->CanRestoreWeakPointState(Record.WeakPoints)))
			|| (Sever && !Sever->CanRestoreSeveredRegionMask(Record.SeveredRegionMask)))
		{
			AbortRestore(TEXT("Checkpoint weak-point/sever authoring is incompatible with the current NPC.")); return;
		}
		NPC->SetWieldState(FWeaponWieldState());
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC ownership changed while clearing its wield state.")); return; }
		FNarrativeActorRecord ActorRecord = Record.ActorRecord;
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC))
		{
			// The existing ASC serializer can include authored Max attributes.
			// This checkpoint restores explicit combat currents while the newly
			// initialized definition remains authoritative for tuning and maxima.
			ActorRecord.SavedComponents.RemoveAll([&](const FNarrativeSaveComponent& Component) { return Component.ComponentName == ASC->GetFName(); });
		}
		const bool bLoaded = Save->LoadActorFromRecord(NPC, ActorRecord);
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC record callbacks changed checkpoint ownership.")); return; }
		if (!bLoaded) { AbortRestore(TEXT("Narrative rejected a checkpoint NPC record.")); return; }
		NPC->SetActorTransform(Record.ActorRecord.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC transform callbacks changed checkpoint ownership.")); return; }
		FWeaponWieldState Wields;
		Wields.EquipSlots = Record.WieldEquipSlots;
		Wields.WieldSlots = Record.WieldSlots;
		NPC->SetWieldState(Wields);
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC wield callbacks changed checkpoint ownership.")); return; }
		bool bRestored = USovEncounterSnapshotLibrary::RestoreResources(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC), Record.Resources);
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC resource callbacks changed checkpoint ownership.")); return; }
		if (bRestored && Record.bHasWeakPoints) { bRestored = IsValid(Weak) && Weak->RestoreWeakPointState(Record.WeakPoints); }
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC weak-point callbacks changed checkpoint ownership.")); return; }
		if (bRestored && Sever) { bRestored = IsValid(Sever) && Sever->RestoreSeveredRegionMask(Record.SeveredRegionMask); }
		if (!OwnsTick()) { return; }
		if (!IsValid(NPC) || GetParticipant(Record.ParticipantId) != NPC) { AbortRestore(TEXT("NPC sever callbacks changed checkpoint ownership.")); return; }
		if (!bRestored)
		{
			AbortRestore(TEXT("Participant record failed to restore. Entry checkpoint is retained for retry.")); return;
		}
		RestoredParticipants.Add(Record.ParticipantId);
	}
	if (RestoredParticipants.Num() == EntryParticipants.Num()) { FinishRestore(); }
	else if (GetWorld()->GetTimeSeconds() - RestoreStartedAt >= FMath::Max(1.f, RestoreTimeoutSeconds))
	{
		AbortRestore(TEXT("Timed out waiting for NPC definition/appearance initialization."));
	}
}

void ASovEncounterDirector::FinishRestore()
{
	if (State != ESovEncounterState::Restoring || IsActorBeingDestroyed()) { return; }
	const uint64 Generation = RestoreGeneration;
	const FGuid RestoringAttempt = AttemptId;
	ASovPlayerCharacterBase* const Player = ResolvePlayer();
	AController* const Controller = IsValid(Player) ? Player->GetController() : nullptr;
	ASovPlayerState* const PS = IsValid(Player) ? Player->GetPlayerState<ASovPlayerState>() : nullptr;
	UAbilitySystemComponent* const PlayerASC = IsValid(Player) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player) : nullptr;
	UNarrativeSaveSubsystem* const Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	const TArray<FSovEncounterParticipant> ExpectedParticipants = Participants;
	const auto OwnsRestore = [this, Generation, RestoringAttempt, Player, Controller, PS, PlayerASC, &ExpectedParticipants]()
	{
		if (!IsValid(this) || IsActorBeingDestroyed() || RestoreGeneration != Generation || State != ESovEncounterState::Restoring
			|| AttemptId != RestoringAttempt || ResolvePlayer() != Player || !IsValid(Player) || !IsValid(Controller)
			|| Player->GetController() != Controller || Controller->GetPawn() != Player || !IsValid(PS) || Player->GetPlayerState<ASovPlayerState>() != PS
			|| RestoreController.Get() != Controller || RestorePlayerState.Get() != PS || RestorePlayerASC.Get() != PlayerASC
			|| !IsValid(PlayerASC) || UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player) != PlayerASC || PlayerASC->GetAvatarActor() != Player
			|| Participants.Num() != ExpectedParticipants.Num()) { return false; }
		for (const auto& Participant : ExpectedParticipants)
		{ if (!IsValid(Participant.Character) || GetParticipant(Participant.ParticipantId) != Participant.Character) { return false; } }
		return true;
	};
	const auto StopStaleRestore = [this, Generation, RestoringAttempt]()
	{
		// The old arena remains frozen. Do not suspend, revive, release or save replacement owners.
		if (IsValid(this) && !IsActorBeingDestroyed() && RestoreGeneration == Generation && State == ESovEncounterState::Restoring && AttemptId == RestoringAttempt)
		{
			++RestoreGeneration; bPlayerAndControllerRestored = false; SetActorTickEnabled(false);
			SetState(ESovEncounterState::Failed);
		}
	};
	if (!OwnsRestore()) { StopStaleRestore(); return; }
	if (!Save) { AbortRestore(TEXT("Checkpoint save ownership disappeared.")); return; }
	FString Error;
	if (!bPlayerAndControllerRestored)
	{
		const TArray<FSovEncounterNPCRecord> NPCRecords = EntryParticipants;
		const FSovProtagonistSnapshot PlayerRecord = EntryPlayer;
		const FNarrativeActorRecord SavedController = EntryController;
		for (const FSovEncounterNPCRecord& Record : NPCRecords)
		{
			ASovNPCCharacterBase* const NPC = GetParticipant(Record.ParticipantId);
			if (!IsValid(NPC)) { AbortRestore(TEXT("A checkpoint link participant disappeared.")); return; }
			TArray<USovCommandLinkComponent*> Links; NPC->GetComponents(Links);
			for (const FSovEncounterLinkRecord& LinkRecord : Record.Links)
			{
				USovCommandLinkComponent* const* Link = Links.FindByPredicate([&](const USovCommandLinkComponent* Candidate) { return IsValid(Candidate) && Candidate->GetFName() == LinkRecord.ComponentName; });
				TArray<AActor*> LinkedActors;
				for (FName LinkedId : LinkRecord.LinkedParticipantIds) { LinkedActors.Add(GetParticipant(LinkedId)); }
				const bool bRestored = Link && IsValid(*Link) && (*Link)->RestoreCommandLinkState(LinkRecord.State, GetParticipant(LinkRecord.SourceParticipantId), LinkedActors);
				if (!OwnsRestore()) { StopStaleRestore(); return; }
				if (!bRestored) { AbortRestore(TEXT("Checkpoint command link could not restore its participants.")); return; }
			}
		}
		// Formation rules resolve their source from these replacement actors. Refuse
		// a partial or misbound checkpoint before restoring or releasing the player.
		if (!Coordination || !Coordination->ValidateComposition(Error) || !ValidateProtectionConfiguration(Error))
		{ AbortRestore(Error); return; }
		if (!OwnsRestore()) { StopStaleRestore(); return; }
		if (Cast<ASovPlayerController>(Controller) && !USovFatalRecoveryComponent::IsSafeRecoveryPosition(Player, PlayerRecord.PawnRecord.Transform.GetLocation()))
		{ AbortRestore(TEXT("Checkpoint spawn is obstructed or no longer has walkable ground.")); return; }
		const bool bPlayerRestored = PS->RestoreProtagonistSnapshot(Player, PlayerRecord, true, Error);
		if (!OwnsRestore()) { StopStaleRestore(); return; }
		if (!bPlayerRestored) { AbortRestore(Error); return; }
		if (SavedController.IsValid())
		{
			FNarrativeActorRecord ControllerRecord = SavedController; ControllerRecord.Transform = FTransform::Identity; ControllerRecord.bHasTransform = false;
			const bool bControllerRestored = Save->LoadActorFromRecord(Controller, ControllerRecord);
			if (!OwnsRestore()) { StopStaleRestore(); return; }
			if (!bControllerRestored) { AbortRestore(TEXT("Narrative rejected the checkpoint controller record.")); return; }
		}
		if (!PS->StoreProtagonistSnapshot(PlayerRecord)) { AbortRestore(TEXT("Restored protagonist snapshot could not be retained.")); return; }
		if (!OwnsRestore()) { StopStaleRestore(); return; }
		bPlayerAndControllerRestored = true;
	}
	// Player restoration occurs once; a separate companion ASC may complete appearance asynchronously.
	if (ASovPlayerController* PC = Cast<ASovPlayerController>(Controller))
	{
		USovConvergenceCompanionState* CompanionState = PC->GetConvergenceCompanionState();
		if (CompanionState && CompanionState->IsEncounterRestorePending())
		{
			const bool bCompanionRestored = CompanionState->FinishEncounterRestore(Player, Error);
			if (!OwnsRestore()) { StopStaleRestore(); return; }
			if (!bCompanionRestored)
			{
				if (!Error.IsEmpty()) { AbortRestore(Error); }
				else if (GetWorld()->GetTimeSeconds() - RestoreStartedAt >= FMath::Clamp(RestoreTimeoutSeconds, 1.f, 120.f))
				{ AbortRestore(TEXT("Checkpoint companion initialization timed out.")); }
				return;
			}
		}
	}
	if (!Player->IsAlive() || !Player->IsCharacterReady()) { AbortRestore(TEXT("Checkpoint player is not ready for release.")); return; }
	for (const auto& Participant : ExpectedParticipants)
	{
		const bool bSaved = Save->SaveSingleActor(Participant.Character);
		if (!OwnsRestore()) { StopStaleRestore(); return; }
		if (!bSaved) { AbortRestore(TEXT("A restored participant could not be captured for subsequent world streaming.")); return; }
	}
	BindDeaths();
	if (!OwnsRestore()) { StopStaleRestore(); return; }
	if (Player->GetRecoveryComponent())
	{
		const bool bProtected = Player->GetRecoveryComponent()->ProtectRestoredCheckpoint();
		if (!OwnsRestore()) { StopStaleRestore(); return; }
		if (!bProtected) { AbortRestore(TEXT("Checkpoint respawn protection could not be applied.")); return; }
	}
	if (!ReleaseSuspensions(OwnsRestore) || !OwnsRestore()) { StopStaleRestore(); return; }
	if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->BeginEncounter(); }
	if (!OwnsRestore()) { StopStaleRestore(); return; }
	if (!Player->IsAlive()) { AbortRestore(TEXT("Player was defeated during checkpoint release.")); return; }
	AttemptId = FGuid::NewGuid(); DefeatedParticipants.Reset(); ClaimedAttemptRewards.Reset();
	SetActorTickEnabled(!ProtectedParticipantIds.IsEmpty());
	SetState(ESovEncounterState::Active);
}

void ASovEncounterDirector::AbortRestore(const FString& Error)
{
	const uint64 Generation = ++RestoreGeneration;
	const FGuid AbortedAttempt = AttemptId;
	const ESovEncounterState AbortedState = State;
	const auto OwnsAbort = [this, Generation, AbortedAttempt, AbortedState]()
	{ return IsValid(this) && !IsActorBeingDestroyed() && RestoreGeneration == Generation && AttemptId == AbortedAttempt && State == AbortedState; };
	bPlayerAndControllerRestored = false;
	SetActorTickEnabled(false);
	// Retain the entry record and safely stop partial replacements. Retry can
	// rebuild them again; no partial attempt is announced as playable.
	const TArray<FSovEncounterParticipant> PartialParticipants = Participants;
	for (const FSovEncounterParticipant& Participant : PartialParticipants)
	{
		if (!OwnsAbort()) { return; }
		if (IsValid(Participant.Character) && GetParticipant(Participant.ParticipantId) == Participant.Character) { SuspendActor(Participant.Character); }
		if (!OwnsAbort()) { return; }
	}
	SetState(ESovEncounterState::Failed);
	if (IsValid(this) && !IsActorBeingDestroyed() && RestoreGeneration == Generation && AttemptId == AbortedAttempt && State == ESovEncounterState::Failed)
	{ OnEncounterRestoreFailed.Broadcast(Error); }
}

void ASovEncounterDirector::PrepareForSave_Implementation()
{
	if (!HasAuthority()) { return; }
	CaptureMassTransforms();
	for (AActor* Actor : AttemptActors)
	{
		if (IsValid(Actor) && Actor->Implements<UNarrativeSavableActor>())
		{
			const FGuid Guid = INarrativeSavableActor::Execute_GetActorGUID(Actor);
			if (Guid.IsValid()) { AttemptActorGuids.AddUnique(Guid); }
		}
	}
}

void ASovEncounterDirector::Load_Implementation()
{
	if (!HasAuthority()) { return; }
	const uint64 LoadGeneration = ++RestoreGeneration;
	bMaintainLoadedParticipantHold = false;
	ClearMassRepresentations(false);
	UnbindDeaths();
	SetActorTickEnabled(false);
	SetState(static_cast<ESovEncounterState>(SovEncounterPolicy::StateAfterLoad(static_cast<unsigned>(State), bHasEntryCheckpoint)));
	if (IsActorBeingDestroyed() || RestoreGeneration != LoadGeneration) { return; }
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		if (TransferredParticipantIds.Contains(Record.ParticipantId))
		{ Participants.RemoveAll([&Record](const auto& P) { return P.ParticipantId == Record.ParticipantId; }); continue; }
		if (IsParticipantMassRepresented(Record.ParticipantId)) { continue; }
		if (UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			AActor* const SavedActor = Save->LookupActorByGUID(Record.ActorRecord.ActorGUID);
			// A transferred roster starts empty in the map. Confirmed deaths still own their
			// saved membership after Narrative applies a tombstone and removes the actor.
			// Reconstruct only metadata; an unexplained missing living actor is not a defeat.
			if (!SavedActor && bHasEntryCheckpoint && SnapshotSchemaVersion == 1 && AttemptId.IsValid()
				&& (State == ESovEncounterState::Succeeded || State == ESovEncounterState::Failed)
				&& !Record.ParticipantId.IsNone() && Record.ActorRecord.ActorGUID.IsValid()
				&& Record.bRequiredForVictory && DefeatedParticipants.Contains(Record.ParticipantId)
				&& !Participants.ContainsByPredicate([&Record](const auto& P) { return P.ParticipantId == Record.ParticipantId; }))
			{
				int32 IdMatches = 0, GuidMatches = 0;
				for (const auto& Candidate : EntryParticipants)
				{
					IdMatches += Candidate.ParticipantId == Record.ParticipantId ? 1 : 0;
					GuidMatches += Candidate.ActorRecord.ActorGUID == Record.ActorRecord.ActorGUID ? 1 : 0;
				}
				if (IdMatches == 1 && GuidMatches == 1)
				{
					auto& Added = Participants.AddDefaulted_GetRef();
					Added.ParticipantId = Record.ParticipantId;
					Added.bRequiredForVictory = Record.bRequiredForVictory;
					Added.bAllowMassRepresentation = Record.bAllowMassRepresentation;
				}
			}
			if (ASovNPCCharacterBase* NPC = Cast<ASovNPCCharacterBase>(SavedActor))
			{
				if (FSovEncounterParticipant* Participant = Participants.FindByPredicate([&](const FSovEncounterParticipant& Candidate) { return Candidate.ParticipantId == Record.ParticipantId; })) { Participant->Character = NPC; }
				else { FSovEncounterParticipant& Added = Participants.AddDefaulted_GetRef(); Added.ParticipantId = Record.ParticipantId; Added.Character = NPC; Added.bRequiredForVictory = Record.bRequiredForVictory; }
				// Generic world reload can reconstruct a previously retried dynamic NPC too.
				// Only the saved GUID's exact current participant may receive its metadata.
				if (!Record.ActorTags.IsEmpty() && IsValid(NPC) && !NPC->IsActorBeingDestroyed() && NPC->GetWorld() == GetWorld())
				{
					const FGuid CurrentGUID = INarrativeSavableActor::Execute_GetActorGUID(NPC);
					if (IsActorBeingDestroyed() || RestoreGeneration != LoadGeneration) { return; }
					if (CurrentGUID.IsValid() && CurrentGUID == Record.ActorRecord.ActorGUID && IsValid(NPC) && !NPC->IsActorBeingDestroyed()
						&& NPC->GetWorld() == GetWorld() && GetParticipant(Record.ParticipantId) == NPC)
					{
						bool bForeignOwner = false;
						for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
						{
							const FName OtherId = It->FindParticipantId(NPC);
							if (*It != this && !It->IsActorBeingDestroyed() && !OtherId.IsNone()
								&& !It->TransferredParticipantIds.Contains(OtherId)) { bForeignOwner = true; break; }
						}
						if (!bForeignOwner) { for (const FName Tag : Record.ActorTags) { NPC->Tags.AddUnique(Tag); } }
					}
				}
			}
		}
	}
	if (bHoldParticipantsBeforeEntry && State == ESovEncounterState::Inactive && !bHasEntryCheckpoint)
	{ NextPreEntryHoldAt = 0.; SetActorTickEnabled(true); RefreshPreEntryHold(); }
	// Active saves/streamed records remain Failed until explicit RetryEncounter, including C/D participants.
	// Recreate only the director-owned proxy, never resurrect a stale saved combat actor alongside it.
	for (auto& Record : MassParticipants)
	{
		FSovEncounterParticipant* Participant = Participants.FindByPredicate([&](const auto& P) { return P.ParticipantId == Record.NPC.ParticipantId; });
		if (!Participant) { Participant = &Participants.AddDefaulted_GetRef(); Participant->ParticipantId = Record.NPC.ParticipantId; }
		Participant->bRequiredForVictory = Record.NPC.bRequiredForVictory; Participant->bAllowMassRepresentation = true;
		if (IsValid(Participant->Character))
		{
			AController* Controller = Participant->Character->GetController(); Participant->Character->Destroy();
			if (IsValid(Controller) && !Controller->GetPawn()) { Controller->Destroy(); }
		}
		Participant->Character = nullptr;
		FString Error;
		if (!CreateMassEntity(Record, Error)) { SetState(ESovEncounterState::Failed); OnEncounterRestoreFailed.Broadcast(Error); }
	}
	if (IsActorBeingDestroyed() || RestoreGeneration != LoadGeneration) { return; }
	if (State == ESovEncounterState::Failed && bHasEntryCheckpoint)
	{
		// Arena-entry records serialize Inactive before the attempt exists. The existing load
		// policy correctly converts them to Failed; do not release their late-created owners.
		LoadedParticipantHoldGeneration = LoadGeneration;
		bMaintainLoadedParticipantHold = true;
		NextLoadedParticipantHoldAt = 0.;
		SetActorTickEnabled(true);
		RefreshLoadedParticipantHold();
	}
	else if (State == ESovEncounterState::Failed)
	{
		for (const FSovEncounterParticipant& Participant : Participants) { SuspendActor(Participant.Character); }
	}
}

void ASovEncounterDirector::RefreshLoadedParticipantHold()
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !bMaintainLoadedParticipantHold || !bHasEntryCheckpoint
		|| State != ESovEncounterState::Failed || RestoreGeneration != LoadedParticipantHoldGeneration || bMutationInProgress) { return; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	const uint64 Generation = RestoreGeneration;
	const auto Expected = Participants;
	const auto OwnsHold = [this, Generation, &Expected]()
	{
		if (!IsValid(this) || IsActorBeingDestroyed() || !bMaintainLoadedParticipantHold || !bHasEntryCheckpoint
			|| State != ESovEncounterState::Failed || RestoreGeneration != Generation || LoadedParticipantHoldGeneration != Generation
			|| Participants.Num() != Expected.Num()) { return false; }
		for (const auto& P : Expected) { if (GetParticipant(P.ParticipantId) != P.Character) { return false; } }
		return true;
	};
	for (const auto& P : Expected)
	{
		if (!OwnsHold()) { return; }
		auto* NPC = P.Character.Get();
		if (!IsValid(NPC) || NPC->IsActorBeingDestroyed() || NPC->GetWorld() != GetWorld()
			|| TransferredParticipantIds.Contains(P.ParticipantId) || IsParticipantMassRepresented(P.ParticipantId)) { continue; }
		bool bOtherDirectorOwnsActor = false;
		for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
		{ if (*It != this && !It->FindParticipantId(NPC).IsNone()) { bOtherDirectorOwnsActor = true; break; } }
		if (bOtherDirectorOwnsActor) { continue; }
		auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
		if (!NPC->IsEncounterSnapshotReady() || !IsValid(ASC) || ASC->GetAvatarActor() != NPC)
		{
			// Freeze an already-created controller, but leave GAS/visual initialization untouched.
			// A subsequent refresh adds the owned tags after this exact actor publishes readiness.
			SuspendActor(NPC, false);
			if (!OwnsHold()) { return; }
			continue;
		}
		if (!NPC->IsAlive() || ASC->IsDead() || ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f) { continue; }
		// A native ASC reload may clear loose tags on the same object. Only a zero total
		// proves that our old contribution is absent; never remove somebody else's count.
		if (SuspendedAvatars.FindRef(ASC).Get() == NPC)
		{
			if (ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Busy) == 0) { OwnedBusySuspensions.Remove(ASC); }
			if (ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Invulnerable) == 0) { OwnedProtectionSuspensions.Remove(ASC); }
		}
		SuspendActor(NPC);
		if (!OwnsHold()) { return; }
	}
}

void ASovEncounterDirector::RefreshPreEntryHold()
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !bHoldParticipantsBeforeEntry || bHasEntryCheckpoint
		|| State != ESovEncounterState::Inactive || bMutationInProgress || bRefreshingPreEntryHold) { return; }
	TGuardValue<bool> Refreshing(bRefreshingPreEntryHold, true);
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	const uint64 Generation = RestoreGeneration;
	const auto Expected = Participants;
	const auto OwnsHold = [this, Generation, &Expected]()
	{
		if (!IsValid(this) || IsActorBeingDestroyed() || RestoreGeneration != Generation || State != ESovEncounterState::Inactive
			|| !bHoldParticipantsBeforeEntry || bHasEntryCheckpoint || Participants.Num() != Expected.Num()) { return false; }
		for (const auto& P : Expected) { if (GetParticipant(P.ParticipantId) != P.Character) { return false; } }
		return true;
	};
	PreEntryHoldError.Reset();
	if (bInvalidEncounterIdentity || EncounterId.IsNone() || Expected.IsEmpty())
	{ PreEntryHoldError = TEXT("Pre-entry hold requires a unique encounter identity and registered participants."); return; }
	TSet<FName> Ids; TSet<const ASovNPCCharacterBase*> Actors;
	for (const auto& P : Expected)
	{
		const auto* NPC = P.Character.Get();
		if (P.ParticipantId.IsNone() || Ids.Contains(P.ParticipantId) || !IsValid(NPC) || NPC->IsActorBeingDestroyed()
			|| NPC->GetWorld() != GetWorld() || Actors.Contains(NPC) || IsParticipantMassRepresented(P.ParticipantId))
		{ PreEntryHoldError = TEXT("Pre-entry hold requires exact, living authored actors; missing identities cannot be skipped."); return; }
		for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->FindParticipantId(NPC).IsNone())
			{ PreEntryHoldError = TEXT("A pre-entry participant is registered with another encounter director."); return; }
		}
		Ids.Add(P.ParticipantId); Actors.Add(NPC);
	}
	for (const auto& P : Expected)
	{
		if (!OwnsHold()) { return; }
		// Story participants remain available before entry; CaptureEntryCheckpoint still admits every actor.
		if (!P.bRequiredForVictory && !bHoldNonVictoryParticipantsBeforeEntry) { continue; }
		auto* NPC = P.Character.Get();
		auto* ASC = IsValid(NPC) ? NPC->GetNarrativeAbilitySystemComponent() : nullptr;
		// Do not cancel initialization or install a Busy tag before its native readiness is published.
		if (!IsValid(NPC) || !NPC->IsEncounterSnapshotReady() || !IsValid(ASC) || ASC->GetAvatarActor() != NPC)
		{ PreEntryHoldError = FString::Printf(TEXT("Waiting for participant readiness: %s"), *P.ParticipantId.ToString()); continue; }
		if (!NPC->IsAlive() || ASC->IsDead() || !(ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f))
		{ PreEntryHoldError = FString::Printf(TEXT("Participant was defeated before entry: %s"), *P.ParticipantId.ToString()); continue; }
		SuspendActor(NPC);
		if (!OwnsHold()) { return; }
		if (!IsValid(NPC) || !IsValid(ASC) || NPC->GetNarrativeAbilitySystemComponent() != ASC || ASC->GetAvatarActor() != NPC
			|| !SuspendedASCs.Contains(ASC) || !OwnedBusySuspensions.Contains(ASC) || !OwnedProtectionSuspensions.Contains(ASC)
			|| !IsQuiescent(ASC, true))
		{ PreEntryHoldError = FString::Printf(TEXT("Participant has not settled under its entry hold: %s"), *P.ParticipantId.ToString()); }
	}
}

bool ASovEncounterDirector::IsOwnedPreEntryHold(const ASovNPCCharacterBase* NPC) const
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !bHoldParticipantsBeforeEntry
		|| State != ESovEncounterState::Inactive || bHasEntryCheckpoint || bMutationInProgress || bRefreshingPreEntryHold
		|| !IsValid(NPC) || NPC->IsActorBeingDestroyed() || NPC->GetWorld() != GetWorld()
		|| !NPC->IsEncounterSnapshotReady() || !NPC->IsAlive()) { return false; }
	const FName Id = FindParticipantId(NPC);
	if (Id.IsNone() || GetParticipant(Id) != NPC || IsParticipantMassRepresented(Id)) { return false; }
	if (!bHoldNonVictoryParticipantsBeforeEntry && !Participants.ContainsByPredicate(
		[Id](const FSovEncounterParticipant& P) { return P.ParticipantId == Id && P.bRequiredForVictory; })) { return false; }
	auto* ASC = NPC->GetNarrativeAbilitySystemComponent();
	if (!IsValid(ASC) || ASC->GetAvatarActor() != NPC || !SuspendedASCs.Contains(ASC)
		|| SuspendedAvatars.FindRef(ASC).Get() != NPC || !OwnedBusySuspensions.Contains(ASC)
		|| !OwnedProtectionSuspensions.Contains(ASC) || !IsQuiescent(ASC, true)
		|| ASC->GetTagCount(FNarrativeGameplayTags::Get().State_Invulnerable) != 1) { return false; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{ if (*It != this && !It->FindParticipantId(NPC).IsNone()) { return false; } }
	if (NPC->GetController())
	{
		auto* Controller = Cast<ANarrativeNPCController>(NPC->GetController());
		if (!IsValid(Controller) || Controller->GetPawn() != NPC
			|| SuspendedThreatControllers.FindRef(Controller).Get() != NPC
			|| !Controller->IsThreatMemorySuspendedOnlyBy(this)) { return false; }
	}
	return true;
}
