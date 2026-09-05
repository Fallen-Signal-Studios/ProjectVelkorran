// Copyright Narrative Tools 2025.


#include "Cinematics/NarrativeLevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "AI/NPCDefinition.h"
#include "AbilitySystemComponent.h"
#include "ArsenalSettings.h"
#include "Cinematics/NarrativeLevelSequencePlayer.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "ArsenalStatics.h"
#include "MovieSceneSequencePlayer.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "Character/NarrativeCharacterVisual.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "Spawners/NPCSpawnComponent.h"
#include "Spawners/NPCSpawner.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "MovieScene.h"
#include "GAS/NarrativeAbilitySystemComponent.h"

static const FName TAG_Player("Player");
static const FName TAG_PlayerController("PlayerController");

ANarrativeLevelSequenceActor::ANarrativeLevelSequenceActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UNarrativeLevelSequencePlayer>("AnimationPlayer"))
{
	bReplicatePlayback = true; 
	
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true; 
	SetActorTickEnabled(true);

	GetSequencePlayer()->OnPlay.AddDynamic(this, &ANarrativeLevelSequenceActor::OnPlay);
	GetSequencePlayer()->OnPlayReverse.AddDynamic(this, &ANarrativeLevelSequenceActor::OnPlay);
	GetSequencePlayer()->OnStop.AddDynamic(this, &ANarrativeLevelSequenceActor::OnStop);
	GetSequencePlayer()->OnFinished.AddDynamic(this, &ANarrativeLevelSequenceActor::OnStop);
}

void ANarrativeLevelSequenceActor::BeginPlay()
{
	AActor::BeginPlay();

	if (GetSequencePlayer())
	{
		AddReplicatedSubObject(GetSequencePlayer());
	}
	
	if (PlaybackSettings.bAutoPlay)
	{
		PlaySequence();
	}

}


bool ANarrativeLevelSequenceActor::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	FVector FixedSource = SrcLocation;

	//Generally speaking the relevancy source location in a cinematic is really the location of the person viewing it, and shouldnt use our camera because when the cutscene starts it will move our camera around. 
	//Otherwise our own cutscene may stop playing immediately because the cinecam takes us away from the location of the actor itself which isn't what we want. 
	if (const AController* Ctrl = Cast<AController>(RealViewer))
	{
		if (APawn* CtrlPawn = Ctrl->GetPawn())
		{
			FixedSource = CtrlPawn->GetActorLocation();
		}
	}
	
	//Sequence should only replicate to those in the owner controllers list. If thats empty, we can just use standard check. 
	if (OwnerControllers.Num() > 0)
	{

		return OwnerControllers.Contains(RealViewer) && Super::IsNetRelevantFor(RealViewer, ViewTarget, FixedSource);
	}
	
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, FixedSource);
}

TArray<UObject*> ANarrativeLevelSequenceActor::GetBoundObjects() const
{
	TArray<TObjectPtr<UObject>> Objs;

	if(ULevelSequencePlayer* Player = GetSequencePlayer())
	{
		if (ULevelSequence* LS = Cast<ULevelSequence>(Player->GetSequence()))
		{
			if (const UMovieScene* MS = LS->GetMovieScene())
			{
				const TArray<FMovieSceneBinding>& Bindings = MS->GetBindings();

				for (const FMovieSceneBinding& Binding : Bindings)
				{
					FGuid ObjectGuid = Binding.GetObjectGuid();

					for (auto& Object : Player->FindBoundObjects(ObjectGuid, MovieSceneSequenceID::Root))
					{
						if (Object.IsValid())
						{
							Objs.AddUnique(Object.Get());
						}
					}
				}
			}
		}
	}

	return Objs;

}

void ANarrativeLevelSequenceActor::SetBindingsUsingSpawner(class ANPCSpawner* Spawner)
{
	if (Spawner)
	{
		TArray<UActorComponent*> Spawns;
		Spawner->GetComponents(UNPCSpawnComponent::StaticClass(), Spawns);

		for (auto& Spawn : Spawns)
		{
			if (UNPCSpawnComponent* NPCSpawn = Cast<UNPCSpawnComponent>(Spawn))
			{
				if (ANarrativeNPCCharacter* NPCChar = NPCSpawn->GetSpawnedNPC())
				{
					FName BindName = NPCSpawn->GetFName();
					SetBindingByTag(BindName, {NPCChar});
				}
			}
		}
	}
}

/*
bool ANarrativeLevelSequenceActor::HandleBindingsAndStartSequence()
{
	if (GetSequencePlayer() && GetSequencePlayer()->GetSequence())
	{
		if (NarrativeSequenceParams.BindingConfigs.Num() > 0)
		{
			if (NarrativeSequenceParams.StopTags.IsValid())
			{
				for (auto& BindingConfig : NarrativeSequenceParams.BindingConfigs)
				{
					if (BindingConfig.Character)
					{
						//We'll reuse this to stop the character from moving 
						if (NarrativeSequenceParams.bDisableMovementInput)
						{
							if (UCharacterMovementComponent* CMC = BindingConfig.Character->GetCharacterMovement())
							{
								CMC->StopMovementImmediately();
							}
						}

						//If any players have stop tags nope out 
						if (UAbilitySystemComponent* ASC = BindingConfig.Character->GetAbilitySystemComponent())
						{
							if (ASC->HasAnyMatchingGameplayTags(NarrativeSequenceParams.StopTags))
							{
								return false;
							}
						}
					}
				}
			}
			

		}

		PlaySequence();


		SetActorTickEnabled(true);

		return true; 
	}

	return false; 
}*/

void ANarrativeLevelSequenceActor::UpdateSequence(ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings InSettings)
{
	if (!CanAcceptPlayback()) { return; }
	++PlaybackGeneration;
	if (!IsValid(LevelSequence) || !GetSequencePlayer()) { FailPlayback(); return; }
	{
		TGuardValue<bool> Changing(bChangingSequence, true);
		if (GetSequencePlayer()->IsPlaying() || GetSequencePlayer()->IsPaused()) { GetSequencePlayer()->Stop(); }
		OnStop();
		if (bIsEndingPlay || IsActorBeingDestroyed()) { return; }
		SetSequence(LevelSequence);
		PlaybackSettings = InSettings;
		NarrativeSequenceParams = InSettings;
		GetSequencePlayer()->SetPlaybackSettings(InSettings);
	}
	if (PlaybackSettings.bAutoPlay) { PlaySequence(); }
}

void ANarrativeLevelSequenceActor::BlendOutAndStop()
{
	if (bBlendingOut || !CanAcceptPlayback()) { return; }
	bBlendingOut = true;
	bPendingPlayback = false;
	for (UObject* Object : GetBoundObjects())
	{
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Object))
		{ if (auto* Instance = Cast<UNarrativeAnimInstance>(Mesh->GetAnimInstance())) { Instance->BlendOutOfSequencer(); } }
		else if (const auto* Character = Cast<ANarrativeCharacter>(Object))
		{ if (auto* Instance = Cast<UNarrativeAnimInstance>(Character->GetMesh()->GetAnimInstance())) { Instance->BlendOutOfSequencer(); } }
	}
	const float Delay = FMath::IsFinite(BlendOutSeconds) ? FMath::Clamp(BlendOutSeconds, 0.f, 2.f) : 0.f;
	if (GetWorld() && Delay > 0.f) { GetWorld()->GetTimerManager().SetTimer(BlendOutTimer, this, &ANarrativeLevelSequenceActor::FinishBlendOut, Delay, false); }
	else { FinishBlendOut(); }
}

void ANarrativeLevelSequenceActor::FinishBlendOut()
{
	if (GetSequencePlayer()) { GetSequencePlayer()->Stop(); }
	OnStop(); // Stop is a no-op on an already-stopped player; completion must still be finite.
}

UNarrativeLevelSequencePlayer* ANarrativeLevelSequenceActor::CreateNarrativeLevelSequencePlayer(UObject* WorldContextObject, const TArray<APlayerController*>& Players, const FVector SpawnLocation, const float RelevancyDist, ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings Settings, ANarrativeLevelSequenceActor*& OutActor)
{
	if (LevelSequence == nullptr || !IsValid(WorldContextObject))
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World == nullptr || World->bIsTearingDown)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	SpawnParams.bAllowDuringConstructionScript = true;
	
	//SpawnParams.Owner = OwnerPlayer;

	// Defer construction for autoplay so that BeginPlay() is called
	SpawnParams.bDeferConstruction = true;

	if (ANarrativeLevelSequenceActor* LevelSequenceActor = World->SpawnActor<ANarrativeLevelSequenceActor>(SpawnParams))
	{
		LevelSequenceActor->PlaybackSettings = Settings;
		LevelSequenceActor->NarrativeSequenceParams = Settings;
		LevelSequenceActor->OwnerControllers = Players;
		LevelSequenceActor->GetSequencePlayer()->SetPlaybackSettings(Settings);

		LevelSequenceActor->SetSequence(LevelSequence);

		LevelSequenceActor->InitializePlayer();
		OutActor = LevelSequenceActor;

		FTransform DefaultTransform;
		DefaultTransform.SetLocation(SpawnLocation);
		LevelSequenceActor->FinishSpawning(DefaultTransform);

		LevelSequenceActor->bAlwaysRelevant = RelevancyDist <= KINDA_SMALL_NUMBER;
		LevelSequenceActor->SetNetCullDistanceSquared(RelevancyDist * RelevancyDist);
		
		return Cast<UNarrativeLevelSequencePlayer>(LevelSequenceActor->GetSequencePlayer());
	}
	
	return nullptr; 
}

void ANarrativeLevelSequenceActor::BindingVisualReady(class ANarrativeCharacter* Character)
{
	RefreshBindings();
}

void ANarrativeLevelSequenceActor::RefreshBindings()
{
	if (!GetSequencePlayer() || !GetSequencePlayer()->GetSequence()) { return; }
	if (UMovieScene* Scene = GetSequencePlayer()->GetSequence()->GetMovieScene())
	{
		TMap<FName, FMovieSceneObjectBindingIDs> AllTaggedBindings = Scene->AllTaggedBindings();

		//Narrative should autobind anything using character IDs. 
		for (auto& Bind : AllTaggedBindings)
		{
			if (UNarrativeCharacterSubsystem* CharSS = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
			{
				//Player controller has special meaning. It should bind our local controller on clients, and all controllers viewing the cutscene on the server.
				//If you only need to bind a particular controller, you should use the standard characterID route ie MySpecificPersonsIDController
				if (Bind.Key == TAG_PlayerController)
				{
					if (!HasAuthority())
					{
						SetBindingByTag(Bind.Key, {UGameplayStatics::GetPlayerController(this, 0)});
					}
					else
					{
						SetBindingByTag(Bind.Key, TArray<AActor*>(OwnerControllers));
					}
				}
				else
				{
					ANarrativeCharacter* Char = nullptr;

					// "Player" has special meaning. It should bind the local viewer if only 1 person is viewing. In networked games you probably dont want this, and will want to use standard binding process instead in order to specify which player takes which binding.
					// The main reason we've kept this is backwards compatibility, the player used to be defined in single player games using tag "Player" so ideally this should still work. 
					if (Bind.Key == TAG_Player)
					{
						if (HasAuthority() && OwnerControllers.Num() == 1 && OwnerControllers.IsValidIndex(0))
						{
							if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(OwnerControllers[0]))
							{
								Char = PC->GetNarrativeCharacter();
							}
						}
						else
						{
							Char = UArsenalStatics::GetNarrativePlayerCharacter(this, 0);
						}
					}
					else
					{
						Char = CharSS->FindCharacterByID(Bind.Key);
					}
					
					if (Char)
					{
						SetBindingByTag(Bind.Key, { Char });
						
						//Bind the character visual into the cinematic using pattern 'CharNameVisual'
						if (ANarrativeCharacterVisual* CharVis = Char->GetCharacterVisual())
						{
							FName VisualBindingTag = FName(Bind.Key.ToString() + "Visual");
							SetBindingByTag(VisualBindingTag, { CharVis });
						}
						else
						{
							//If any characters visual is still pending, bind so we can try rebind again once it is loaded. 
							Char->CharacterVisualInitialized.AddUniqueDynamic(this, &ANarrativeLevelSequenceActor::BindingVisualReady);
							WaitingVisuals.AddUnique(Char);
						}

						//Bind the characters attach parent, usually a mount, using pattern 'CharNameMount'
						if (AActor* CharMount = Char->GetAttachParentActor())
						{
							FName MountBindingTag = FName(Bind.Key.ToString() + "Mount");
							SetBindingByTag(MountBindingTag, { CharMount });
						}
			
						//Bind the characters owning controller, using pattern 'CharNameController'
						if (AActor* CharController = Char->GetController())
						{
							FName ControllerBindingTag = FName(Bind.Key.ToString() + "Controller");
							SetBindingByTag(ControllerBindingTag, { CharController });
						}
					}
				}

			}
		}
	}
	
}

bool ANarrativeLevelSequenceActor::ParticipantsReady() const
{
	if (!GetSequencePlayer() || !GetSequencePlayer()->GetSequence()) { return false; }
	const UMovieScene* Scene = GetSequencePlayer()->GetSequence()->GetMovieScene();
	if (!Scene || NarrativeSequenceParams.RequiredParticipantBindingTags.Num() > 32) { return false; }
	const auto Tagged = Scene->AllTaggedBindings();
	TSet<FName> Seen;
	for (FName Tag : NarrativeSequenceParams.RequiredParticipantBindingTags)
	{
		if (Tag.IsNone() || Seen.Contains(Tag)) { return false; }
		Seen.Add(Tag);
		const auto* Bindings = Tagged.Find(Tag);
		if (!Bindings || Bindings->IDs.IsEmpty()) { return false; }
		for (const auto& Binding : Bindings->IDs)
		{
			bool bFound = false;
			for (const auto& Object : GetSequencePlayer()->FindBoundObjects(Binding.GetGuid(), MovieSceneSequenceID::Root))
			{
				if (!IsValid(Object.Get())) { continue; }
				if (const auto* PlayerCharacter = Cast<ANarrativePlayerCharacter>(Object.Get()); PlayerCharacter && !PlayerCharacter->IsCharacterReady()) { return false; }
				if (const auto* Character = Cast<ANarrativeCharacter>(Object.Get()))
				{
					const auto* ASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character));
					const auto* Visual = Character->GetCharacterVisual();
					if (!ASC || ASC->GetAvatarActor() != Character || !ASC->bInitializedFromConfig || !Visual || !Visual->bBaseAppearanceLoaded) { return false; }
				}
				bFound = true;
			}
			if (!bFound) { return false; }
		}
	}
	for (UObject* Object : GetBoundObjects())
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			if (Actor->IsActorBeingDestroyed()) { return false; }
			if (const auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
			{ if (ASC->HasAnyMatchingGameplayTags(NarrativeSequenceParams.StopTags)) { return false; } }
		}
	}
	return true;
}

bool ANarrativeLevelSequenceActor::AcquireParticipantOwnership(uint64 ExpectedEpoch)
{
	const FGameplayTagContainer RequestedTags = NarrativeSequenceParams.TagsToApplyWhilstBound;
	for (UObject* Object : GetBoundObjects())
	{
		AActor* Actor = Cast<AActor>(Object);
		auto* ASC = Actor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor) : nullptr;
		if (!IsValid(ASC)) { continue; }
		for (FGameplayTag Tag : RequestedTags)
		{
			if (OwnershipEpoch != ExpectedEpoch || !bSessionActive || !CanAcceptPlayback() || IsActorBeingDestroyed()) { return false; }
			if (const auto* Existing = OwnedParticipantTags.Find(ASC); Existing && Existing->HasTagExact(Tag)) { continue; }
			// Record one exact tag before its callback-producing write. Never remove a container of tags not yet applied.
			OwnedParticipantTags.FindOrAdd(ASC).AddTag(Tag);
			ASC->AddLooseGameplayTag(Tag, 1, EGameplayTagReplicationState::CountToOwner);
			if (OwnershipEpoch != ExpectedEpoch || !bSessionActive || bIsEndingPlay || IsActorBeingDestroyed()) { return false; }
		}
	}
	return true;
}

void ANarrativeLevelSequenceActor::ReleaseParticipantOwnership()
{
	const auto Previous = MoveTemp(OwnedParticipantTags);
	OwnedParticipantTags.Reset();
	for (const auto& Pair : Previous)
	{ if (auto* ASC = Pair.Key.Get()) { ASC->RemoveLooseGameplayTags(Pair.Value, 1, EGameplayTagReplicationState::CountToOwner); } }
	for (const auto& Character : WaitingVisuals)
	{ if (Character.IsValid()) { Character->CharacterVisualInitialized.RemoveDynamic(this, &ANarrativeLevelSequenceActor::BindingVisualReady); } }
	WaitingVisuals.Reset();
}

void ANarrativeLevelSequenceActor::OnPlay()
{
	if (!CanAcceptPlayback() || bBlendingOut)
	{
		// A direct player Play() can arrive from a tag/controller teardown callback.
		// Reject the engine playback itself, not merely the ownership notification.
		if (auto* Player = GetSequencePlayer()) { Player->Stop(); }
		return;
	}
	RefreshBindings();
	if (!ParticipantsReady()) { FailPlayback(); return; }
	bPendingPlayback = false;
	const bool bWasActive = bSessionActive;
	bSessionActive = true;
	const uint64 Epoch = OwnershipEpoch;
	if (!AcquireParticipantOwnership(Epoch) || OwnershipEpoch != Epoch || !bSessionActive) { return; }
	if (bWasActive) { return; }
	ActiveNotificationSettings = NarrativeSequenceParams;
	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (auto* PC = Cast<ANarrativePlayerController>(Iterator->Get()); PC && (PC->IsLocalController() || PC->HasAuthority())
				&& (OwnerControllers.IsEmpty() || OwnerControllers.Contains(PC)))
			{
				NotifiedControllers.AddUnique(PC);
				PC->LevelSequencePlayed(this, ActiveNotificationSettings);
				if (OwnershipEpoch != Epoch || !bSessionActive || bIsEndingPlay || IsActorBeingDestroyed()) { return; }
			}
		}
	}
}

void ANarrativeLevelSequenceActor::OnStop()
{
	if (bEndingPlayback) { return; }
	TGuardValue<bool> Guard(bEndingPlayback, true);
	++OwnershipEpoch;
	bPendingPlayback = false;
	const bool bWasBlending = bBlendingOut;
	bBlendingOut = false;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(BlendOutTimer); }
	ReleaseParticipantOwnership();
	const auto PreviousControllers = MoveTemp(NotifiedControllers);
	NotifiedControllers.Reset();
	bSessionActive = false;
	for (const auto& Controller : PreviousControllers)
	{ if (Controller.IsValid()) { Controller->LevelSequenceStopped(this, ActiveNotificationSettings); } }
	if (bWasBlending) { OnBlendOutFinished.Broadcast(); }
}

void ANarrativeLevelSequenceActor::FailPlayback()
{
	if (bEndingPlayback) { return; }
	if (GetSequencePlayer()) { GetSequencePlayer()->Stop(); }
	OnStop();
	OnPlaybackFailed.Broadcast();
}

void ANarrativeLevelSequenceActor::PlaySequence()
{
	if (!CanAcceptPlayback()) { return; }
	if (!GetSequencePlayer() || !GetSequencePlayer()->GetSequence()) { FailPlayback(); return; }
	RefreshBindings();
	if (ParticipantsReady()) { bPendingPlayback = false; GetSequencePlayer()->Play(); }
	else
	{
		bPendingPlayback = true;
		PendingPlaybackSeconds = 0.f;
	}
}

void ANarrativeLevelSequenceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!CanAcceptPlayback() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }
	if (bPendingPlayback)
	{
		PendingPlaybackSeconds += DeltaSeconds;
		const float Timeout = FMath::IsFinite(NarrativeSequenceParams.ParticipantReadyTimeoutSeconds)
			? FMath::Clamp(NarrativeSequenceParams.ParticipantReadyTimeoutSeconds, .1f, 30.f) : .1f;
		RefreshBindings();
		if (ParticipantsReady()) { bPendingPlayback = false; GetSequencePlayer()->Play(); }
		else if (PendingPlaybackSeconds >= Timeout) { FailPlayback(); }
	}
	else if (bSessionActive && !bBlendingOut)
	{
		ParticipantPollSeconds += DeltaSeconds;
		if (ParticipantPollSeconds >= .1f)
		{
			ParticipantPollSeconds = 0.f;
			if (!ParticipantsReady()) { FailPlayback(); }
		}
	}
}

void ANarrativeLevelSequenceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIsEndingPlay = true;
	++PlaybackGeneration;
	OnStop();
	if (GetSequencePlayer())
	{
		GetSequencePlayer()->OnPlay.RemoveDynamic(this, &ANarrativeLevelSequenceActor::OnPlay);
		GetSequencePlayer()->OnPlayReverse.RemoveDynamic(this, &ANarrativeLevelSequenceActor::OnPlay);
		GetSequencePlayer()->OnStop.RemoveDynamic(this, &ANarrativeLevelSequenceActor::OnStop);
		GetSequencePlayer()->OnFinished.RemoveDynamic(this, &ANarrativeLevelSequenceActor::OnStop);
	}
	Super::EndPlay(EndPlayReason);
}

void UAsyncAction_PlayNarrativeSequence::Activate()
{
	auto* Actor = LevelSequenceActor.Get();
	if (!IsValid(Actor) || !Actor->CanAcceptPlayback() || !Sequence.IsValid() || !Actor->GetSequencePlayer()) { Interrupted(); return; }
	auto* Player = Actor->GetSequencePlayer();
	// Stop the previous session before subscribing to this request's terminal events.
	const uint64 PreviousGeneration = Actor->GetPlaybackGeneration();
	if (Player->IsPlaying() || Player->IsPaused()) { Player->Stop(); }
	if (!LevelSequenceActor.IsValid() || Actor->IsActorBeingDestroyed() || !Actor->CanAcceptPlayback()
		|| Actor->GetSequencePlayer() != Player || Actor->GetPlaybackGeneration() != PreviousGeneration) { Interrupted(); return; }
	Player->OnFinished.AddUniqueDynamic(this, &UAsyncAction_PlayNarrativeSequence::Finished);
	Player->OnStop.AddUniqueDynamic(this, &UAsyncAction_PlayNarrativeSequence::Stopped);
	Actor->OnPlaybackFailed.AddUniqueDynamic(this, &UAsyncAction_PlayNarrativeSequence::Interrupted);
	Actor->OnDestroyed.AddUniqueDynamic(this, &UAsyncAction_PlayNarrativeSequence::ActorDestroyed);
	ExpectedPlaybackGeneration = Actor->GetPlaybackGeneration() + 1;
	Actor->UpdateSequence(Sequence.Get(), PlaybackSettings);
}

UAsyncAction_PlayNarrativeSequence* UAsyncAction_PlayNarrativeSequence::PlayNarrativeSequence(ANarrativeLevelSequenceActor* SequenceActor, ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings InSettings)
{
	if (!IsValid(SequenceActor) || !IsValid(LevelSequence)) { return nullptr; }
	auto* Action = NewObject<UAsyncAction_PlayNarrativeSequence>();
	Action->LevelSequenceActor = SequenceActor; Action->Sequence = LevelSequence; Action->PlaybackSettings = InSettings;
	Action->RegisterWithGameInstance(SequenceActor); return Action;
}

void UAsyncAction_PlayNarrativeSequence::Finished()
{
	const auto* Actor = LevelSequenceActor.Get();
	FinishAction(Actor && Actor->GetPlaybackGeneration() == ExpectedPlaybackGeneration);
}
void UAsyncAction_PlayNarrativeSequence::Interrupted() { FinishAction(false); }
void UAsyncAction_PlayNarrativeSequence::ActorDestroyed(AActor* Actor) { Interrupted(); }
void UAsyncAction_PlayNarrativeSequence::Stopped()
{
	// Natural finish can also emit a stop in the same engine call. Resolve cancellation after its finish delegate.
	if (auto* Actor = LevelSequenceActor.Get(); Actor && Actor->GetWorld())
	{
		Actor->GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { Interrupted(); }));
	}
	else { Interrupted(); }
}
void UAsyncAction_PlayNarrativeSequence::FinishAction(bool bSuccessful)
{
	if (bCompleted) { return; }
	bCompleted = true;
	if (auto* Actor = LevelSequenceActor.Get())
	{
		if (auto* Player = Actor->GetSequencePlayer())
		{
			Player->OnFinished.RemoveDynamic(this, &UAsyncAction_PlayNarrativeSequence::Finished);
			Player->OnStop.RemoveDynamic(this, &UAsyncAction_PlayNarrativeSequence::Stopped);
		}
		Actor->OnPlaybackFailed.RemoveDynamic(this, &UAsyncAction_PlayNarrativeSequence::Interrupted);
		Actor->OnDestroyed.RemoveDynamic(this, &UAsyncAction_PlayNarrativeSequence::ActorDestroyed);
	}
	if (bSuccessful) { OnFinished.Broadcast(); } else { OnInterrupted.Broadcast(); }
	SetReadyToDestroy();
}

void UAsyncAction_BlendOutNarrativeSequence::Activate()
{
	auto* Actor = LevelSequenceActor.Get();
	if (!IsValid(Actor) || !Actor->CanAcceptPlayback()) { FinishAction(false); return; }
	Actor->OnBlendOutFinished.AddUniqueDynamic(this, &UAsyncAction_BlendOutNarrativeSequence::Finished);
	Actor->OnDestroyed.AddUniqueDynamic(this, &UAsyncAction_BlendOutNarrativeSequence::ActorDestroyed);
	Actor->BlendOutAndStop();
}
UAsyncAction_BlendOutNarrativeSequence* UAsyncAction_BlendOutNarrativeSequence::BlendOutNarrativeSequence(ANarrativeLevelSequenceActor* SequenceActor)
{
	if (!IsValid(SequenceActor)) { return nullptr; }
	auto* Action = NewObject<UAsyncAction_BlendOutNarrativeSequence>();
	Action->LevelSequenceActor = SequenceActor; Action->RegisterWithGameInstance(SequenceActor); return Action;
}
void UAsyncAction_BlendOutNarrativeSequence::Finished() { FinishAction(true); }
void UAsyncAction_BlendOutNarrativeSequence::ActorDestroyed(AActor* Actor) { FinishAction(false); }
void UAsyncAction_BlendOutNarrativeSequence::FinishAction(bool bSuccessful)
{
	if (bCompleted) { return; }
	bCompleted = true;
	if (auto* Actor = LevelSequenceActor.Get())
	{
		Actor->OnBlendOutFinished.RemoveDynamic(this, &UAsyncAction_BlendOutNarrativeSequence::Finished);
		Actor->OnDestroyed.RemoveDynamic(this, &UAsyncAction_BlendOutNarrativeSequence::ActorDestroyed);
	}
	if (bSuccessful) { OnFinished.Broadcast(); } else { OnInterrupted.Broadcast(); }
	SetReadyToDestroy();
}
