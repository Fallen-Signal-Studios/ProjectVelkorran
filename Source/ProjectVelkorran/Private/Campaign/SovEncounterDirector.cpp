// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEncounterDirector.h"
#include "Campaign/SovEncounterPolicy.h"
#include "Campaign/SovEncounterSnapshotLibrary.h"
#include "Characters/SovNPCCharacterBase.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovDismembermentComponent.h"
#include "Components/SovEchoComponent.h"
#include "Framework/SovPlayerState.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "AI/NarrativeNPCController.h"
#include "AI/NPCDefinition.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
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
	bool IsQuiescent(UAbilitySystemComponent* ASC)
	{
		if (!ASC) { return false; }
		const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
		const FSovGameplayTags& Sov = FSovGameplayTags::Get();
		const FGameplayTag Blockers[] = { Narrative.State_Busy, Narrative.State_Interacting,
			Narrative.State_SequencerControlled, Narrative.State_IsDead,
			Sov.State_Poise_Broken, Sov.State_Poise_Recovering, Sov.State_Guard_Broken };
		for (FGameplayTag Tag : Blockers) { if (ASC->HasMatchingGameplayTag(Tag)) { return false; } }
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
}

void ASovEncounterDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
		|| GetParticipant(ParticipantId) || !FindParticipantId(Character).IsNone()) { return false; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (*It != this && !It->FindParticipantId(Character).IsNone()) { return false; }
	}
	FSovEncounterParticipant& Participant = Participants.AddDefaulted_GetRef();
	Participant.ParticipantId = ParticipantId;
	Participant.Character = Character;
	Participant.bRequiredForVictory = bRequiredForVictory;
	Character->SetEncounterOwned();
	return true;
}

bool ASovEncounterDirector::CaptureNPC(const FSovEncounterParticipant& Participant, FSovEncounterNPCRecord& OutRecord, FString& Error) const
{
	ASovNPCCharacterBase* NPC = Participant.Character;
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC);
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!IsValid(NPC) || !NPC->IsEncounterSnapshotReady() || !NPC->IsAlive() || !IsQuiescent(ASC)
		|| !NPC->GetNPCDefinition() || NPC->GetEncounterSpawnInfo().OwningSpawnerGUID.IsValid())
	{
		Error = FString::Printf(TEXT("Participant %s must be initialized, alive, idle, and owned by this encounter (not a settlement spawner)."), *Participant.ParticipantId.ToString());
		return false;
	}
	FSovEncounterNPCRecord Record;
	Record.ParticipantId = Participant.ParticipantId;
	Record.bRequiredForVictory = Participant.bRequiredForVictory;
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
	if (Archive.IsError() || !Save || !Save->CreateActorRecord(NPC, Record.ActorRecord)
		|| !USovEncounterSnapshotLibrary::CaptureResources(ASC, Record.Resources) || Record.Resources.Health <= 0.f)
	{
		Error = FString::Printf(TEXT("Participant %s could not create a valid Narrative resource/actor record."), *Participant.ParticipantId.ToString());
		return false;
	}
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
		if (LinkRecord.SourceParticipantId.IsNone()) { Error = TEXT("Command source must be a registered participant."); return false; }
		for (const AActor* Linked : Link->GetLinkedActors())
		{
			const FName LinkedId = FindParticipantId(Linked);
			if (LinkedId.IsNone()) { Error = TEXT("Every linked actor must be registered with this encounter."); return false; }
			LinkRecord.LinkedParticipantIds.AddUnique(LinkedId);
		}
	}
	OutRecord = MoveTemp(Record);
	return true;
}

bool ASovEncounterDirector::CaptureEntryCheckpoint(ASovPlayerCharacterBase* Player, FString& Error)
{
	Error.Reset();
	if (!HasAuthority() || State != ESovEncounterState::Inactive || bHasEntryCheckpoint || bMutationInProgress || bInvalidEncounterIdentity
		|| EncounterId.IsNone() || Participants.IsEmpty() || !IsValid(Player) || !Player->IsCharacterReady() || !Player->IsAlive())
	{
		Error = TEXT("Entry capture requires a uniquely named inactive encounter, participants, and a ready living player.");
		return false;
	}
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

bool ASovEncounterDirector::BeginEncounter()
{
	if (!HasAuthority() || bMutationInProgress || !SovEncounterPolicy::CanBegin(static_cast<unsigned>(State)) || !bHasEntryCheckpoint) { return false; }
	ASovPlayerCharacterBase* Player = ResolvePlayer();
	if (!IsValid(Player) || !Player->IsCharacterReady() || !Player->IsAlive()) { return false; }
	for (const FSovEncounterParticipant& Participant : Participants)
	{
		if (!IsValid(Participant.Character) || !Participant.Character->IsEncounterSnapshotReady() || !Participant.Character->IsAlive()) { return false; }
	}
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	EncounterPlayer = Player;
	AttemptId = FGuid::NewGuid();
	DefeatedParticipants.Reset();
	ClaimedAttemptRewards.Reset();
	BindDeaths();
	ReleaseSuspensions();
	if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->BeginEncounter(); }
	SetState(ESovEncounterState::Active);
	return true;
}

bool ASovEncounterDirector::CompleteEncounter()
{
	if (!HasAuthority() || bMutationInProgress || !SovEncounterPolicy::CanResolve(static_cast<unsigned>(State))) { return false; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	UnbindDeaths();
	if (ASovPlayerCharacterBase* Player = ResolvePlayer())
	{
		if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->EndEncounter(CompletionEchoReserve); }
	}
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
	if (!HasAuthority() || !bIsDead || State != ESovEncounterState::Active || bMutationInProgress) { return; }
	if (KilledActor == ResolvePlayer()) { FailEncounter(); return; }
	const FName DefeatedId = FindParticipantId(KilledActor);
	if (DefeatedId.IsNone()) { return; }
	DefeatedParticipants.Add(DefeatedId);
	if (!bCompleteWhenRequiredParticipantsDefeated) { return; }
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

void ASovEncounterDirector::SuspendActor(AActor* Actor)
{
	if (!IsValid(Actor)) { return; }
	if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor))
	{
		if (!SuspendedASCs.Contains(ASC))
		{
			ASC->CancelAllAbilities();
			ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
			ASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll);
			SuspendedASCs.Add(ASC);
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
			Brain->PauseLogic(TEXT("Encounter checkpoint restore"));
			PausedBrains.AddUnique(Brain);
		}
	}
}

void ASovEncounterDirector::ReleaseSuspensions()
{
	TArray<TObjectPtr<UAbilitySystemComponent>> ASCs = MoveTemp(SuspendedASCs);
	TArray<TObjectPtr<UBrainComponent>> Brains = MoveTemp(PausedBrains);
	for (UAbilitySystemComponent* ASC : ASCs)
	{
		if (!IsValid(ASC)) { continue; }
		ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
		ASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Invulnerable, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	for (UBrainComponent* Brain : Brains)
	{
		if (IsValid(Brain) && Brain->IsPaused()) { Brain->ResumeLogic(TEXT("Encounter checkpoint ready")); }
	}
}

void ASovEncounterDirector::RemoveTimedEffects(UAbilitySystemComponent* ASC)
{
	if (!ASC) { return; }
	const TArray<FActiveGameplayEffectHandle> Handles = ASC->GetActiveEffects(FGameplayEffectQuery());
	for (const FActiveGameplayEffectHandle Handle : Handles)
	{
		const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle);
		if (Effect && Effect->Spec.GetDuration() > 0.f) { ASC->RemoveActiveGameplayEffect(Handle); }
	}
}

void ASovEncounterDirector::HandleActorSpawned(AActor* Actor)
{
	if ((State == ESovEncounterState::Active || State == ESovEncounterState::Failed) && IsAttributedTo(Actor, this, ResolvePlayer())) { RegisterAttemptActor(Actor); }
}

bool ASovEncounterDirector::RegisterAttemptActor(AActor* SpawnedActor)
{
	if (!HasAuthority() || (State != ESovEncounterState::Active && State != ESovEncounterState::Failed) || !IsValid(SpawnedActor) || SpawnedActor->GetWorld() != GetWorld()
		|| SpawnedActor == this || SpawnedActor == ResolvePlayer() || !FindParticipantId(SpawnedActor).IsNone()
		|| !IsAttributedTo(SpawnedActor, this, ResolvePlayer())) { return false; }
	AttemptActors.AddUnique(SpawnedActor);
	return true;
}

void ASovEncounterDirector::CleanupAttemptActors()
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
		if (!IsValid(Actor)) { continue; }
		if (Save && Actor->Implements<UNarrativeSavableActor>()) { Save->RemoveSingleActor(Actor); }
		Actor->Destroy();
	}
}

bool ASovEncounterDirector::ValidateEntry(FString& Error) const
{
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
		IDs.Add(Record.ParticipantId);
		GUIDs.Add(Record.ActorRecord.ActorGUID);
	}
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
	{
		Error = TEXT("Only an active or failed encounter with an entry checkpoint can retry."); return false;
	}
	ASovPlayerCharacterBase* Player = ResolvePlayer();
	if (!ValidateEntry(Error)) { return false; }
	if (!IsValid(Player) || !Player->IsCharacterReady() || !Player->GetPlayerState<ASovPlayerState>()
		|| Player->GetProtagonistIdentityTag() != EntryPlayer.ProtagonistTag || Player->GetClass() != EntryPlayer.PawnClass.LoadSynchronous()
		|| Player->GetPlayerDefinition() != EntryPlayer.PlayerDefinition.LoadSynchronous())
	{
		Error = TEXT("Retry requires the initialized entry protagonist class/definition; finish campaign handoff first."); return false;
	}
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	EncounterPlayer = Player;
	UnbindDeaths();
	SetState(ESovEncounterState::Restoring);
	SuspendActor(Player);
	for (const FSovEncounterParticipant& Participant : Participants) { SuspendActor(Participant.Character); }
	RemoveTimedEffects(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player));
	CleanupAttemptActors();
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		ASovNPCCharacterBase* Old = GetParticipant(Record.ParticipantId);
		if (!IsValid(Old) && Save) { Old = Cast<ASovNPCCharacterBase>(Save->LookupActorByGUID(Record.ActorRecord.ActorGUID)); }
		if (IsValid(Old))
		{
			AController* OldController = Old->GetController();
			Old->Destroy();
			if (IsValid(OldController)) { OldController->Destroy(); }
		}
	}
	Participants.Reset();
	RestoredParticipants.Reset();
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		FNPCSpawnInfo SpawnInfo = Record.SpawnInfo;
		FMemoryReader Reader(Record.SpawnInfoData);
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		FNPCSpawnInfo::StaticStruct()->SerializeItem(Archive, &SpawnInfo, nullptr);
		if (Archive.IsError()) { Error = TEXT("Narrative spawn override record is corrupt."); AbortRestore(Error); return false; }
		SpawnInfo.OwningSpawn.Reset();
		ASovNPCCharacterBase* NPC = GetWorld()->SpawnActorDeferred<ASovNPCCharacterBase>(Record.ActorRecord.ActorSoftClass.Get(), Record.ActorRecord.Transform,
			this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!NPC) { Error = TEXT("Could not spawn a checkpoint participant. Retry remains available."); AbortRestore(Error); return false; }
		NPC->PrepareForEncounterRestore(SpawnInfo, Record.ActorRecord.ActorGUID);
		NPC->SetNPCDefinition(Record.Definition.Get());
		NPC->FinishSpawning(Record.ActorRecord.Transform);
		NPC->EnsureEncounterController();
		if (Save) { Save->RefreshStableActorIdentity(NPC); }
		FSovEncounterParticipant& Participant = Participants.AddDefaulted_GetRef();
		Participant.ParticipantId = Record.ParticipantId;
		Participant.Character = NPC;
		Participant.bRequiredForVictory = Record.bRequiredForVictory;
		if (UNarrativeCharacterSubsystem* Characters = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>()) { Characters->RegisterCharacter(NPC); }
		SuspendActor(NPC);
	}
	RestoreStartedAt = GetWorld()->GetTimeSeconds();
	SetActorTickEnabled(true);
	return true;
}

void ASovEncounterDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || State != ESovEncounterState::Restoring || bMutationInProgress) { return; }
	TGuardValue<bool> Mutation(bMutationInProgress, true);
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (!Save || !IsValid(ResolvePlayer())) { AbortRestore(TEXT("Player or Narrative save subsystem disappeared during restore.")); return; }
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		ASovNPCCharacterBase* NPC = GetParticipant(Record.ParticipantId);
		if (!IsValid(NPC)) { AbortRestore(TEXT("A checkpoint participant disappeared during restore.")); return; }
		SuspendActor(NPC);
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
		FNarrativeActorRecord ActorRecord = Record.ActorRecord;
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC))
		{
			// The existing ASC serializer can include authored Max attributes.
			// This checkpoint restores explicit combat currents while the newly
			// initialized definition remains authoritative for tuning and maxima.
			ActorRecord.SavedComponents.RemoveAll([&](const FNarrativeSaveComponent& Component) { return Component.ComponentName == ASC->GetFName(); });
		}
		if (!Save->LoadActorFromRecord(NPC, ActorRecord)) { AbortRestore(TEXT("Narrative rejected a checkpoint NPC record.")); return; }
		NPC->SetActorTransform(Record.ActorRecord.Transform, false, nullptr, ETeleportType::TeleportPhysics);
		FWeaponWieldState Wields;
		Wields.EquipSlots = Record.WieldEquipSlots;
		Wields.WieldSlots = Record.WieldSlots;
		NPC->SetWieldState(Wields);
		if (!USovEncounterSnapshotLibrary::RestoreResources(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC), Record.Resources)
			|| (Record.bHasWeakPoints && !Weak->RestoreWeakPointState(Record.WeakPoints))
			|| (Sever && !Sever->RestoreSeveredRegionMask(Record.SeveredRegionMask)))
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
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		ASovNPCCharacterBase* NPC = GetParticipant(Record.ParticipantId);
		TArray<USovCommandLinkComponent*> Links;
		NPC->GetComponents(Links);
		for (const FSovEncounterLinkRecord& LinkRecord : Record.Links)
		{
			USovCommandLinkComponent* const* Link = Links.FindByPredicate([&](const USovCommandLinkComponent* Candidate) { return Candidate->GetFName() == LinkRecord.ComponentName; });
			TArray<AActor*> LinkedActors;
			for (FName LinkedId : LinkRecord.LinkedParticipantIds) { LinkedActors.Add(GetParticipant(LinkedId)); }
			if (!Link || !(*Link)->RestoreCommandLinkState(LinkRecord.State, GetParticipant(LinkRecord.SourceParticipantId), LinkedActors))
			{
				AbortRestore(TEXT("Checkpoint command link could not restore its participants.")); return;
			}
		}
	}
	ASovPlayerCharacterBase* Player = ResolvePlayer();
	ASovPlayerState* PS = Player ? Player->GetPlayerState<ASovPlayerState>() : nullptr;
	FString Error;
	if (!PS || !PS->RestoreProtagonistSnapshot(Player, EntryPlayer, true, Error)) { AbortRestore(Error); return; }
	UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>();
	if (EntryController.IsValid() && Player->GetController())
	{
		FNarrativeActorRecord ControllerRecord = EntryController;
		ControllerRecord.Transform = FTransform::Identity;
		if (!Save->LoadActorFromRecord(Player->GetController(), ControllerRecord)) { AbortRestore(TEXT("Narrative rejected the checkpoint controller record.")); return; }
	}
	PS->StoreProtagonistSnapshot(EntryPlayer);
	for (const FSovEncounterParticipant& Participant : Participants) { Save->SaveSingleActor(Participant.Character); }
	AttemptId = FGuid::NewGuid();
	DefeatedParticipants.Reset();
	ClaimedAttemptRewards.Reset();
	BindDeaths();
	SetActorTickEnabled(false);
	ReleaseSuspensions();
	if (USovEchoComponent* Echo = Player->GetEchoComponent()) { Echo->BeginEncounter(); }
	SetState(ESovEncounterState::Active);
}

void ASovEncounterDirector::AbortRestore(const FString& Error)
{
	SetActorTickEnabled(false);
	// Retain the entry record and safely stop partial replacements. Retry can
	// rebuild them again; no partial attempt is announced as playable.
	for (const FSovEncounterParticipant& Participant : Participants) { SuspendActor(Participant.Character); }
	SetState(ESovEncounterState::Failed);
	OnEncounterRestoreFailed.Broadcast(Error);
}

void ASovEncounterDirector::PrepareForSave_Implementation()
{
	if (!HasAuthority()) { return; }
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
	UnbindDeaths();
	SetActorTickEnabled(false);
	SetState(static_cast<ESovEncounterState>(SovEncounterPolicy::StateAfterLoad(static_cast<unsigned>(State), bHasEntryCheckpoint)));
	for (const FSovEncounterNPCRecord& Record : EntryParticipants)
	{
		if (UNarrativeSaveSubsystem* Save = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			if (ASovNPCCharacterBase* NPC = Cast<ASovNPCCharacterBase>(Save->LookupActorByGUID(Record.ActorRecord.ActorGUID)))
			{
				if (FSovEncounterParticipant* Participant = Participants.FindByPredicate([&](const FSovEncounterParticipant& Candidate) { return Candidate.ParticipantId == Record.ParticipantId; })) { Participant->Character = NPC; }
				else { FSovEncounterParticipant& Added = Participants.AddDefaulted_GetRef(); Added.ParticipantId = Record.ParticipantId; Added.Character = NPC; Added.bRequiredForVictory = Record.bRequiredForVictory; }
			}
		}
	}
	if (State == ESovEncounterState::Failed)
	{
		for (const FSovEncounterParticipant& Participant : Participants) { SuspendActor(Participant.Character); }
	}
}
