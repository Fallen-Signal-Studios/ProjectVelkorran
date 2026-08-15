// Copyright Narrative Tools 2024. 

#include "Interaction/InteractionComponent.h"
#include "Interaction/InteractableComponent.h"
#include <GameFramework/PlayerController.h>
#include <GameFramework/Pawn.h>
#include <Engine/World.h>
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include <InputAction.h>
#include "UnrealFramework/NarrativeCharacter.h"
#include "GAS/NarrativeInteractAbility.h"
#include <AbilitySystemComponent.h>
#include "Subsystems/NarrativeSaveSubsystem.h"

#define LOCTEXT_NAMESPACE "InteractionComponent"



// Sets default values for this component's properties
UNarrativeInteractionComponent::UNarrativeInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;

	SetAutoActivate(true);
	SetIsReplicatedByDefault(true);

	OccupiedInteractableSlotIdx = -1;
}

void UNarrativeInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningController = Cast<AController>(GetOwner());

	if (OwningController)
	{
		OwningPawn = Cast<ANarrativeCharacter>(OwningController->GetPawn());
	}
}


void UNarrativeInteractionComponent::Load_Implementation()
{
	//Automatically resolve the GUID that points to our occupied interactable 
	if (OccupiedInteractableSoftOwner.IsValid() && OccupiedInteractableSlotIdx != -1)
	{
		if (AActor* InteractableActor = OccupiedInteractableSoftOwner.Get())
		{
			if (UNarrativeInteractableComponent* InteractableComp = Cast<UNarrativeInteractableComponent>(InteractableActor->GetComponentByClass(UNarrativeInteractableComponent::StaticClass())))
			{
				SetOccupiedInteractable(InteractableComp, OccupiedInteractableSlotIdx);
			}
		}
	}
}

void UNarrativeInteractionComponent::SetOccupiedInteractable(class UNarrativeInteractableComponent* Interactable, const int32 SlotIdx)
{
	if (Interactable)
	{
		OccupiedInteractable = Interactable;
		OccupiedInteractableSlotIdx = SlotIdx;

		//We use the interactables stable actor guid to restore the interactable when we load back in 
		if (AActor* InteractableOwner = OccupiedInteractable->GetOwner())
		{
			OccupiedInteractableSoftOwner = InteractableOwner;
			//OccupiedInteractableGUID = INarrativeStableActor::Execute_GetActorGUID(InteractableOwner);
		}
	}
	else
	{
		OccupiedInteractable = nullptr;
		OccupiedInteractableSlotIdx = -1;
		OccupiedInteractableSoftOwner = nullptr; 
	}
}

bool UNarrativeInteractionComponent::ClaimInteractionSlot(class UNarrativeInteractableComponent* Interactable, const int32 SlotIdx)
{
	if (Interactable)
	{
		//Free our slot if we have one already 
		if (InteractionSlotClaimHandle.IsValidHandle())
		{
			if (UNarrativeInteractableComponent* CurrentInteractable = InteractionSlotClaimHandle.HandleOwner.Get())
			{
				CurrentInteractable->UpdateSlotStatus(this, InteractionSlotClaimHandle.HandleIndex, EInteractionSlotStatus::ISS_Free);
			}
		}

		FInteractionSlotClaimHandle Handle = Interactable->ClaimSlot(this, SlotIdx);

		if (Handle.IsValidHandle())
		{
			InteractionSlotClaimHandle = Handle;
			return true;
		}
	}

	return false; 
}

void UNarrativeInteractionComponent::ReleaseInteractionSlot()
{
	//Free our slot - but make sure we actually still own the slot, as it may have been stolen. If it has, just invalidate our handle as we no longer own it anyway. 
	if (InteractionSlotClaimHandle.IsValidHandle())
	{
		if (UNarrativeInteractableComponent* Interactable = InteractionSlotClaimHandle.HandleOwner.Get())
		{
			if (Interactable->SlotStatuses[InteractionSlotClaimHandle.HandleIndex].SlotUser == this)
			{
				Interactable->UpdateSlotStatus(this, InteractionSlotClaimHandle.HandleIndex, EInteractionSlotStatus::ISS_Free);
			}
		}
	}
	InteractionSlotClaimHandle = FInteractionSlotClaimHandle();
}

void UNarrativeInteractionComponent::ServerStopInteractBehavior_Implementation(const bool bWasStolen, UNarrativeInteractionComponent* OptionalStealer /*= nullptr*/, FGameplayEventData OptionalPayload)
{
	StopInteractBehavior(bWasStolen, OptionalStealer);
}

bool UNarrativeInteractionComponent::RunInteractBehavior(const bool bIsStealing, UNarrativeInteractionComponent* StealingFrom)
{ 
	//OwningPawn isn't valid so we have to get through controller 
	ANarrativeCharacter* OwnerChar = OwningPawn;

	if (!OwnerChar)
	{
		OwnerChar = Cast<ANarrativeCharacter>(OwningController->GetPawn());
	}

	//Try get via interface if we cant get it 
	if (!OwnerChar)
	{
		if (INarrativeCharacterOwner* OwnerInterface = Cast<INarrativeCharacterOwner>(OwningController))
		{
			OwnerChar = OwnerInterface->GetNarrativeCharacter();
		}
	}

	if (OwnerChar)
	{
		if (UNarrativeInteractableComponent* CurrentInteractable = InteractionSlotClaimHandle.HandleOwner.Get())
		{
			FInteractionSlotConfig SlotConfig = CurrentInteractable->GetConfigAtSlot(InteractionSlotClaimHandle.HandleIndex); 

			if (IsValid(SlotConfig.SlotInteractBehavior) && IsValid(SlotConfig.SlotInteractBehavior->GetInteractAbility()))
			{
				if (UAbilitySystemComponent* NASC = OwnerChar->GetAbilitySystemComponent())
				{
					//When the ability ends, it will release our slot 
					FGameplayAbilitySpec InteractAbilitySpec = NASC->BuildAbilitySpecFromClass(SlotConfig.SlotInteractBehavior->GetInteractAbility(), 1);

					//Pass the stealer along via event data. 
					FGameplayEventData InteractEventData;

					InteractEventData.EventTag = bIsStealing ? FNarrativeGameplayTags::Get().GameplayEvent_Interact_Steal : FNarrativeGameplayTags::Get().GameplayEvent_Interact;
					InteractEventData.OptionalObject2 = IsValid(StealingFrom) ? StealingFrom : nullptr;

					//To guarantee clients interaction slot claim handle is replicated in tine, pass it via event data. Client can set theirs then.  
					InteractEventData.OptionalObject = CurrentInteractable;
					InteractEventData.EventMagnitude = InteractionSlotClaimHandle.HandleIndex;

					CurrentInteractAbilityHandle = NASC->GiveAbilityAndActivateOnce(InteractAbilitySpec, &InteractEventData);

					return true;
				}
			}
		}
	}
	return false; 
}

bool UNarrativeInteractionComponent::StopInteractBehavior(const bool bWasStolen, UNarrativeInteractionComponent* OptionalStealer, FGameplayEventData OptionalPayload)
{
	if (UNarrativeInteractAbility* Interact = GetInteractAbility())
	{
		//If we successfully ended interaction, have server do the same 
		if (Interact->TryFinishInteraction(bWasStolen, OptionalPayload))
		{
			if (GetOwnerRole() < ROLE_Authority && GetNetMode() != NM_Standalone)
			{
				ServerStopInteractBehavior(bWasStolen, OptionalStealer, OptionalPayload);
			}
			return true;
		}
	}

	return false; 
}

UNarrativeInteractAbility* UNarrativeInteractionComponent::GetInteractAbility() const
{
	ANarrativeCharacter* OwnerChar = OwningPawn;

	//For some reason owning pawn isn't valid on NPCControllers so grab via controlled 
	if (!OwnerChar)
	{
		if (INarrativeCharacterOwner* CharController = Cast<INarrativeCharacterOwner>(OwningController))
		{
			OwnerChar = CharController->GetNarrativeCharacter();
		}
	}

	if (UNarrativeInteractableComponent* CurrentInteractable = InteractionSlotClaimHandle.HandleOwner.Get())
	{
		if (UAbilitySystemComponent* NASC = OwnerChar->GetAbilitySystemComponent())
		{
			//When the ability ends, it will release our slot 
			if (FGameplayAbilitySpec* Spec = NASC->FindAbilitySpecFromHandle(CurrentInteractAbilityHandle))
			{
				if (UNarrativeInteractAbility* Interact = Cast<UNarrativeInteractAbility>(Spec->GetPrimaryInstance()))
				{
					return Interact;
				}
			}
		}
	}

	return nullptr; 
}


#undef LOCTEXT_NAMESPACE
