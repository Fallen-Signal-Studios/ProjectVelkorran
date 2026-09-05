// Copyright Fallen Signal Studios. All Rights Reserved.
#include "FieldRecovery/SovFieldRecoveryStation.h"
#include "FieldRecovery/SovFieldRecoveryComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/Controller.h"
#include "Interaction/InteractionComponent.h"

ASovFieldRecoveryStation::ASovFieldRecoveryStation()
{
	bRefillsFieldRecovery = true;
	Interaction = CreateDefaultSubobject<USovFieldRecoveryStationInteraction>(TEXT("FieldRecoveryInteraction"));
	SafeBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
USovFieldRecoveryStationInteraction::USovFieldRecoveryStationInteraction()
{
	InteractionTime = 0.f; InteractionDistance = 300.f;
	InteractableNameText = FText::FromString(TEXT("Field supply"));
	InteractableActionText = FText::FromString(TEXT("Refill recovery"));
}
bool USovFieldRecoveryStationInteraction::ValidatePlayer(APawn* Pawn, UNarrativeInteractionComponent* InteractionComp, FText& Error) const
{
	const auto* Player = Cast<ASovPlayerCharacterBase>(Pawn);
	const auto* Point = Cast<ASovFieldRecoveryStation>(GetOwner());
	const auto* Recovery = Player ? Player->GetFieldRecoveryComponent() : nullptr;
	if (bRefilling || !IsValid(Player) || !IsValid(Point) || !IsActive() || !Point->HasAuthority() || !Point->bRefillsFieldRecovery
		|| !IsValid(InteractionComp) || !Player->GetController() || Player->GetController()->GetPawn() != Player
		|| (InteractionComp->GetOwner() != Player && InteractionComp->GetOwner() != Player->GetController())
		|| Player->GetWorld() != GetWorld() || !Recovery || !Recovery->IsInitialized() || Recovery->IsUsing()
		|| Recovery->GetCharges() >= Recovery->Capacity || !FMath::IsFinite(InteractionDistance) || InteractionDistance <= 0.f
		|| FVector::DistSquared(Player->GetActorLocation(), Point->GetActorLocation()) > FMath::Square(InteractionDistance)
		|| !Point->AllowsModification(Player->GetPlayerState<ASovPlayerState>()))
	{ Error = FText::FromString(TEXT("Field recovery supplies are unavailable or already full.")); return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovFieldSupply), false, Point);
	Query.AddIgnoredActor(Player);
	TArray<AActor*> Attached; Player->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetActorLocation(), Point->GetActorLocation(), ECC_Visibility, Query))
	{ Error = FText::FromString(TEXT("The supply station is obstructed.")); return false; }
	return true;
}
bool USovFieldRecoveryStationInteraction::CanInteract_Implementation(APawn* Player, UNarrativeInteractionComponent* InteractionComp, FText& Error)
{
	return ValidatePlayer(Player, InteractionComp, Error) && Super::CanInteract_Implementation(Player, InteractionComp, Error);
}
bool USovFieldRecoveryStationInteraction::Interact(APawn* Player, UNarrativeInteractionComponent* InteractionComp)
{
	FText Error;
	if (!ValidatePlayer(Player, InteractionComp, Error) || !CanInteract(Player, InteractionComp, Error)) { return false; }
	TGuardValue<bool> Refilling(bRefilling, true);
	FString RefillError;
	if (!CastChecked<ASovPlayerCharacterBase>(Player)->GetFieldRecoveryComponent()->RefillAtSafePoint(CastChecked<ASovFieldRecoveryStation>(GetOwner()), RefillError)) { return false; }
	OnInteract(Player, InteractionComp); OnInteracted.Broadcast(Player, InteractionComp);
	return true;
}
