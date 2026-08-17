// Copyright Narrative Tools 2024. 


#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "UnrealFramework/NarrativePlayerController.h"
#include "UnrealFramework/NarrativePlayerState.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GAS/NarrativeAbilityInputMapping.h"
#include "NarrativeArsenal.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/NarrativeCameraComponent.h"
#include "Navigation/NavigationMarkerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Navigation/NavigatorGameplayTags.h"
#include <UObject/ConstructorHelpers.h>
#include "Items/NarrativeItem.h"
#include <Engine/LocalPlayer.h>
#include "Net/UnrealNetwork.h"
#include "Character/PlayerDefinition.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Items/WeaponItem.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "CharacterCreator/NarrativeSaveWithCreatorData.h"
#include "UnrealFramework/NarrativeGameMode.h"
#include "ArsenalSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NarrativeGameplayTags.h"
#include "AI/NarrativeCharacterSubsystem.h"
#include "Character/NarrativeCharacterMovement.h"
#include "AI/NarrativeNPCController.h"
#include "GameFramework/InputSettings.h"
#include "Settings/NarrativeInputSettings.h"
#include "Sovereign/SovGameplayTags.h"
#include "Weapons/WeaponVisual.h"

#define LOCTEXT_NAMESPACE "NarrativePlayerCharacter"

ANarrativePlayerCharacter::ANarrativePlayerCharacter(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{


}

void ANarrativePlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ANarrativePlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	TryInitializePlayerCharacter();
}

void ANarrativePlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (ANarrativePlayerController* PlayerC = Cast<ANarrativePlayerController>(NewController))
	{
		CachedController = PlayerC;
	}

	TryInitializePlayerCharacter();
}

void ANarrativePlayerCharacter::OnRep_Controller()
{
	//Clients cache here as they do not recieve PossessedBy call 
	Super::OnRep_Controller();

	if (ANarrativePlayerController* PlayerC = Cast<ANarrativePlayerController>(Controller))
	{
		CachedController = PlayerC;
	}
	TryInitializePlayerCharacter();
}

class UAbilitySystemComponent* ANarrativePlayerCharacter::GetAbilitySystemComponent() const
{
	if (UAbilitySystemComponent* SuperASC = Super::GetAbilitySystemComponent())
	{
		return SuperASC;
	}
	else if(ANarrativePlayerState* NPS = GetNarrativePlayerState())
	{
		//If for whatever reason our base ASC pointer gets gargled this ensures we still safely return it. 
		return NPS->GetAbilitySystemComponent();
	}

	return nullptr; 
}

void ANarrativePlayerCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	TryInitializePlayerCharacter();
}

void ANarrativePlayerCharacter::TryInitializePlayerCharacter()
{
	// PlayerState and PlayerDefinition replicate independently. Either order is
	// legal, so initialization waits instead of asserting or polling.
	if (!IsValid(PlayerDefinition))
	{
		return;
	}

	ANarrativePlayerState* PS = GetNarrativePlayerState();
	if (!IsValid(PS))
	{
		return;
	}

	UNarrativeAbilitySystemComponent* NewASC = Cast<UNarrativeAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	UNarrativeAttributeSetBase* NewAttributeSet = PS->GetAttributeSetBase();
	if (!IsValid(NewASC) || !IsValid(NewAttributeSet))
	{
		return;
	}

	const bool bNewASCGeneration = InitializedAbilitySystem != NewASC;
	const bool bNeedsActorInfo = NewASC->GetOwnerActor() != PS || NewASC->GetAvatarActor() != this;
	AbilitySystemComponent = NewASC;
	AttributeSetBase = NewAttributeSet;

	if (bNeedsActorInfo)
	{
		NewASC->InitAbilityActorInfo(PS, this);
	}

	if (bNewASCGeneration)
	{
		InitializedAbilitySystem = NewASC;
		bCharacterReady = false;
		++CharacterInitializationGeneration;
		HandleAbilitySystemReady(NewASC);
		OnASCInitialized.Broadcast();
	}

	if (HasAuthority() && !bAuthoritativeGameplayInitialized)
	{
		// This block is deliberately once per pawn. Repeated RepNotifies and
		// possession callbacks must never reset resources or duplicate abilities.
		InitializeAttributes();
		SetHealth(GetMaxHealth());
		SetStamina(GetMaxStamina());
		AddStartupEffects();
		AddDefaultAbilities();
		bAuthoritativeGameplayInitialized = true;
	}

	if (InitializedPlayerDefinition != PlayerDefinition)
	{
		InitializedPlayerDefinition = PlayerDefinition;
		OnDefinitionSet(PlayerDefinition);
	}

	TryFinalizeCharacterReadiness();
}

void ANarrativePlayerCharacter::HandleAbilitySystemReady(UNarrativeAbilitySystemComponent* ReadyAbilitySystem)
{
	static_cast<void>(ReadyAbilitySystem);
}

bool ANarrativePlayerCharacter::AreAdditionalCharacterSystemsReady() const
{
	return true;
}

void ANarrativePlayerCharacter::TryFinalizeCharacterReadiness()
{
	if (bCharacterReady
		|| !IsValid(InitializedAbilitySystem)
		|| InitializedAbilitySystem->GetAvatarActor() != this
		|| InitializedPlayerDefinition != PlayerDefinition
		|| !bVisualReadyForGameplay
		|| (HasAuthority() && !bAuthoritativeGameplayInitialized)
		|| !AreAdditionalCharacterSystemsReady())
	{
		return;
	}

	if (HasAuthority() && !bAuthoritativeCharacterReady)
	{
		bAuthoritativeCharacterReady = true;
		ForceNetUpdate();
	}
	if (!bAuthoritativeCharacterReady)
	{
		return;
	}

	bCharacterReady = true;
	OnCharacterReady.Broadcast(this);

	if (HasAuthority())
	{
		FGameplayEventData Payload;
		Payload.EventTag = FSovGameplayTags::Get().Event_Character_Ready;
		Payload.Instigator = this;
		Payload.Target = this;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, Payload.EventTag, Payload);
	}
}

void ANarrativePlayerCharacter::OnRep_AuthoritativeCharacterReady()
{
	TryFinalizeCharacterReadiness();
}

void ANarrativePlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ANarrativePlayerCharacter, PlayerDefinition);
	DOREPLIFETIME(ANarrativePlayerCharacter, bAuthoritativeCharacterReady);
}

FGameplayTagContainer ANarrativePlayerCharacter::GetFactions() const
{
	//Access player state via controller as playerstate nulls if we're possessing a vehicle etc 
	if (ANarrativePlayerState* PS = GetNarrativePlayerState())
	{
		return PS->GetFactions();
	}

	return FGameplayTagContainer::EmptyContainer;
}

void ANarrativePlayerCharacter::AddFaction(const FGameplayTag& Faction)
{
	if (ANarrativePlayerState* PS = GetNarrativePlayerState())
	{
		return PS->AddFaction(Faction);
	}
}

void ANarrativePlayerCharacter::RemoveFaction(const FGameplayTag& Faction)
{
	if (ANarrativePlayerState* PS = GetNarrativePlayerState())
	{
		return PS->RemoveFaction(Faction);
	}
}

bool ANarrativePlayerCharacter::IsPlayerControlled() const
{
	return true;
}

bool ANarrativePlayerCharacter::IsBotControlled() const
{
	return false; 
}

FText ANarrativePlayerCharacter::GetCharacterName() const
{
	if (ANarrativePlayerState* PS = GetNarrativePlayerState())
	{
		return FText::FromString(PS->GetPlayerName());
	}

	return FText::GetEmpty();
}

void ANarrativePlayerCharacter::ApplyAppearance_Implementation(class UCharacterAppearance* DefaultAppearance)
{

	FString UsernameToApply = "";

	//TODO character creator data isn't replicated at this time 
	if (UNarrativeSaveWithCreatorData* CreatorData = GetCharacterCreatorData())
	{
		if (CharVisual)
		{
			CharVisual->InitializeFromCharacterAndAttributes(this, CreatorData->CharacterCreatorAttributes);
			UsernameToApply = CreatorData->CharacterCreatorUsername;
		}
	}
	else
	{
		Super::ApplyAppearance_Implementation(DefaultAppearance);

		//In networked we currently let clients pass a custom name to server when joining. 
		if (GetNetMode() == NM_Standalone)
		{
			//Change the players username to their appearances one. If people need something else they can override this. 
			if (UPlayerDefinition* PDef = GetPlayerDefinition())
			{
				if(!PDef->PlayerDisplayName.IsEmptyOrWhitespace())
				{
					UsernameToApply = PDef->PlayerDisplayName.ToString();
				}
				else
				{
					if (UArsenalSettings* Settings = GetMutableDefault<UArsenalSettings>())
					{
						if (Settings->DefaultUsername.Len())
						{
							UsernameToApply = Settings->DefaultUsername;
						}
					}
				}
			}


		}
	}
	
	if (ANarrativeGameMode* NGM = Cast<ANarrativeGameMode>(GetWorld()->GetAuthGameMode()))
	{
		NGM->ChangeName(GetController(), UsernameToApply, true);
	}
}

TSubclassOf<class ANarrativeCharacterVisual> ANarrativePlayerCharacter::GetCharacterVisualClass_Implementation(class UCharacterAppearance* DefaultAppearance) const
{
	if (UNarrativeSaveWithCreatorData* CreatorData = GetCharacterCreatorData())
	{
		return CreatorData->CharacterCreatorAttributes.CharacterVisualClass;
	}
	else
	{
		return Super::GetCharacterVisualClass_Implementation(DefaultAppearance);
	}
}

void ANarrativePlayerCharacter::OnCharacterVisualInitialized()
{
	if (!bInitialPlayerDataApplied)
	{
		// Save/load and initial equipment are part of readiness, and must run once.
		if (UNarrativeSaveSubsystem* SaveSub = GetWorld()->GetSubsystem<UNarrativeSaveSubsystem>())
		{
			if (GetNetMode() == NM_Standalone)
			{
				if (SaveSub->IsNewGame())
				{
					InitNewCharacter(GetCharacterDefinition());
				}
				else
				{
					SaveSub->LoadPlayerData();
				}
			}
			else
			{
				if (GetNetMode() == NM_Client)
				{
					InitNewCharacter(GetCharacterDefinition());
				}
				else if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer)
				{
					if (!SaveSub->LoadPlayerOnlySave(GetPlayerController()))
					{
						InitNewCharacter(GetCharacterDefinition());
					}
				}
			}
		}

		bInitialPlayerDataApplied = true;
	}

	if (!bVisualReadyForGameplay)
	{
		Super::OnCharacterVisualInitialized();
		bVisualReadyForGameplay = true;
	}
	TryFinalizeCharacterReadiness();
}

UNarrativeSaveWithCreatorData* ANarrativePlayerCharacter::GetCharacterCreatorData() const
{
	//Check if we have a creator appearance to apply 
	if (UWorld* World = GetWorld())
	{
		if (World->GetNetMode() == NM_Standalone)
		{
			if (UNarrativeSaveSubsystem* SaveSub = World->GetSubsystem<UNarrativeSaveSubsystem>())
			{
				if (UNarrativeSaveWithCreatorData* CreatorSave = Cast<UNarrativeSaveWithCreatorData>(SaveSub->GetSaveObject()))
				{
					//Only return the creator data if it actually has some stuff in it
					if (CreatorSave->CharacterCreatorAttributes.Meshes.Num())
					{
						return CreatorSave;
					}
				}
			}
		}
	}

	return nullptr;
}

class ANarrativePlayerController* ANarrativePlayerCharacter::GetPlayerController() const
{

	if (CachedController)
	{
		return CachedController;
	}

	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(GetController()))
	{
		return PC;
	}

	if (ANarrativePlayerController* PC = Cast<ANarrativePlayerController>(PreviousController))
	{
		return PC;
	}

	return nullptr;
}

class ANarrativePlayerState* ANarrativePlayerCharacter::GetNarrativePlayerState() const
{
	if (ANarrativePlayerController* PC = GetPlayerController())
	{
		if (ANarrativePlayerState* PState = PC->GetPlayerState<ANarrativePlayerState>())
		{
			return PState;
		}
	}

	return GetPlayerState<ANarrativePlayerState>();
}

AController* ANarrativePlayerCharacter::GetOwningController() const
{
	return GetPlayerController();
}


void ANarrativePlayerCharacter::Move(const FInputActionValue& Value)
{
	//input is a Vector2D
	MovementVector = Value.Get<FVector2D>();
     
	if (Controller != nullptr)
	{ 
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();

		FRotator YawRotation(0, Rotation.Yaw, 0);

		//Swimming needs pitch 
		if (GetCharacterMovement() && GetCharacterMovement()->IsSwimming())
		{
			YawRotation = FRotator(Rotation.Pitch, Rotation.Yaw, 0);
		}
		
		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ANarrativePlayerCharacter::CompletedMove()
{
	MovementVector = FVector2D::ZeroVector;	
}

void ANarrativePlayerCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		//Moving
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ANarrativePlayerCharacter::Move);
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ANarrativePlayerCharacter::CompletedMove);
	}
}

class UCharacterDefinition* ANarrativePlayerCharacter::GetCharacterDefinition() const
{
	return PlayerDefinition;
}

void ANarrativePlayerCharacter::OnRep_PlayerDefinition()
{
	if (PlayerDefinition)
	{
		//tell the character subsystem to add the char now that we know its chardef is set 
		if (UNarrativeCharacterSubsystem* NPCSubsystem = GetWorld()->GetSubsystem<UNarrativeCharacterSubsystem>())
		{
			NPCSubsystem->RegisterCharacter(this);
		}
	}

	TryInitializePlayerCharacter();
}

bool ANarrativePlayerCharacter::IsCameraInsideHead() const
{
	//Dedicated servers want to know if we're first person because of capsule rotation rules & executions, but otherwise non local clients are never inside the head. 
	const bool bLocalOrAuth = (HasAuthority() && GetNetMode() == NM_DedicatedServer) || (IsLocallyControlled() || IsLocallyViewed());
	
	return HasMatchingGameplayTag(FNarrativeGameplayTags::Get().Camera_FirstPerson_CameraInsideHead) && bLocalOrAuth;
}

bool ANarrativePlayerCharacter::ShouldCameraFollow3PHeadBoneLocation() const
{
	if(UNarrativeCharacterMovement* NCMC = GetNarrativeCharacterMovement())
	{
		if (UNarrativeAnimInstance* CharInst = GetCharacterAnimInstance())
		{
			if (CharInst->GetCurveValue(FName("Camera_Follow3PHeadLocation")) > 0.01f)
			{
				return true; 
			}
		}

		return HasMatchingGameplayTag(FNarrativeGameplayTags::Get().Camera_FirstPerson_Follow3PHeadLocation) || IsPlayingAttachWarpMontage || NCMC->IsClimbing();
	}
	return false;
}

void ANarrativePlayerCharacter::SetPlayerDefinition(class UPlayerDefinition* PDef)
{
	if(PDef)
	{
		PlayerDefinition = PDef;
		OnRep_PlayerDefinition();
	}
}
