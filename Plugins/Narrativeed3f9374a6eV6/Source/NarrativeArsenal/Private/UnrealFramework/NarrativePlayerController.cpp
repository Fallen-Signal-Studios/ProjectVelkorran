// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeCheatManager.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "UnrealFramework/NarrativeGameState.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Navigation/NarrativeNavigationComponent.h"
#include <EnhancedInputComponent.h>
#include <CommonInputSubsystem.h>
#include <Engine/Canvas.h>
#include <DisplayDebugHelpers.h>
#include "ArsenalStatics.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/NarrativeGameplayHUD.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "NarrativeGameplayTags.h"
#include "Camera/NarrativeCameraSystemActor.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/InputSettings.h"
#include "Settings/NarrativeInputSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraModifier.h"
#include "UnrealFramework/NarrativeGameMode.h"

ANarrativePlayerController::ANarrativePlayerController(const class FObjectInitializer& ObjectInitializer)
{
	InteractionComponent = CreateDefaultSubobject<UPlayerInteractionComponent>(TEXT("InteractionComponent"));

	//TODO dont default subobject these and let users add at their own whim 
	NavigationComponent = CreateDefaultSubobject<UNarrativeNavigationComponent>(TEXT("NavigationComponent"));
	TalesComponent = CreateDefaultSubobject<UTalesComponent>(TEXT("TalesComponent"));

	CheatClass = UNarrativeCheatManager::StaticClass();

	auto DefaultMappingsFinder = ConstructorHelpers::FObjectFinder<UNarrativeAbilityInputMapping>(TEXT("/Script/NarrativeArsenal.NarrativeAbilityInputMapping'/NarrativePro/Pro/Core/Data/Input/DA_DefaultAbilityInputs.DA_DefaultAbilityInputs'"));

	if (DefaultMappingsFinder.Succeeded())
	{
		AbilityInputMappings = DefaultMappingsFinder.Object;
	}

}

void ANarrativePlayerController::HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	if (bIsDead)
	{
		if (InteractionComponent)
		{
			InteractionComponent->StopInteractBehavior(false);
		}
	}

	RefreshGameplayMappingContext();

}

void ANarrativePlayerController::BeginPlay()
{
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	EnableCheats();
#endif
}

void ANarrativePlayerController::OnPossess(APawn* InPawn)
{

	//We cache this because GetPawn() can change if we get into cars etc.
	if (ANarrativePlayerCharacter* OwnedChar = Cast<ANarrativePlayerCharacter>(InPawn))
	{
		SetOwnedCharacter(OwnedChar);
	}

	Super::OnPossess(InPawn);

	if (ANarrativePlayerState* PS = GetPlayerState<ANarrativePlayerState>())
	{
		if (UNarrativeAbilitySystemComponent* NASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent()))
		{
			NASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
		}
	}

	RefreshGameplayReadiness();

}

void ANarrativePlayerController::OnUnPossess()
{
	Super::OnUnPossess();
	RefreshGameplayMappingContext();

	//UE by default wants to set our view target to our player controller on unpossess, but GameplayCameraSystem requires that it is set to our player
}

void ANarrativePlayerController::AutoManageActiveCameraTarget(AActor* SuggestedTarget)
{
	Super::AutoManageActiveCameraTarget(SuggestedTarget);

	//In Narrative, we keep the gameplay camera on the character at all times. Make sure when we auto-manage, we keep focus on the character
	if (ANarrativeCharacter* OwnedChar = GetNarrativeCharacter())
	{
		SetViewTarget(OwnedChar);
	}
}

void ANarrativePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	//We cache this because GetPawn() can change if we get into cars etc.
	if (ANarrativePlayerCharacter* OwnedChar = Cast<ANarrativePlayerCharacter>(GetPawn()))
	{
		SetOwnedCharacter(OwnedChar);
	}

	if (ANarrativePlayerState* PS = GetPlayerState<ANarrativePlayerState>())
	{
		// Init ASC with PS (Owner) and our new Pawn (AvatarActor)
		if (UNarrativeAbilitySystemComponent* NASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent()))
		{
			NASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
		}
	}

	RefreshGameplayReadiness();
}

void ANarrativePlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	if (ANarrativePlayerCharacter* OwnedChar = Cast<ANarrativePlayerCharacter>(GetPawn()))
	{
		SetOwnedCharacter(OwnedChar);
	}

	// Vehicle and other non-character possession must also remove the gameplay
	// mapping context while preserving the cached on-foot character.
	RefreshGameplayReadiness();
}

void ANarrativePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		//Looking
		if (IsValid(LookAction))
		{
			EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ANarrativePlayerController::Look);
		}

		//Abilities will have all been granted, but we need to bind them to input! We bind them in PC
		//so we can activate abilities regardless of our controlled pawn. 
		if (AbilityInputMappings)
		{
			for (auto& IA : AbilityInputMappings->InputAbilities)
			{
				if (IsValid(IA.InputAction))
				{
					EnhancedInput->BindAction(IA.InputAction, ETriggerEvent::Started, this, &ANarrativePlayerController::AbilityInputPressed, IA.InputTag);
					EnhancedInput->BindAction(IA.InputAction, ETriggerEvent::Completed, this, &ANarrativePlayerController::AbilityInputReleased, IA.InputTag);
					EnhancedInput->BindAction(IA.InputAction, ETriggerEvent::Canceled, this, &ANarrativePlayerController::AbilityInputReleased, IA.InputTag);
				}
			}
		}

		bInputBindingsInstalled = true;
	}

	RefreshGameplayReadiness();
}

void ANarrativePlayerController::SetCinematicMode(bool bInCinematicMode, bool bHidePlayer, bool bAffectsHUD, bool bAffectsMovement, bool bAffectsTurning)
{
	Super::SetCinematicMode(bInCinematicMode, bHidePlayer, bAffectsHUD, bAffectsMovement, bAffectsTurning);

	//Turn off any camera modifiers that are running 
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ForEachCameraModifier([&](UCameraModifier* CameraModifier)
			{
				bool bContinue = true;

				// Apply camera modification and output into DesiredCameraOffset/DesiredCameraRotation
				if ((CameraModifier != NULL))
				{
					if (bInCinematicMode)
					{
						CameraModifier->DisableModifier(true);
					}
					else
					{
						CameraModifier->EnableModifier();
					}
				}

				return bContinue;
			});

	}

	//Default UE implementation doesnt disable all input, when generally its rare we'd ever want to disable movement input but keep all other inputs. 
	if (bInCinematicMode)
	{
		if (bAffectsMovement)
		{
			DisableInput(this);
		}
		else
		{
			EnableInput(this);
		}
	}
	else
	{
		EnableInput(this);
	}

	RefreshGameplayMappingContext();

	// Default UE implementation doesn't hide any actors attached to our pawn, when possessing vehicle/mount this results in floating player. Fix that. 
	if (APawn* OurPawn = GetPawn())
	{
		// Only hide the pawn if in cinematic mode and we want to
		if (bCinematicMode && bHidePawnInCinematicMode)
		{
			TArray<AActor*> Attached;
			OurPawn->GetAttachedActors(Attached, true, true);
			Attached.Add(OurPawn);

			for (auto& Attach : Attached)
			{
				if (Attach)
				{
					Attach->SetActorHiddenInGame(true);
				}
			}
		}
		// Always safe to show the pawn when not in cinematic mode
		else if (!bCinematicMode)
		{
			TArray<AActor*> Attached;
			OurPawn->GetAttachedActors(Attached, true, true);
			Attached.Add(OurPawn);

			for (auto& Attach : Attached)
			{
				if (Attach)
				{
					Attach->SetActorHiddenInGame(false);
				}
			}
		}
	}
}

void ANarrativePlayerController::Look(const FInputActionValue& Value)
{
	//TODO - needs moved to playercontroller so sensitivity is globally applied. 
	FVector2D LookAxisInput = Value.Get<FVector2D>();
	const UInputSettings* DefaultInputSettings = GetDefault<UInputSettings>();

	float AimSensitivity = 1.f;

	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* EIS = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (UNarrativeInputSettings* IS = Cast<UNarrativeInputSettings>(EIS->GetUserSettings()))
			{
				if (IS->GetInvertVertical())
				{
					LookAxisInput.Y = -LookAxisInput.Y;
				}

				if (IS->GetInvertHorizontal())
				{
					LookAxisInput.X = -LookAxisInput.X;
				}

				AimSensitivity = IS->GetAimSensitivity();
			}
		}
	}

	//UE Look input scaling feature appears to be broken. Add it back in here.
	float const FOVScale = (DefaultInputSettings->bEnableFOVScaling && PlayerCameraManager) ? (DefaultInputSettings->FOVScale * PlayerCameraManager->GetFOVAngle()) : 1.0f;


	AddYawInput(LookAxisInput.X * FOVScale * AimSensitivity);
	AddPitchInput(LookAxisInput.Y * FOVScale * AimSensitivity);
}

void ANarrativePlayerController::ClientShowHUDNotification_Implementation(const FText& Message, const float Duration)
{
	if (GameplayHUD)
	{
		GameplayHUD->ShowNotification(Message, Duration);
	}
}

void ANarrativePlayerController::LevelSequencePlayed(ANarrativeLevelSequenceActor* SequenceActor, const FNarrativeSequencePlaybackSettings& InSettings)
{

	CurrentSequences.Add(SequenceActor);

	if (InSettings.bStopDialogue && TalesComponent && TalesComponent->IsInDialogue())
	{
		TalesComponent->ExitDialogue(EExitDialogueReason::EDR_StoppedByCinematic);
	}

	OnLevelSequencePlay.Broadcast(SequenceActor, InSettings);
}

void ANarrativePlayerController::LevelSequenceStopped(ANarrativeLevelSequenceActor* SequenceActor, const FNarrativeSequencePlaybackSettings& InSettings)
{
	CurrentSequences.Remove(SequenceActor);

	OnLevelSequenceStop.Broadcast(SequenceActor, InSettings);
}

class UAbilitySystemComponent* ANarrativePlayerController::GetAbilitySystemComponent() const
{
	if (ANarrativeCharacter* NChar = GetOwnedCharacter())
	{
		return NChar->GetAbilitySystemComponent();
	}

	return nullptr;
}

FGameplayTagContainer ANarrativePlayerController::GetFactions() const
{
	if (ANarrativePlayerState* PS = GetPlayerState<ANarrativePlayerState>())
	{
		return PS->GetFactions();
	}

	return FGameplayTagContainer::EmptyContainer;
}

void ANarrativePlayerController::AddFaction(const FGameplayTag& Faction)
{
	if (ANarrativePlayerState* PS = GetPlayerState<ANarrativePlayerState>())
	{
		return PS->AddFaction(Faction);
	}
}

void ANarrativePlayerController::RemoveFaction(const FGameplayTag& Faction)
{
	if (ANarrativePlayerState* PS = GetPlayerState<ANarrativePlayerState>())
	{
		return PS->RemoveFaction(Faction);
	}
}

ETeamAttitude::Type ANarrativePlayerController::GetTeamAttitudeTowards(const AActor& Other) const
{
	if (ANarrativePlayerState* PS = GetPlayerState<ANarrativePlayerState>())
	{
		return PS->GetTeamAttitudeTowards(Other);
	}

	return ETeamAttitude::Neutral;
}

void ANarrativePlayerController::PrepareForSave_Implementation()
{
	SavedControlRotation = GetControlRotation();
}

void ANarrativePlayerController::Load_Implementation()
{
	RespawnTethers();

	SetControlRotation(SavedControlRotation);
}

ANarrativeCharacter* ANarrativePlayerController::GetNarrativeCharacter() const
{
	if (ANarrativeCharacter* NChar = GetOwnedCharacter())
	{
		return NChar;
	}

	return Cast<ANarrativeCharacter>(GetPawn());
}

void ANarrativePlayerController::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor(255, 255, 0));
	
	
	if (DebugDisplay.IsDisplayOn("Factions"))
	{
		FGameplayTagContainer Factions = GetFactions();

		if (Factions.IsValid())
		{
			DisplayDebugManager.DrawString(FString::Printf(TEXT("Factions: %s"), *Factions.ToString()));
		}
		else
		{
			DisplayDebugManager.DrawString("Factions: none");
		}
	}

	if (DebugDisplay.IsDisplayOn("TimeOfDay"))
	{
		DisplayDebugManager.DrawString(FString::Printf(TEXT("Time: %s"), *UArsenalStatics::GetTimeOfDayAsString(this)));

		FString TimeEvents;

		if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
		{
			for (auto& TODEvent : GS->TimeOfDayEvents)
			{
				TimeEvents += FString::Printf(TEXT("%f, "), TODEvent.EventTime);
			}
		}

		DisplayDebugManager.DrawString(TimeEvents);
	}

	if (DebugDisplay.IsDisplayOn("Tethers"))
	{
		FString TethersString;

		if (ANarrativeGameState* GS = Cast<ANarrativeGameState>(GetWorld()->GetGameState()))
		{
			for (auto& Tether : NPCTethers)
			{
				if (IsValid(Tether.NPCDef))
				{
					TethersString += FString::Printf(TEXT("%s, "), *Tether.NPCDef->NPCName.ToString());
				}
			}

			DisplayDebugManager.DrawString(FString::Printf(TEXT("Tethers (%d): %s"), NPCTethers.Num(), *TethersString));
		}
	}

	if (DebugDisplay.IsDisplayOn("Cinematics"))
	{
		FString Sequences;

		DisplayDebugManager.DrawString(FString::Printf(TEXT("NARRATIVE SEQUENCES (%d running)"), CurrentSequences.Num()));
		for (auto& Sequence : CurrentSequences)
		{
			if (Sequence.IsValid())
			{

			}
		}
	}
}

bool ANarrativePlayerController::IsLookInputIgnored() const
{
	if (HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Player_IgnoreLookInput))
	{
		return true; 
	}

	/* Unfortunately for the time being in networked games, first person clients cannot rotate during attacks.
	 *
	 * If we want this, we would have to allow the client camera to look around 
	 */
	if (HasMatchingGameplayTag(FNarrativeGameplayTags::Get().Camera_FirstPerson_CameraInsideHead))
	{
		if (OwnedCharacter)
		{
			//return OwnedCharacter->IsPlayingNetworkedRootMotionMontage();
		}
	}
	
	return Super::IsLookInputIgnored();
}

void ANarrativePlayerController::PawnLeavingGame()
{
	
	
	Super::PawnLeavingGame();
}

void ANarrativePlayerController::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (OwnedCharacter)
	{
		OwnedCharacter->GetOwnedGameplayTags(TagContainer);
	}

}

bool ANarrativePlayerController::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if (OwnedCharacter)
	{
		return OwnedCharacter->HasMatchingGameplayTag(TagToCheck);
	}
	return true; 
}

bool ANarrativePlayerController::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (OwnedCharacter)
	{
		return OwnedCharacter->HasAllMatchingGameplayTags(TagContainer);
	}
	return true;
}

bool ANarrativePlayerController::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if (OwnedCharacter)
	{
		return OwnedCharacter->HasAnyMatchingGameplayTags(TagContainer);
	}
	return true;
}

FString ANarrativePlayerController::GetNarrativeInputDeviceName() const
{
	if(UCommonInputSubsystem* CIS = UCommonInputSubsystem::Get(GetLocalPlayer()))
	{
		ECommonInputType InputType = CIS->GetCurrentInputType();

		if (InputType == ECommonInputType::MouseAndKeyboard)
		{
			return "Keyboard";
		}
		else if (InputType == ECommonInputType::Gamepad)
		{
			const FName GPName = CIS->GetCurrentGamepadName();

			if (GPName.IsEqual("Generic"))
			{
				return "Xbox";
			}
			else
			{
				return GPName.ToString();
			}
		}
		else if (InputType == ECommonInputType::Touch)
		{
			return "Touch";
		}
		else if (InputType == ECommonInputType::Count)
		{
			return "Count";
		}
	}

	return "None";
}

bool ANarrativePlayerController::IsUsingGamepad() const
{
	if(UCommonInputSubsystem* CIS = UCommonInputSubsystem::Get(GetLocalPlayer()))
	{
		ECommonInputType InputType = CIS->GetCurrentInputType();

		return InputType == ECommonInputType::Gamepad;
	}

	return false;
}

void ANarrativePlayerController::NotifyDealtDamage_Implementation(AActor* DamagedActor, const float DamgeAmount)
{
	HandleDamageActor(DamagedActor, DamgeAmount);
}

void ANarrativePlayerController::SetOwnedCharacter(ANarrativePlayerCharacter* InCharacter)
{
	if (OwnedCharacter == InCharacter)
	{
		RefreshGameplayReadiness();
		return;
	}

	// Release through the old character's ASC before replacing the pointer.
	// Otherwise a held guard during respawn can be released on the new pawn and
	// leave the old ability active.
	if (!PressedAbilityInputTags.IsEmpty())
	{
		ReleaseHeldAbilityInputs();
	}

	if (IsValid(OwnedCharacter))
	{
		OwnedCharacter->OnCharacterReadinessChanged.RemoveDynamic(
			this,
			&ThisClass::HandleOwnedCharacterReadinessChanged);
	}

	OwnedCharacter = InCharacter;
	if (IsValid(OwnedCharacter))
	{
		OwnedCharacter->OnCharacterReadinessChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleOwnedCharacterReadinessChanged);
	}

	RefreshGameplayReadiness();
}

void ANarrativePlayerController::HandleOwnedCharacterReadinessChanged(
	ANarrativePlayerCharacter* ReadyCharacter,
	const bool bIsReady)
{
	static_cast<void>(bIsReady);
	if (ReadyCharacter == OwnedCharacter)
	{
		RefreshGameplayReadiness();
	}
}

void ANarrativePlayerController::RefreshGameplayReadiness()
{
	if (!IsLocalPlayerController() || !IsValid(OwnedCharacter) || !OwnedCharacter->IsCharacterReady())
	{
		RefreshGameplayMappingContext();
		return;
	}

	EnsureGameplayHUDCreated();
	RefreshGameplayMappingContext();
}

void ANarrativePlayerController::EnsureGameplayHUDCreated()
{
	if (!IsLocalPlayerController() || IsValid(GameplayHUD) || !IsValid(GameplayHUDClass))
	{
		return;
	}

	GameplayHUD = CreateWidget<UNarrativeGameplayHUD>(this, GameplayHUDClass);
	if (IsValid(GameplayHUD))
	{
		GameplayHUD->AddToViewport();
	}
}

void ANarrativePlayerController::RefreshGameplayMappingContext()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer)
		: nullptr;
	if (!Subsystem || !IsValid(DefaultMappingContext))
	{
		if (bGameplayMappingContextApplied)
		{
			ReleaseHeldAbilityInputs();
		}
		bGameplayMappingContextApplied = false;
		return;
	}

	const bool bShouldApply = IsLocalPlayerController()
		&& bInputBindingsInstalled
		&& IsValid(OwnedCharacter)
		&& OwnedCharacter->IsCharacterReady()
		&& GetPawn() == OwnedCharacter
		&& !bCinematicMode
		&& !OwnedCharacter->HasMatchingGameplayTag(
			FNarrativeGameplayTags::Get().State_IsDead);

	if (bShouldApply && !bGameplayMappingContextApplied)
	{
		FModifyContextOptions ModifyOptions;
		ModifyOptions.bNotifyUserSettings = true;
		Subsystem->AddMappingContext(DefaultMappingContext, 0, ModifyOptions);
		bGameplayMappingContextApplied = true;
	}
	else if (!bShouldApply && bGameplayMappingContextApplied)
	{
		ReleaseHeldAbilityInputs();
		Subsystem->RemoveMappingContext(DefaultMappingContext);
		bGameplayMappingContextApplied = false;
	}
}

class ANarrativePlayerCharacter* ANarrativePlayerController::GetOwnedCharacter() const
{
	return OwnedCharacter;
}

class ANarrativePlayerCharacter* ANarrativePlayerController::GetControlledCharacter() const
{
	return Cast<ANarrativePlayerCharacter>(GetPawn());
}

void ANarrativePlayerController::TryRespawn()
{
	if (!HasAuthority())
	{
		ServerTryRespawn();
	}

	HandleRespawn();
}

void ANarrativePlayerController::HandleRespawn_Implementation()
{
	if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		ASC->bStartupEffectsApplied = false; 
		ASC->Revive();
	}

	//Ask the game mode to restart us. RestartPlayer and similar funcs are overridable for more game specific behavior. 
	if (ANarrativeGameMode* NGM = Cast<ANarrativeGameMode>(GetWorld()->GetAuthGameMode()))
	{
		NGM->RestartPlayer(this);
	}
}

void ANarrativePlayerController::ServerTryRespawn_Implementation()
{
	TryRespawn();
}

bool ANarrativePlayerController::TetherNPC(ANarrativeNPCCharacter* NPCToTether)
{
	if (NPCToTether && NPCToTether->Implements<UNarrativeSavableActor>())
	{
		const FGuid NPCSaveGUID = INarrativeSavableActor::Execute_GetActorGUID(NPCToTether);

		//Empty GUID means this NPC is transient and doesnt need saved 
		if (!NPCSaveGUID.IsValid())
		{
			return false;
		}

		//Ensure a tether with the same GUID doesn't already exist 
		for (auto& Tether : NPCTethers)
		{
			if (Tether.NPCSaveGUID == NPCSaveGUID)
			{
				return false;
			}
		}

		FNPCTether NPCTether;
		NPCTether.NPCCharacter = NPCToTether;
		NPCTether.NPCSaveGUID = NPCSaveGUID;
		NPCTether.NPCDef = NPCToTether->GetNPCDefinition();

		checkf(NPCTether.NPCSaveGUID.IsValid(), TEXT("NPC to tether had none Save GUID - this shouldn't happen."));

		UE_LOG(LogTemp, Verbose, TEXT("NPC %s tethered to player controller"), *GetNameSafe(NPCToTether));

		NPCTethers.Add(NPCTether);

		NPCToTether->OnDestroyed.AddUniqueDynamic(this, &ANarrativePlayerController::OnTetheredNPCDestroyed);

		return true;
	}

	return false; 
}

bool ANarrativePlayerController::UntetherNPC(ANarrativeNPCCharacter* NPCToUntether)
{
	if (NPCToUntether && NPCToUntether->Implements<UNarrativeSavableActor>())
	{
		const FGuid NPCSaveGUID = INarrativeSavableActor::Execute_GetActorGUID(NPCToUntether);

		for (int32 i = NPCTethers.Num() - 1; i >= 0; --i)
		{
			if (NPCTethers[i].NPCSaveGUID == NPCSaveGUID)
			{
				UE_LOG(LogTemp, Verbose, TEXT("NPC %s untethered from player controller"), *GetNameSafe(NPCTethers[i].NPCDef));

				NPCToUntether->OnDestroyed.RemoveDynamic(this, &ANarrativePlayerController::OnTetheredNPCDestroyed);

				NPCTethers.RemoveAt(i);
				return true; 
			}
		}
	}

	return false; 
}

bool ANarrativePlayerController::GetTether(const FGuid& NPCToCheckGUID, FNPCTether& OutTether) const
{
	for (int32 i = NPCTethers.Num() - 1; i >= 0; --i)
	{
		if (NPCTethers[i].NPCSaveGUID == NPCToCheckGUID)
		{
			OutTether = NPCTethers[i];
			return true;
		}
	}

	return false;
}

void ANarrativePlayerController::RespawnTethers()
{
	//Spawn our NPCs back in when we load
	for (auto& Tether : NPCTethers)
	{
		if (!IsValid(Tether.NPCCharacter) && IsValid(Tether.NPCDef) && Tether.NPCSaveGUID.IsValid())
		{
			if (UNarrativeCharacterSubsystem* NPCSub = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
			{
				ANarrativeNPCCharacter* NPCChar = NPCSub->SpawnNPC(Tether.NPCDef);
				Tether.NPCCharacter = NPCChar;

				if (NPCChar)
				{
					NPCChar->SpawnInfo.SpawnAssignedSaveGUID = Tether.NPCSaveGUID;
				}
			}
		}
	}
}

void ANarrativePlayerController::OnTetheredNPCDestroyed(AActor* DestroyedActor)
{
	if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(DestroyedActor))
	{
		UntetherNPC(NPC);
	}
}

void ANarrativePlayerController::AbilityInputPressed(FGameplayTag InputTag)
{
	if (InputTag.IsValid())
	{
		PressedAbilityInputTags.Add(InputTag);
	}

	if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		ASC->AbilityInputTagPressed(InputTag);
	}
}

void ANarrativePlayerController::AbilityInputReleased(FGameplayTag InputTag)
{
	PressedAbilityInputTags.Remove(InputTag);

	if (UNarrativeAbilitySystemComponent* ASC = Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent()))
	{
		ASC->AbilityInputTagReleased(InputTag);
	}
}

void ANarrativePlayerController::ReleaseHeldAbilityInputs()
{
	UNarrativeAbilitySystemComponent* ASC =
		Cast<UNarrativeAbilitySystemComponent>(GetAbilitySystemComponent());
	const TArray<FGameplayTag> HeldTags = PressedAbilityInputTags.Array();
	PressedAbilityInputTags.Empty();

	if (ASC)
	{
		for (const FGameplayTag& InputTag : HeldTags)
		{
			ASC->AbilityInputTagReleased(InputTag);
		}
	}
}
