// Copyright Narrative Tools 2024. 


#include "Interaction/PlayerInteractionComponent.h"
#include "Interaction/InteractableComponent.h"
#include "Interaction/InteractionSubsystem.h"
#include "UnrealFramework/NarrativeGameUserSettings.h"
#include "Engine/World.h"
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


void UPlayerInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!FMath::IsFinite(DeltaTime) || DeltaTime < 0.f || !GetWorld()) { return; }
	ANarrativeCharacter* CurrentPawn = OwningController ? Cast<ANarrativeCharacter>(OwningController->GetPawn()) : nullptr;
	if (CurrentPawn != OwningPawn) { ClearViewedInteractable(); OwningPawn = CurrentPawn; }
	if (!IsActive() || !OwningPawn) { return; }
	if (GetWorld()->TimeSince(LastInteractionCheckTime) >= InteractionCheckFrequency) { PerformInteractionCheck(DeltaTime); }
	if (!bInteractHeld || !IsValid(ViewedInteractable) || GetOwnerRole() < ROLE_Authority || RemainingInteractTime <= -998.f) { return; }
	if (!IsInteractableInReach(ViewedInteractable)) { EndInteract(); return; }
	RemainingInteractTime -= DeltaTime;
	if (RemainingInteractTime <= 0.f) { CompletePendingInteraction(); }
}
void UPlayerInteractionComponent::CompletePendingInteraction()
{
	if (!bInteractHeld || GetOwnerRole() < ROLE_Authority || !IsInteractableInReach(ViewedInteractable)
		|| RemainingInteractTime > 0.f || RemainingInteractTime <= -998.f) { return; }
	TWeakObjectPtr<UNarrativeInteractableComponent> Target = ViewedInteractable;
	ANarrativeCharacter* Pawn = OwningPawn;
	RemainingInteractTime = -999.f; // Consume before callbacks; no recursive or repeated completion.
	FText Error;
	if (!Target->CanInteract(Pawn, this, Error) || !Target.IsValid() || ViewedInteractable != Target.Get()
		|| !OwningController || OwningController->GetPawn() != Pawn || !IsInteractableInReach(Target.Get())) { EndInteract(); return; }
	if (!Target->InteractionSlots.IsEmpty())
	{
		const int32 Slot = Target->GetBestAvailableSlot(this, Target->GetAvailableSlots(this, true));
		if (Target.IsValid() && Target->SlotStatuses.IsValidIndex(Slot))
		{
			UNarrativeInteractionComponent* Previous = Target->SlotStatuses[Slot].SlotStatus == EInteractionSlotStatus::ISS_Occupied
				? Target->SlotStatuses[Slot].SlotUser.Get() : nullptr;
			if (ClaimInteractionSlot(Target.Get(), Slot)) { RunInteractBehavior(IsValid(Previous), Previous); }
		}
	}
	else if (Target->Interact(Pawn, this) && Target.IsValid())
	{
		OnBeginUseInteractable.Broadcast(Target->GetOwner(), Target.Get());
		if (Target.IsValid()) { OnFinishUseInteractable.Broadcast(Target->GetOwner(), Target.Get()); }
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

bool UPlayerInteractionComponent::IsInteractableInReach(UNarrativeInteractableComponent* Target) const
{
	if (!IsValid(Target) || !Target->IsActive() || !IsValid(Target->GetOwner()) || !OwningController || !OwningPawn
		|| OwningController->GetPawn() != OwningPawn || Target->GetWorld() != GetWorld()
		|| !FMath::IsFinite(Target->InteractionDistance) || Target->InteractionDistance <= 0.f
		|| !FMath::IsFinite(Target->MaxViewAngleDegrees)) { return false; }
	const FBox Bounds = Target->GetInteractableBounds();
	const FVector Focus = Bounds.GetCenter();
	const FVector Closest = Bounds.GetClosestPointTo(OwningPawn->GetActorLocation());
	if (!Bounds.IsValid || Focus.ContainsNaN() || FVector::DistSquared(Closest, OwningPawn->GetActorLocation())
		> FMath::Square(FMath::Min(Target->InteractionDistance, InteractionCheckDistance))) { return false; }
	FVector Eye; FRotator Rotation; OwningController->GetPlayerViewPoint(Eye, Rotation);
	const FVector ToTarget = Focus - Eye;
	if (ToTarget.ContainsNaN() || FVector::DotProduct(Rotation.Vector(), ToTarget.GetSafeNormal())
		< FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Target->MaxViewAngleDegrees, 1.f, 85.f)))) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(NarrativeInteractReach), false, OwningPawn);
	TArray<AActor*> Attached; OwningPawn->GetAttachedActors(Attached, true, true);
	Query.AddIgnoredActors(Attached);
	const UArsenalSettings* Settings = UArsenalStatics::GetNarrativeProSettings();
	FHitResult Hit;
	const bool Blocked = GetWorld()->LineTraceSingleByChannel(Hit, Eye, Focus,
		Settings ? Settings->InteractionTraceChannel.GetValue() : ECC_Visibility, Query);
	return !Blocked || Hit.GetActor() == Target->GetOwner();
}
void UPlayerInteractionComponent::PerformInteractionCheck(float DeltaTime)
{
	LastInteractionCheckTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	UInteractionSubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UInteractionSubsystem>() : nullptr;
	if (!Registry || !OwningController || !OwningPawn) { ClearViewedInteractable(); return; }
	FVector Eye; FRotator Rotation; OwningController->GetPlayerViewPoint(Eye, Rotation);
	UNarrativeInteractableComponent* Best = nullptr; float BestScore = -FLT_MAX; bool bBestAdmits = false;
	for (UNarrativeInteractableComponent* Candidate : Registry->GetInteractableActors())
	{
		if (!IsInteractableInReach(Candidate)) { continue; }
		const FVector Focus = Candidate->GetInteractableBounds().GetCenter();
		const float Facing = FVector::DotProduct(Rotation.Vector(), (Focus - Eye).GetSafeNormal());
		const float Distance = FVector::Distance(Focus, OwningPawn->GetActorLocation());
		const float Score = FMath::Clamp(Candidate->InteractionPriority, -100, 100) * 4.f + Facing * 2.f
			- Distance / FMath::Max(1.f, Candidate->InteractionDistance);
		// A prompt the player cannot use never displaces one they can: a living hostile refuses interaction,
		// so it must not take the prompt from a control beside it. Refusals still win when nothing admits.
		FText Refusal; const bool bAdmits = Candidate->CanInteract(OwningPawn, this, Refusal);
		if (Best && bBestAdmits && !bAdmits) { continue; }
		const bool bBetter = !Best || (bAdmits && !bBestAdmits) || Score > BestScore
			|| (FMath::IsNearlyEqual(Score, BestScore) && Candidate->GetPathName() < Best->GetPathName());
		if (bBetter) { Best = Candidate; BestScore = Score; bBestAdmits = bAdmits; }
	}
	if (Best) { SetViewedInteractable(Best); } else { ClearViewedInteractable(); }
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
	if (!IsValid(Interactable)) { ClearViewedInteractable(); return; }
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
	if (GetOwnerRole() < ROLE_Authority) { ServerBeginInteract(); }
	ANarrativeCharacter* CurrentPawn = OwningController ? Cast<ANarrativeCharacter>(OwningController->GetPawn()) : nullptr;
	if (CurrentPawn != OwningPawn) { ClearViewedInteractable(); OwningPawn = CurrentPawn; }
	if (!IsActive() || !OwningPawn || bInteractHeld) { return; }
	PerformInteractionCheck(0.f);
	TWeakObjectPtr<UNarrativeInteractableComponent> Target = ViewedInteractable;
	ANarrativeCharacter* Pawn = OwningPawn;
	FText Error;
	if (!IsInteractableInReach(Target.Get()) || !Target->CanInteract(Pawn, this, Error)
		|| !Target.IsValid() || Target.Get() != ViewedInteractable || OwningController->GetPawn() != Pawn) { return; }
	const UNarrativeGameUserSettings* Settings = UNarrativeGameUserSettings::GetSovPlayerSettings(Pawn);
	const float Scale = Settings ? FMath::Clamp(Settings->GetInteractionHoldScale(), 0.1f, 1.f) : 1.f;
	RemainingInteractTime = Settings && Settings->UseTapInteractions() ? 0.f : FMath::Max(0.f, Target->InteractionTime) * Scale;
	bInteractHeld = true;
	OnInteractPressed.Broadcast(this);
	if (bInteractHeld && Target.IsValid() && Target.Get() == ViewedInteractable && OwningController->GetPawn() == Pawn)
	{
		Target->BeginInteract(Pawn, this);
		if (Target.IsValid() && Target.Get() == ViewedInteractable && OwningController->GetPawn() == Pawn)
		{ CompletePendingInteraction(); }
	}
	else { EndInteract(); }
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
