// Copyright Narrative Tools 2024. 


#include "Interaction/NPCInteractionComponent.h"

UNPCInteractionComponent::UNPCInteractionComponent()
{
	bFindNewSlotIfSlotTaken = false; 
}

bool UNPCInteractionComponent::TargetBestInteractionSlot(class UNarrativeInteractableComponent* Interactable, const bool bInFindNewSlotIfSlotTaken)
{
	if (Interactable)
	{
		int32 BestInteractionSlot = Interactable->GetBestAvailableSlot(this, Interactable->GetAvailableSlots(this, false));

		return TargetInteractionSlot(Interactable, BestInteractionSlot, bInFindNewSlotIfSlotTaken);
	}

	return false; 
}

bool UNPCInteractionComponent::TargetInteractionSlot(class UNarrativeInteractableComponent* Interactable, const int32 Index, const bool bInFindNewSlotIfSlotTaken)
{
	if (Interactable && Index >= 0)
	{
		bFindNewSlotIfSlotTaken = bInFindNewSlotIfSlotTaken;

		//Free our slot if we have one already 
		if (InteractionSlotClaimHandle.IsValidHandle())
		{
			if (UNarrativeInteractableComponent* CurrentInteractable = InteractionSlotClaimHandle.HandleOwner.Get())
			{	
				CurrentInteractable->UpdateSlotStatus(this, InteractionSlotClaimHandle.HandleIndex, EInteractionSlotStatus::ISS_Free);
			}
		}

		FInteractionSlotClaimHandle Handle = Interactable->ClaimSlot(this, Index, true);

		if (Handle.IsValidHandle())
		{
			InteractionSlotClaimHandle = Handle;

			//Now that we've claimed the interaction handle, listen for our slot being lost so we can reserve a new one
			Interactable->OnTargetedSlotTaken.AddUniqueDynamic(this, &UNPCInteractionComponent::OnTargetSlotTaken);
			OnTargetedInteractionSlotChanged.Broadcast(Interactable, InteractionSlotClaimHandle.HandleIndex);
			return true;
		}	
	}

	return false; 
}

void UNPCInteractionComponent::OnTargetSlotTaken(int32 Slot, class UNarrativeInteractionComponent* StealerComp, class UNarrativeInteractableComponent* InteractableComp)
{
	//Make sure someoen else took the slot, not us, and make sure the taken slot was the one we were targeting.
	if (StealerComp != this && InteractionSlotClaimHandle.HandleIndex == Slot && InteractionSlotClaimHandle.HandleOwner == InteractableComp)
	{
		//Make sure it was our current interactable that was stolen from 
		if (UNarrativeInteractableComponent* CurrentInteractable = InteractionSlotClaimHandle.HandleOwner.Get())
		{
			if (CurrentInteractable == InteractableComp)
			{
				//Empty out our claim handle, our slot was stolen 
				InteractionSlotClaimHandle = FInteractionSlotClaimHandle::InvalidHandle();

				if (bFindNewSlotIfSlotTaken)
				{
					TargetBestInteractionSlot(CurrentInteractable, true);
				}
			}
		}
	}
}
