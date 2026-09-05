// Copyright Narrative Tools 2024. 

#include "Interaction/InteractableComponent.h"
#include <UObject/ConstructorHelpers.h>
#include <Materials/MaterialInterface.h>
#include <Components/MeshComponent.h>
#include <GameFramework/Actor.h>
#include <GameFramework/PlayerController.h>
#include <Engine/World.h>
#include "Net/UnrealNetwork.h"
#include "Interaction/InteractionComponent.h"
#include "Interaction/InteractionSubsystem.h"
#include "GAS/NarrativeInteractAbility.h"
#include <InputAction.h>
#include "UnrealFramework/NarrativeNPCCharacter.h"

#define LOCTEXT_NAMESPACE "InteractableComponent"

// Sets default values for this component's properties
UNarrativeInteractableComponent::UNarrativeInteractableComponent()
{
	InteractionTime = 0.f;
	InteractionDistance = 200.f;
	InteractableNameText = LOCTEXT("InteractableNameText", "Interactable Object");
	InteractableActionText = LOCTEXT("InteractableActionText", "Interact");
	
	//auto FocusedOverlayFinder = ConstructorHelpers::FObjectFinder<UMaterialInterface>(TEXT("/Script/Engine.Material'/NarrativePro/NarrativeInteraction/Materials/M_Pulse.M_Pulse'"));
	//if (FocusedOverlayFinder.Succeeded())
	//{
	//	FocusedOverlayMaterial = FocusedOverlayFinder.Object;
	//}

	SetAutoActivate(true);
	SetComponentTickEnabled(false);
	SetIsReplicatedByDefault(true);
}

void UNarrativeInteractableComponent::BeginPlay()
{
	Super::BeginPlay();

	//Allocate a slot status for each slot 
	SlotStatuses.SetNum(InteractionSlots.Num());

	if (UWorld* World = GetWorld())
	{
		if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
		{
			InteractionSubsystem->CacheInteractable(this);
		}
	}

}

void UNarrativeInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UWorld* World = GetWorld())
	{
		if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
		{
			InteractionSubsystem->UncacheInteractable(this);
		}
	}
}

void UNarrativeInteractableComponent::Deactivate()
{
	Super::Deactivate();
}

void UNarrativeInteractableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UNarrativeInteractableComponent, SlotStatuses);
}

FText UNarrativeInteractableComponent::GetInteractableNameText_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp) const
{
	return InteractableNameText;
}

FText UNarrativeInteractableComponent::GetInteractableActionText_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp) const
{
	return InteractableActionText;
}

void UNarrativeInteractableComponent::SetInteractableNameText(const FText& NewNameText)
{
	InteractableNameText = NewNameText;
}

void UNarrativeInteractableComponent::SetInteractableActionText(const FText& NewActionText)
{
	InteractableActionText = NewActionText;
}

int32 UNarrativeInteractableComponent::GetBestAvailableSlot_Implementation(class UNarrativeInteractionComponent* Claimer, const TArray<int32>& SlotsToCheck)
{
	if (Claimer)
	{
		if (AController* ClaimerController = Cast<AController>(Claimer->GetOwner()))
		{
			if (APawn* ClaimerPawn = ClaimerController->GetPawn())
			{
				float BestDist = FLT_MAX;
				int32 BestSlot = -1;

				for (auto& Slot : SlotsToCheck)
				{
					float Dist = FVector::Dist(ClaimerPawn->GetActorLocation(), (InteractionSlots[Slot].SlotTransform * GetOwner()->GetActorTransform()).GetLocation());

					if (Dist < BestDist)
					{
						BestSlot = Slot;
						BestDist = Dist;
					}
				}

				return BestSlot;
			}
		}
	}

	return -1;
}

TArray<int32> UNarrativeInteractableComponent::GetAvailableSlots(class UNarrativeInteractionComponent* Claimer, const bool bIncludeTargeted, const bool bIncludeStealable)
{
	TArray<int32> Slots;

	for (int32 i = 0; i < SlotStatuses.Num(); ++i)
	{

		if (UInteractionSlotBehavior* SlotBehavior = InteractionSlots[i].SlotInteractBehavior)
		{
			if (SlotBehavior->IsUsable(i, this, Claimer))
			{
				if (SlotStatuses[i].SlotStatus == EInteractionSlotStatus::ISS_Free || (bIncludeTargeted && SlotStatuses[i].SlotStatus == EInteractionSlotStatus::ISS_Targeted))
				{
					Slots.Add(i);
				}
				else if (SlotStatuses[i].SlotStatus == EInteractionSlotStatus::ISS_Occupied && bIncludeStealable)
				{
					//If occupied, check if claimer can steal
					if (SlotBehavior->IsStealable(i, this, Claimer))
					{
						Slots.Add(i);
					}
				}
			}
		}
	}

	return Slots; 
}

TArray<int32> UNarrativeInteractableComponent::GetSlotsAtIndex(const int32 SlotIndex) const
{
	if (InteractionSlots.IsValidIndex(SlotIndex))
	{
		TArray<int32> Slots = {SlotIndex};

		//TODO this should be bi-directional
		Slots.Append(InteractionSlots[SlotIndex].LinkedSlots);

		return Slots;
	}

	return {};
}

FInteractionSlotClaimHandle UNarrativeInteractableComponent::ClaimSlot(class UNarrativeInteractionComponent* Claimer, int32 SlotToClaimIdx, const bool bMarkTargeted)
{
	if (GetOwnerRole() >= ROLE_Authority && Claimer)
	{
		if (SlotToClaimIdx >= 0 && SlotStatuses.IsValidIndex(SlotToClaimIdx) && InteractionSlots.IsValidIndex(SlotToClaimIdx))
		{	
			//If we're stealing a slot, we need to inform the existing person to end their interaction - if that fails abort. 
			if (SlotStatuses[SlotToClaimIdx].SlotStatus == EInteractionSlotStatus::ISS_Occupied)
			{
				if (!SlotStatuses[SlotToClaimIdx].SlotUser->StopInteractBehavior(true, Claimer))
				{
					return FInteractionSlotClaimHandle::InvalidHandle();
				}
			}

			TArray<FActiveInteractionSlot> OldSlotStatuses = SlotStatuses;

			FInteractionSlotClaimHandle Handle(SlotToClaimIdx, this);

			UpdateSlotStatus(Claimer, SlotToClaimIdx, bMarkTargeted ? EInteractionSlotStatus::ISS_Targeted : EInteractionSlotStatus::ISS_Occupied);

			//SlotStatuses[SlotToClaimIdx].SlotUser = Claimer;
			//SlotStatuses[SlotToClaimIdx].SlotStatus = bMarkTargeted ? EInteractionSlotStatus::ISS_Targeted : EInteractionSlotStatus::ISS_Occupied;

			//OnRep_SlotStatuses(OldSlotStatuses);

			return Handle;
		}
	}
	return FInteractionSlotClaimHandle::InvalidHandle();
}

//FInteractionSlotClaimHandle UNarrativeInteractableComponent::ClaimSlot(class UNarrativeInteractionComponent* Claimer, const bool bFindClosest, const bool bMarkTargeted)
//{
//	if (GetOwnerRole() >= ROLE_Authority && Claimer)
//	{
//		int32 i = 0;
//
//		float BestDist = FLT_MAX;
//		int32 BestSlot = -1;
//		
//		for (auto& SlotStatus : SlotStatuses)
//		{
//			//If we're trying to target a slot, only target free slots 
//			const bool bSlotIsFree = SlotStatus.SlotStatus != EInteractionSlotStatus::ISS_Occupied && !bMarkTargeted || SlotStatus.SlotStatus == EInteractionSlotStatus::ISS_Free;
//		
//			if (bSlotIsFree && InteractionSlots.IsValidIndex(i))
//			{
//				float Dist = FVector::Dist(Character->GetActorLocation(), (InteractionSlots[i].SlotTransform * GetOwner()->GetActorTransform()).GetLocation());
//
//				if (Dist < BestDist)
//				{
//					BestSlot = i;
//					BestDist = Dist;
//
//					//Find closest needs to check every slot, otherwise early out we've found a good one 
//					if (!bFindClosest)
//					{
//						break;
//					}
//				}
//			}
//			++i;
//		}
//
//		if (BestSlot != -1 && SlotStatuses.IsValidIndex(BestSlot) && InteractionSlots.IsValidIndex(BestSlot))
//		{
//			TArray<FActiveInteractionSlot> OldSlotStatuses = SlotStatuses;
//
//			SlotStatuses[BestSlot].SlotUser = Character;
//			SlotStatuses[BestSlot].SlotStatus = bMarkTargeted ? EInteractionSlotStatus::ISS_Targeted : EInteractionSlotStatus::ISS_Occupied;
//
//			OnRep_SlotStatuses(OldSlotStatuses);
//
//			OutConfig = InteractionSlots[BestSlot];
//
//			return FInteractionSlotClaimHandle(BestSlot, this);
//		}
//	}
//	return FInteractionSlotClaimHandle();
//}

void UNarrativeInteractableComponent::UpdateSlotStatus(class UNarrativeInteractionComponent* Claimer, const int32 SlotIndex, const EInteractionSlotStatus NewStatus)
{
	if (GetOwnerRole() >= ROLE_Authority)
	{
		TArray<FActiveInteractionSlot> OldSlotStatuses = SlotStatuses;

		for (auto& SlotIdx : GetSlotsAtIndex(SlotIndex))
		{
			if (SlotStatuses.IsValidIndex(SlotIdx))
			{
				//Targeting a slot someone is occupying shouldn't update the slot 
				if (NewStatus == EInteractionSlotStatus::ISS_Targeted && SlotStatuses[SlotIdx].SlotStatus == EInteractionSlotStatus::ISS_Occupied)
				{
					return;
				}

				SlotStatuses[SlotIdx].SlotStatus = NewStatus;

				if (NewStatus == EInteractionSlotStatus::ISS_Free)
				{
					SlotStatuses[SlotIdx].SlotUser = nullptr;
				}
				else if (NewStatus == EInteractionSlotStatus::ISS_Occupied)// If we're targeting a slot, dont mark us as the user, that will break stealing || NewStatus == EInteractionSlotStatus::ISS_Targeted)
				{
					SlotStatuses[SlotIdx].SlotUser = Claimer;
				}
			}
		}

		OnRep_SlotStatuses(OldSlotStatuses);
	}
}
 
FInteractionSlotConfig UNarrativeInteractableComponent::GetConfigAtSlot(int32 SlotIndex)
{
	if (InteractionSlots.IsValidIndex(SlotIndex))
	{
		return InteractionSlots[SlotIndex];
	}

	return FInteractionSlotConfig();
}

bool UNarrativeInteractableComponent::HasSlotAvailable(const bool bAllowTargeted) const
{
	for (auto& Slot : SlotStatuses)
	{
		if (Slot.SlotStatus !=  EInteractionSlotStatus::ISS_Occupied || (bAllowTargeted && Slot.SlotStatus == EInteractionSlotStatus::ISS_Targeted))
		{
			return true;
		}
	}

	return false; 
}

FBox UNarrativeInteractableComponent::GetInteractableBounds() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return FBox(ForceInit); }
	if (InteractionSlots.IsEmpty())
	{
		const FBox Bounds = Owner->GetComponentsBoundingBox(true);
		return Bounds.IsValid ? Bounds : FBox(Owner->GetActorLocation(), Owner->GetActorLocation());
	}
	FBox Bounds(ForceInit);
	for (const auto& Slot : InteractionSlots)
	{
		Bounds += Slot.SlotTransform.GetLocation() + FVector(40, 40, 90);
		Bounds += Slot.SlotTransform.GetLocation() - FVector(40, 40, 90);
	}
	return Bounds.TransformBy(Owner->GetTransform());
}

void UNarrativeInteractableComponent::BeginFocus(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	if (!IsActive() || !GetOwner() || !Interactor)
	{
		return;
	}

	OnBeginFocus.Broadcast(Interactor, InteractionComp);

	if (GetNetMode() != NM_DedicatedServer)
	{
		TInlineComponentArray<UMeshComponent*> Meshes;
		GetOwner()->GetComponents(Meshes);

		for (auto& Mesh : Meshes)
		{
			if (Mesh)
			{
				Mesh->SetOverlayMaterial(FocusedOverlayMaterial);
			}
		}
	}
}

void UNarrativeInteractableComponent::EndFocus(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	OnEndFocus.Broadcast(Interactor, InteractionComp);

	if (GetNetMode() != NM_DedicatedServer)
	{
		TInlineComponentArray<UMeshComponent*> Meshes;
		GetOwner()->GetComponents(Meshes);

		for (auto& Mesh : Meshes)
		{
			if (Mesh)
			{
				Mesh->SetOverlayMaterial(nullptr);
			}
		}
	}
}

void UNarrativeInteractableComponent::BeginInteract(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	OnBeginInteract(Interactor, InteractionComp);
	OnBeginInteracted.Broadcast(Interactor, InteractionComp);
}

void UNarrativeInteractableComponent::EndInteract(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	OnEndInteract(Interactor, InteractionComp);
	OnEndInteracted.Broadcast(Interactor, InteractionComp);
}

bool UNarrativeInteractableComponent::Interact(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{
	FText ErrorMessage;

	const bool bCanInteract = CanInteract(Interactor, InteractionComp, ErrorMessage);

	if (bCanInteract)
	{
		OnInteract(Interactor, InteractionComp);
		OnInteracted.Broadcast(Interactor, InteractionComp);
	}

	return bCanInteract;
}

void UNarrativeInteractableComponent::OnInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{

}

void UNarrativeInteractableComponent::OnBeginInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{

}

void UNarrativeInteractableComponent::OnEndInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp)
{

}

bool UNarrativeInteractableComponent::CanInteract_Implementation(class APawn* Interactor, class UNarrativeInteractionComponent* InteractionComp, FText& ErrorMessage)
{
	if (InteractionSlots.Num() && !HasSlotAvailable(true))
	{
		ErrorMessage = LOCTEXT("SlotTakenError", "This is being used right now.");
		return false; 
	}

	return true; 
}

void UNarrativeInteractableComponent::OnRep_SlotStatuses(const TArray<FActiveInteractionSlot>& OldStatuses)
{
	for (int32 i = 0; i < OldStatuses.Num(); ++i)
	{
		if (OldStatuses.IsValidIndex(i) && SlotStatuses.IsValidIndex(i))
		{
			//Handle targeted slot being taken by someone so we can find a new slot to target
			if (OldStatuses[i].SlotStatus == EInteractionSlotStatus::ISS_Targeted && SlotStatuses[i].SlotStatus == EInteractionSlotStatus::ISS_Occupied)
			{
				//Targeted slots are only considered taken if someone else takes them 
				if (OldStatuses[i].SlotUser != SlotStatuses[i].SlotUser)
				{
					OnTargetedSlotTaken.Broadcast(i, SlotStatuses[i].SlotUser, this);
				}
			}
		}
	}
}



#if WITH_GAMEPLAY_DEBUGGER

FGameplayDebuggerCategory_Interactable::FGameplayDebuggerCategory_Interactable()
{

}

void FGameplayDebuggerCategory_Interactable::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	if (OwnerPC && OwnerPC->GetPawn())
	{
		if (UWorld* World = GetDataWorld(OwnerPC, DebugActor))
		{
			if (UInteractionSubsystem* InteractionSubsystem = World->GetSubsystem<UInteractionSubsystem>())
			{
				FVector ViewLocation = FVector::ZeroVector;
				FVector ViewDirection = FVector::ForwardVector;
				bool bApplyCulling = GetViewPoint(OwnerPC, ViewLocation, ViewDirection);

				constexpr float DebugArrowThickness = 2.f;
				constexpr float DebugCircleRadius = 40.f;
				constexpr float DebugArrowHeadSize = 10.f;

				const FColor FreeColor = FColorList::Green;
				const FColor TargetedColor = FColorList::Yellow;
				const FColor OccupiedColor = FColorList::Red;
				const FColor DisabledColor = FColorList::Grey;

				for (auto& Interactable : InteractionSubsystem->GetInteractableActors())
				{
					if (Interactable)
					{
						if (AActor* InteractableOwner = Interactable->GetOwner())
						{
							for (int32 i = 0; i < Interactable->InteractionSlots.Num(); ++i)
							{
								if (Interactable->InteractionSlots.IsValidIndex(i) && Interactable->SlotStatuses.IsValidIndex(i))
								{
									FInteractionSlotConfig& Config = Interactable->InteractionSlots[i];
									FActiveInteractionSlot& SlotStatus = Interactable->SlotStatuses[i];

									FTransform SlotTrans = Config.SlotTransform * InteractableOwner->GetActorTransform();

									FColor DrawColor = FreeColor;

									if (SlotStatus.SlotStatus == EInteractionSlotStatus::ISS_Targeted)
									{
										DrawColor = TargetedColor;
									}
									else if (SlotStatus.SlotStatus == EInteractionSlotStatus::ISS_Occupied)
									{
										DrawColor = OccupiedColor;
									}

									AddShape(FGameplayDebuggerShape::MakeCircle(SlotTrans.GetLocation(), FVector::UpVector, DebugCircleRadius, DrawColor));
									AddShape(FGameplayDebuggerShape::MakeCircle(SlotTrans.GetLocation(), FVector::UpVector, 0.75f * DebugCircleRadius, /* Thickness */5.f, DrawColor));
									AddShape(FGameplayDebuggerShape::MakeArrow(SlotTrans.GetLocation(), SlotTrans.GetLocation() + SlotTrans.GetRotation().Vector() * 2.0f * DebugCircleRadius, DebugArrowHeadSize, DebugArrowThickness, DrawColor));
								}
							}
						}
					}
				}
			}
		}
	}
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Interactable::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Interactable());
}

#endif 

UInteractionSlotBehavior::UInteractionSlotBehavior()
{
	FinishInteractText = FText::GetEmpty();
	bIsStealableByDefault = false; 
}

TArray<FInteractionDebugVisualizeSlot> UInteractionSlotBehavior::GetDebugSlots_Implementation(const FTransform& SlotTransform, const FTransform& OwnerTransform) const
{
	return {};
}

bool UInteractionSlotBehavior::IsStealable_Implementation(const int32 OurSlot, class UNarrativeInteractableComponent* OurInteractable, class UNarrativeInteractionComponent* Interactor) const
{
	//If the occupant cannot exit interaction, we should disallow the steal 
	if (OurSlot >= 0 && OurInteractable && Interactor)
	{
		if (UNarrativeInteractionComponent* ExistingInteractor = OurInteractable->SlotStatuses[OurSlot].SlotUser)
		{
			if (UNarrativeInteractAbility* IA = ExistingInteractor->GetInteractAbility())
			{
				if (!IA->CanExitInteraction(true))
				{
					return false; 
				}
			}
		}
	}

	return bIsStealableByDefault; 
}

bool UInteractionSlotBehavior::IsUsable_Implementation(const int32 OurSlot, class UNarrativeInteractableComponent* OurInteractable, class UNarrativeInteractionComponent* Interactor) const
{
	if (AllowedInteractors.Num() > 0)
	{
		return AllowedInteractors.Contains(Interactor);
	}

	return true; 
}

#undef LOCTEXT_NAMESPACE