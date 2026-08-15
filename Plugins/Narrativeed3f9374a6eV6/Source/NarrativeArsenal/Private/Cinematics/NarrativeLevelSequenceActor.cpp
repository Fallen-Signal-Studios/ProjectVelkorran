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
#include "../../../../Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/AbilitySystemGlobals.h"
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
		GetSequencePlayer()->Play();
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
							Objs.Add(Object.Get());
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
	//SetSequence doesn't work unless you do this 
	GetSequencePlayer()->Pause();

	SetSequence(LevelSequence);
	PlaybackSettings = InSettings;
	NarrativeSequenceParams = InSettings;
	GetSequencePlayer()->SetPlaybackSettings(InSettings);

	if (PlaybackSettings.bAutoPlay)
	{
		GetSequencePlayer()->Play();
	}
}

void ANarrativeLevelSequenceActor::BlendOutAndStop()
{

	for (auto& Object : GetBoundObjects())
	{
		if (USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Object))
		{
			if (UNarrativeAnimInstance* CharInst = Cast<UNarrativeAnimInstance>(Mesh->GetAnimInstance()))
			{
				CharInst->BlendOutOfSequencer();
			}
		}
		else if (AActor* Actor = Cast<AActor>(Object))
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
			{
				ASC->RemoveLooseGameplayTags(NarrativeSequenceParams.TagsToApplyWhilstBound, 1, EGameplayTagReplicationState::CountToOwner);
			}
		}
	}
	
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

void ANarrativeLevelSequenceActor::OnPlay()
{
	//Whenever we play, go through the bindings and make sure they are auto-bound to the characters the designer specified. 
	RefreshBindings();
	
	for (auto& Object : GetBoundObjects())
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
			{
				//UE_LOG(LogTemp, Warning, TEXT("%s adding tags %s"), *RoleStr, *NarrativeSequenceParams.TagsToApplyWhilstBound.ToString());
				ASC->AddLooseGameplayTags(NarrativeSequenceParams.TagsToApplyWhilstBound, 1, EGameplayTagReplicationState::CountToOwner);
			}
			//We don't do this because of cheating concerns but games requiring local cinematics will probably need a mechanism like this for stopping server correcting us 
			/*UE_LOG(LogTemp, Warning, TEXT("%s ALLOW %s CLIENT MOVE"), *RoleStr, *GetNameSafe(Actor));
			Actor->GetCharacterMovement()->bServerAcceptClientAuthoritativePosition = true;
			Actor->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true; */
		}
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(Iterator->Get()))
			{
				if (PC && (PC->IsLocalController() || PC->HasAuthority()))
				{
					PC->LevelSequencePlayed(this, NarrativeSequenceParams);
				}
			}
		}
	}

}

void ANarrativeLevelSequenceActor::OnStop()
{
	for (auto& Object : GetBoundObjects())
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
			{
				ASC->RemoveLooseGameplayTags(NarrativeSequenceParams.TagsToApplyWhilstBound, 1, EGameplayTagReplicationState::CountToOwner);
			}
			
			/*UE_LOG(LogTemp, Warning, TEXT("%s STOP %s CLIENT MOVE"), *RoleStr, *GetNameSafe(Actor));
			Actor->GetCharacterMovement()->bServerAcceptClientAuthoritativePosition = false;
			Actor->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false; */
		}
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(Iterator->Get()))
			{
				if (PC && PC->IsLocalController())
				{
					PC->LevelSequenceStopped(this, NarrativeSequenceParams);
				}
			}
		}
	}
}

void ANarrativeLevelSequenceActor::PlaySequence()
{
	GetSequencePlayer()->Play();
}

void UAsyncAction_PlayNarrativeSequence::Activate()
{
	if (ANarrativeLevelSequenceActor* LevelSequenceActorRef = LevelSequenceActor.Get())
	{
		LevelSequenceActorRef->UpdateSequence(Sequence.Get(), PlaybackSettings);
		LevelSequenceActorRef->GetSequencePlayer()->OnFinished.AddDynamic(this, &UAsyncAction_PlayNarrativeSequence::Finished);
	}
}

UAsyncAction_PlayNarrativeSequence* UAsyncAction_PlayNarrativeSequence::PlayNarrativeSequence(ANarrativeLevelSequenceActor* SequenceActor, ULevelSequence* LevelSequence, FNarrativeSequencePlaybackSettings InSettings)
{

	if (IsValid(SequenceActor) && IsValid(LevelSequence))
	{
		UAsyncAction_PlayNarrativeSequence* Action = NewObject<UAsyncAction_PlayNarrativeSequence>();
		Action->LevelSequenceActor = SequenceActor;
		Action->Sequence = LevelSequence;
		Action->PlaybackSettings = InSettings;
		Action->RegisterWithGameInstance(SequenceActor);

		return Action;
	}

	return nullptr;
}

void UAsyncAction_PlayNarrativeSequence::Finished()
{
	OnFinished.Broadcast();
}

void UAsyncAction_BlendOutNarrativeSequence::Activate()
{
	if (ANarrativeLevelSequenceActor* LevelSequenceActorRef = LevelSequenceActor.Get())
	{
		LevelSequenceActorRef->BlendOutAndStop();
		LevelSequenceActorRef->GetSequencePlayer()->OnPause.AddDynamic(this, &UAsyncAction_BlendOutNarrativeSequence::Finished);
	}
}

UAsyncAction_BlendOutNarrativeSequence* UAsyncAction_BlendOutNarrativeSequence::BlendOutNarrativeSequence(ANarrativeLevelSequenceActor* SequenceActor)
{
	if (IsValid(SequenceActor))
	{
		UAsyncAction_BlendOutNarrativeSequence* Action = NewObject<UAsyncAction_BlendOutNarrativeSequence>();
		Action->LevelSequenceActor = SequenceActor;
		Action->RegisterWithGameInstance(SequenceActor);

		return Action;
	}

	return nullptr;
}

void UAsyncAction_BlendOutNarrativeSequence::Finished()
{
	OnFinished.Broadcast();
}