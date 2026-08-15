// Copyright Narrative Tools 2024. 


#include "Interaction/PlayerInteractionComponent.h"
#include "Interaction/InteractableComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "GAS/NarrativeInteractAbility.h"
#include <GameFramework/Controller.h>
#include <AbilitySystemComponent.h>

#include "ArsenalSettings.h"
#include "ArsenalStatics.h"
#include "Subsystems/NarrativeSaveSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "KismetTraceUtils.h"

#define LOCTEXT_NAMESPACE "InteractionComponent"


static TAutoConsoleVariable<bool> CVarInteractionDebug(
	TEXT("n.interaction.debug"),
	false,
	TEXT("Debug Interaction check 0=Off 1=On"),
	ECVF_Default);

// Sets default values for this component's properties
UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;

	ViewedInteractable = nullptr;
	LastInteractionCheckTime = 0.f;
	InteractionCheckDistance = 1000.f;
	RemainingInteractTime = -999.f;

	bInteractHeld = false;
	SetAutoActivate(true);
	SetIsReplicatedByDefault(true);
}

//void UPlayerInteractionComponent::BindToInput(class UEnhancedInputComponent* EnhancedInput)
//{
//	if (EnhancedInput)
//	{
//		for (const auto& InteractInput : InteractionInputs)
//		{
//			int32 i = 0;
//			if (IsValid(InteractInput))
//			{
//				EnhancedInput->BindAction(InteractInput, ETriggerEvent::Started, this, &UPlayerInteractionComponent::InteractInputPressed, i);
//				EnhancedInput->BindAction(InteractInput, ETriggerEvent::Completed, this, &UPlayerInteractionComponent::InteractInputReleased, i);
//
//				++i;
//			}
//		}
//	}
//}


void UPlayerInteractionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Server wasn't able to get this
	if (!OwningPawn && OwningController)
	{
		OwningPawn = Cast<ANarrativeCharacter>(OwningController->GetPawn());
	}

	if (IsActive())
	{
		if (GetWorld()->TimeSince(LastInteractionCheckTime) > InteractionCheckFrequency)
		{
			PerformInteractionCheck(DeltaTime);

		}

		if (bInteractHeld && ViewedInteractable && GetOwnerRole() >= ROLE_Authority)
		{
			if (RemainingInteractTime > 0.f)
			{
				RemainingInteractTime -= DeltaTime;
			}

			if (RemainingInteractTime <= 0.f && RemainingInteractTime > -998.f)
			{
				//Since we added slots, we should check those, but if no slots are added just do old style instant interaction
				if (ViewedInteractable->InteractionSlots.Num())
				{
					if (ANarrativeCharacter* OwnerChar = Cast<ANarrativeCharacter>(OwningPawn))
					{
						const int32 BestInteractionSlot = ViewedInteractable->GetBestAvailableSlot(this, ViewedInteractable->GetAvailableSlots(this, true));

						if (BestInteractionSlot != -1)
						{
							UNarrativeInteractionComponent* StealingFrom = nullptr; 

							//See if the slot we got is a steal
							if (ViewedInteractable->SlotStatuses[BestInteractionSlot].SlotStatus == EInteractionSlotStatus::ISS_Occupied)
							{
								StealingFrom = ViewedInteractable->SlotStatuses[BestInteractionSlot].SlotUser;
							}

							//ClaimInteractionSlot will remove the existing occupant, telling them we removed us 
							if (ClaimInteractionSlot(ViewedInteractable, BestInteractionSlot))
							{
								RunInteractBehavior(IsValid(StealingFrom), StealingFrom);
							}
						}

					}
				}
				else // We still support this legacy style interaction where there aren't any slots and you just instant interact 
				{
					const bool bInteracted = ViewedInteractable->Interact(OwningPawn, this);

					if (bInteracted && ViewedInteractable)
					{
						OnBeginUseInteractable.Broadcast(ViewedInteractable->GetOwner(), ViewedInteractable);
						OnFinishUseInteractable.Broadcast(ViewedInteractable->GetOwner(), ViewedInteractable);
					}
				}

				RemainingInteractTime = -999.f;
			}
		}
	}
}

void UPlayerInteractionComponent::Deactivate()
{
	Super::Deactivate();

	ClearViewedInteractable();
}

void UPlayerInteractionComponent::Load_Implementation()
{
	Super::Load_Implementation();

	//Decided to scrap saving occupied interactable to disk as it just gets really complicated and will likely introduce bugs. 
	//Players can immediately resume interaction, whereas NPCs need to do it via saving their interact goal. 
	//Reclaim the saved slot and resume interaction 
	//if (OccupiedInteractable && OccupiedInteractableSlotIdx != -1)
	//{
	//	if (OccupiedInteractable->InteractionSlots.Num())
	//	{
	//		if (ANarrativeCharacter* OwnerChar = Cast<ANarrativeCharacter>(OwningPawn))
	//		{
	//			if (ClaimInteractionSlot(OccupiedInteractable, OccupiedInteractableSlotIdx))
	//			{
	//				FInteractionSlotConfig SlotConfig = OccupiedInteractable->GetConfigAtSlot(InteractionSlotClaimHandle.HandleIndex);

	//				if (IsValid(SlotConfig.SlotInteractBehavior) && IsValid(SlotConfig.SlotInteractBehavior->GetInteractAbility()))
	//				{
	//					if (UAbilitySystemComponent* NASC = OwnerChar->GetAbilitySystemComponent())
	//					{
	//						//When the ability ends, it will release our slot 
	//						NASC->K2_GiveAbilityAndActivateOnce(SlotConfig.SlotInteractBehavior->GetInteractAbility(), 1);
	//					}
	//				}
	//			}
	//		}
	//	}
	//}
}

void UPlayerInteractionComponent::PerformInteractionCheck(float DeltaTime)
{
	if (OwningController && OwningPawn)
	{
		FVector EyesLoc;
		FRotator EyesRot;

		OwningController->GetPlayerViewPoint(EyesLoc, EyesRot);

		//Add camera dist from pawn as long camera arms shouldn't effect how far you can interact 


		const FVector PawnLoc = OwningPawn->GetActorLocation();
		const FVector AimDir = EyesRot.Vector();
		const FVector FocalLoc = EyesLoc + (AimDir * 1024.f);

		const FVector StartPoint = FocalLoc + (((PawnLoc - FocalLoc) | AimDir) * AimDir);
		const FVector TraceStart = StartPoint;
		const FVector TraceEnd = (EyesRot.Vector() * InteractionCheckDistance) + TraceStart;
		FHitResult TraceHit;

		FCollisionQueryParams QueryParams = FCollisionQueryParams();
		QueryParams.TraceTag = "PlayerInteract";
		QueryParams.AddIgnoredActor(OwningPawn);

		/*When checking for interactables, we use a profile so walls and such block our path to the interactable, which is desired behavior.
		However interactables themselves should block the interactable trace channel - that way we can differentiate interactables from
		walls etc by checking if they block interactable channel. */
		if (UArsenalSettings* Settings = UArsenalStatics::GetNarrativeProSettings())
		{
			if (FMath::IsNearlyZero(InteractionCheckSphereRadius))
			{
				GetWorld()->LineTraceSingleByChannel(TraceHit, TraceStart, TraceEnd, Settings->InteractionTraceChannel, QueryParams);
				//GetWorld()->LineTraceSingleByProfile(TraceHit, TraceStart, TraceEnd, InteractTraceProfile, QueryParams);

#if ENABLE_DRAW_DEBUG
				if (CVarInteractionDebug.GetValueOnGameThread() && ViewedInteractable)
				{
					DrawDebugLineTraceSingle(GetWorld(), TraceStart, TraceEnd, EDrawDebugTrace::Type::ForDuration, TraceHit.bBlockingHit, TraceHit, FLinearColor::Green, FLinearColor::Red, 5.f);
				}
#endif

			}
			else
			{
				GetWorld()->LineTraceSingleByChannel(TraceHit, TraceStart, TraceEnd, Settings->InteractionTraceChannel, QueryParams);
				//GetWorld()->LineTraceSingleByProfile(TraceHit, TraceStart, TraceEnd, InteractTraceProfile, QueryParams);

#if ENABLE_DRAW_DEBUG
				if (CVarInteractionDebug.GetValueOnGameThread())
				{
					DrawDebugLineTraceSingle(GetWorld(), TraceStart, TraceEnd, EDrawDebugTrace::Type::ForDuration, TraceHit.bBlockingHit, TraceHit, FLinearColor::Green, FLinearColor::Red, 5.f);
				}
#endif

				// If our line trace didnt hit an interactable, try the sphere trace
				if (!TraceHit.GetActor() || !TraceHit.GetActor()->GetComponentByClass<UNarrativeInteractableComponent>())
				{
					const FCollisionShape Sphere = FCollisionShape::MakeSphere(InteractionCheckSphereRadius);
					FHitResult Hit;

					const bool bMultiHit = GetWorld()->SweepSingleByChannel(Hit, TraceStart, TraceEnd, FQuat(), Settings->InteractionTraceChannel, Sphere, QueryParams);
					//GetWorld()->SweepMultiByProfile(MultiHit, TraceStart, TraceEnd, FQuat::Identity, InteractTraceProfile, Sphere, QueryParams);
#if ENABLE_DRAW_DEBUG
					if (CVarInteractionDebug.GetValueOnGameThread())
					{
						DrawDebugSphereTraceSingle(GetWorld(), TraceStart, TraceEnd, Sphere.GetSphereRadius(), EDrawDebugTrace::Type::ForOneFrame, bMultiHit, Hit, FLinearColor::Green, FLinearColor::Red, 0.5f);
					}
#endif 

					if(Hit.bBlockingHit)
					{
						if (Hit.GetComponent() && Hit.GetComponent()->GetCollisionResponseToChannel(Settings->InteractionTraceChannel) == ECollisionResponse::ECR_Block)
						{
							if (Hit.GetActor() && Hit.GetActor()->GetComponentByClass<UNarrativeInteractableComponent>())
							{
								TraceHit = Hit;
							}
						}
					}
				}
			}
		}




		//Check if we hit an interactable object
		if (TraceHit.GetActor())
		{
			if (UNarrativeInteractableComponent* InteractableComponent = Cast<UNarrativeInteractableComponent>(TraceHit.GetActor()->GetComponentByClass(UNarrativeInteractableComponent::StaticClass())))
			{
				if (InteractableComponent->IsActive())
				{
					const float Distance = (OwningPawn->GetActorLocation() - TraceHit.ImpactPoint).Size();

					if (Distance <= InteractableComponent->InteractionDistance)
					{
						SetViewedInteractable(InteractableComponent);

#if ENABLE_DRAW_DEBUG
						if (CVarInteractionDebug.GetValueOnGameThread() && ViewedInteractable)
						{
							FString RoleStr = GetOwnerRole() == ROLE_Authority ? "Server" : "Client";
							GEngine->AddOnScreenDebugMessage(-1, 0.001f, FColor::Red, FString::Printf(TEXT("%s: Interaction %s, hit comp %s"), *RoleStr, *GetNameSafe(ViewedInteractable), *GetNameSafe(TraceHit.GetComponent())));
						}
#endif

						return;
					}
				}
			}
		}
	}

	ClearViewedInteractable();
}

void UPlayerInteractionComponent::ClearViewedInteractable()
{
	//Tell the interactable we've stopped focusing on it, and clear the current interactable
	if (ViewedInteractable)
	{
		if (ViewedInteractable)
		{
			ViewedInteractable->EndFocus(OwningPawn, this);
			OnLostInteractable.Broadcast(ViewedInteractable);
		}

		EndInteract();
	}

	ViewedInteractable = nullptr;
}

void UPlayerInteractionComponent::SetViewedInteractable(UNarrativeInteractableComponent* Interactable)
{
	if (Interactable != ViewedInteractable)
	{
		EndInteract();

		if (ViewedInteractable)
		{
			ViewedInteractable->EndFocus(OwningPawn, this);
		}

		ViewedInteractable = Interactable;
		ViewedInteractable->BeginFocus(OwningPawn, this);
		OnFoundInteractable.Broadcast(Interactable);
	}
}

void UPlayerInteractionComponent::ServerBeginInteract_Implementation()
{
	BeginInteract();
}

void UPlayerInteractionComponent::ServerEndInteract_Implementation()
{
	EndInteract();
}

void UPlayerInteractionComponent::BeginInteract()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerBeginInteract();
	}
	
	bInteractHeld = true;

	OnInteractPressed.Broadcast(this);
	
	FText ErrorMessage;
	if (ViewedInteractable && ViewedInteractable->CanInteract(OwningPawn, this, ErrorMessage))
	{
		ViewedInteractable->BeginInteract(OwningPawn, this);
		RemainingInteractTime = ViewedInteractable->InteractionTime;
	}
}

void UPlayerInteractionComponent::EndInteract()
{
	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerEndInteract();
	}

	if (ViewedInteractable)
	{
		ViewedInteractable->EndInteract(OwningPawn, this);
		RemainingInteractTime = -999.f;
	}

	bInteractHeld = false;
	OnInteractReleased.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
