// Copyright Narrative Tools 2024. 


#include "GAS/NarrativeInteractAbility.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "NarrativeGameplayTags.h"
#include "Interaction/InteractionComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Interaction/InteractableComponent.h"
#include "GameFramework/Controller.h"

UNarrativeInteractAbility::UNarrativeInteractAbility()
{	
	//Interaction is always initated by the server 
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UNarrativeInteractAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	//Cache the slot configuration
	if (CharacterOwner && TriggerEventData)
	{
	
		InteractionComponent = CharacterOwner->GetInteractionComponent();

		if (InteractionComponent)
		{
			//If we're a client set our claimed handle based on what the server passed through. We replicate the handle to the client this
			// way as otherwise it may not be replicated by the time ability activates. 
			//Alternative would be to replicate the handle to the client, wait for client to RPC back, and then initiate the ability, but with high ping this would cause huge interaction lag 
			if (!HasAuthority(&ActivationInfo))
			{
				InteractionComponent->InteractionSlotClaimHandle.HandleIndex = (int32)TriggerEventData->EventMagnitude;
				const UNarrativeInteractableComponent* Interactable = Cast<UNarrativeInteractableComponent>(TriggerEventData->OptionalObject);

				//No way around it - EventData is const but the handle owner shouldn't be const 
				InteractionComponent->InteractionSlotClaimHandle.HandleOwner = const_cast<UNarrativeInteractableComponent*>(Interactable);

				//Also need to set the spec handle as client needs this to exit interaction 
				InteractionComponent->CurrentInteractAbilityHandle = Handle; 
			}

			FInteractionSlotClaimHandle& ClaimHandle = InteractionComponent->InteractionSlotClaimHandle;

			if (ClaimHandle.IsValidHandle())
			{
				if (UNarrativeInteractableComponent* Interactable = ClaimHandle.HandleOwner.Get())
				{
					SlotConfiguration = Interactable->GetConfigAtSlot(ClaimHandle.HandleIndex);
					InteractingWithComponent = Interactable;

					//Store this for savegame purposes
					InteractionComponent->SetOccupiedInteractable(Interactable, ClaimHandle.HandleIndex);

					//Set the slot as occupied. 
					Interactable->UpdateSlotStatus(InteractionComponent, ClaimHandle.HandleIndex, EInteractionSlotStatus::ISS_Occupied);

					//Tell interaction we started using it
					InteractionComponent->OnBeginUseInteractable.Broadcast(Interactable->GetOwner(), Interactable);

					//End the ability when we get the gameplay event notify 
					WaitEndInteract = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, FNarrativeGameplayTags::Get().GameplayEvent_WantsEndInteract, GetAvatarActorFromActorInfo(), false, false);

					if (WaitEndInteract)
					{
						WaitEndInteract->EventReceived.AddDynamic(this, &UNarrativeInteractAbility::OnReceiveEndInteractEvent);
					}

					WaitEndInteract->Activate();
				}
			}

			if (TriggerEventData)
			{

				UNarrativeInteractionComponent* StealingFrom = nullptr;

				if (TriggerEventData->OptionalObject2)
				{
					const UNarrativeInteractionComponent* ConstStealingFrom = Cast<UNarrativeInteractionComponent>(TriggerEventData->OptionalObject2);

					StealingFrom = const_cast<UNarrativeInteractionComponent*>(ConstStealingFrom);

				}

				//Pull the stealing info from the EventData we got. todo fill out interaction component 
				HandleInteraction(TriggerEventData->EventTag == FNarrativeGameplayTags::Get().GameplayEvent_Interact_Steal, const_cast<UNarrativeInteractionComponent*>(StealingFrom));
			}

		}
	}

}

bool UNarrativeInteractAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /*= nullptr*/, const FGameplayTagContainer* TargetTags /*= nullptr*/, OUT FGameplayTagContainer* OptionalRelevantTags /*= nullptr*/) const
{
	const bool bSuperAllows = Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

	if (!bSuperAllows)
	{
		return false; 
	}

	if (CharacterOwner)
	{
		if (AController* Controller = CharacterOwner->GetController())
		{
			if (UNarrativeInteractionComponent* InteractionComp = Cast<UNarrativeInteractionComponent>(Controller->GetComponentByClass(UNarrativeInteractionComponent::StaticClass())))
			{
				if (InteractionComp && InteractionComp->InteractionSlotClaimHandle.IsValidHandle())
				{
					return true;
				}
			}
		}
	}

	return false; 
}

void UNarrativeInteractAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);

	if (WaitEndInteract)
	{
		WaitEndInteract->EndTask();
	}

	if (CharacterOwner)
	{
		if (InteractionComponent)
		{
			FInteractionSlotClaimHandle& ClaimHandle = InteractionComponent->InteractionSlotClaimHandle;

			if (ClaimHandle.IsValidHandle())
			{
				if (UNarrativeInteractableComponent* Interactable = ClaimHandle.HandleOwner.Get())
				{

					/**Handles currently aren't automatically updated if someone steals our slot. Ideally we would refactor, 
					but since its just this case, just check if slot was stolen, and invalidate it if it was. */
					

					/*If a new BPA_Interact has started and claimed a new slot, don't release that one!
					Only release the slot if our interactable is still the same as the one we initially claimed. */
					if (InteractingWithComponent == Interactable)
					{
						InteractionComponent->ReleaseInteractionSlot();
					}

					//Same for occupied interactable - make sure something else hasnt changed it. 
					if (InteractionComponent->OccupiedInteractable == InteractingWithComponent)
					{
						InteractionComponent->SetOccupiedInteractable(nullptr, -1);
					}

					InteractionComponent->OnFinishUseInteractable.Broadcast(Interactable->GetOwner(), Interactable);
				}
			}
			else
			{
				InteractionComponent->ReleaseInteractionSlot();
			}
		}
	}
}

bool UNarrativeInteractAbility::CanExitInteraction_Implementation(const bool bWasStolen, FGameplayEventData OptionalPayload) const
{
	return true; 
}

void UNarrativeInteractAbility::OnReceiveEndInteractEvent(FGameplayEventData Payload)
{
	TryFinishInteraction();
}

bool UNarrativeInteractAbility::TryFinishInteraction(const bool bWasStolen, FGameplayEventData OptionalPayload)
{
	if (CanExitInteraction(bWasStolen, OptionalPayload))
	{
		FinishInteraction(bWasStolen, OptionalPayload);
		return true;
	}


	return false; 
}
