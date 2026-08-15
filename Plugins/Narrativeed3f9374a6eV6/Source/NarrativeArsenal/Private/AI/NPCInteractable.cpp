// Copyright Narrative Tools 2024. 


#include "AI/NPCInteractable.h"
#include "Tales/TalesComponent.h"
#include "Tales/NarrativeFunctionLibrary.h"
#include "Items/InventoryFunctionLibrary.h"
#include "Items/InventoryComponent.h"
#include "NarrativeGameplayTags.h"
#include "UnrealFramework/NarrativePlayerController.h"

#define LOCTEXT_NAMESPACE "NPCInteractable"

UNPCInteractable::UNPCInteractable(const FObjectInitializer& ObjectInitializer)
{
	InteractableActionText = LOCTEXT("TalkInteractableActionText", "Talk");
}

bool UNPCInteractable::Interact(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	if (Interactor)
	{
		if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
		{
			if (NPC->IsAlive())
			{
				if (ANarrativePlayerController* PController = Cast<ANarrativePlayerController>(Interactor->GetController()))
				{
					if (UTalesComponent* TalesComponent = PController->GetTalesComponent())
					{
						//We assume the NPCs dialogues first speaker is the NPC
						FDialoguePlayParams PlayParams;
						PlayParams.Speakers.Add(NPC);

						//TODO load this async, potentially before we're even started the interact 
						TalesComponent->BeginDialogue(Dialogue, PlayParams);
					}
				}
			}
			else
			{
				if (UNarrativeInventoryComponent* Looter = UInventoryFunctionLibrary::GetInventoryComponentFromTarget(Interactor))
				{
					Looter->SetLootSource(NPC->GetInventoryComponent());
				}
			}
		}
	}

	return true;
}

bool UNPCInteractable::CanInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp, FText& OutErrorText)
{
	if (ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner()))
	{
		if (!NPC->IsAlive())
		{
			if (NPC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_NPC_DisableLooting))
			{
				OutErrorText = LOCTEXT("LootingDisabled", "Can not Loot.");
				return false;
			}
		}

	
		if (NPC->IsAlive() && NPC->GetNPCDefinition())
		{
			if (!IsValid(Dialogue))
			{
				return false; 
			}

			if (NPC->IsRagdoll())
			{
				OutErrorText = FText::Format(LOCTEXT("NPCIsRagdollError", "You might want to let {0} get up first."), NPC->GetNPCName());
				return false;
			}

			if (NPC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_NPC_IsBusy))
			{
				OutErrorText = FText::Format(LOCTEXT("NPCIsBusyError", "{0} is busy right now."), NPC->GetNPCName());
				return false; 
			}

			if (UTalesComponent* Narrative = UNarrativeFunctionLibrary::GetTalesComponent(Interactor))
			{
				if (IsValid(Narrative->CurrentDialogue) && !Narrative->CurrentDialogue->bCanBeExited)
				{
					OutErrorText = LOCTEXT("AlreadyInDialogueError", "Wait for the current dialogue to finish.");
					return false;
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE 