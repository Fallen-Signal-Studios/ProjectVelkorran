// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Corruption/SovCorruptionInteractableComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SovCorruptionComponent.h"
#include "Corruption/SovCorruptionSourceComponent.h"
#include "Corruption/SovCorruptionMath.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Interaction/InteractionComponent.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

USovCorruptionInteractableComponent::USovCorruptionInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	InteractionTime = 0.0f;
}
bool USovCorruptionInteractableComponent::ValidateInteractor(APawn* Player, FName& OutMission) const
{
	if (!IsValid(Player) || !Player->GetController() || Player->GetController()->GetPawn() != Player
		|| !IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !IsActive() || !IsValid(Profile)
		|| !FMath::IsFinite(InteractionDistance) || InteractionDistance <= 0.0f
		|| Player->GetWorld() != GetWorld() || GetOwner()->IsActorBeingDestroyed()
		|| FVector::DistSquared(Player->GetActorLocation(), GetOwner()->GetActorLocation()) > FMath::Square(InteractionDistance)) { return false; }
	auto* Component = Player->FindComponentByClass<USovCorruptionComponent>();
	ESovCorruptionBand Cap;
	if (!Component || !Component->ValidateProfilePermission(Profile, Cap, OutMission) || ConsumedMissions.Contains(OutMission)) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CorruptionInteractionLOS), false, GetOwner());
	Query.AddIgnoredActor(Player);
	TArray<AActor*> Attached;
	Player->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Player->GetActorLocation(), GetOwner()->GetActorLocation(), ECC_Visibility, Query);
}
bool USovCorruptionInteractableComponent::ValidateSignal(APawn* Player) const
{
	const auto* Source = IsValid(SourceActor) ? SourceActor->FindComponentByClass<USovCorruptionSourceComponent>() : nullptr;
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(SourceActor);
	const auto* Team = Cast<INarrativeTeamAgentInterface>(Player);
	float Falloff;
	return Source && Source->Profile == Profile && Profile->Escape == ESovCorruptionEscape::ProtectSignal
		&& ASC && ASC->GetAvatarActor() == SourceActor && ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.0f
		&& Team && Team->GetTeamAttitudeTowards(*SourceActor) == ETeamAttitude::Friendly
		&& Source->ValidateContact(Player, Falloff) && FMath::IsFinite(ProtectionSeconds) && ProtectionSeconds >= 1.0f && ProtectionSeconds <= 60.0f;
}
bool USovCorruptionInteractableComponent::CanInteract_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp, FText& ErrorMessage)
{
	FName Mission;
	if (bApplying || !ValidateInteractor(Interactor, Mission) || !Super::CanInteract_Implementation(Interactor, InteractionComp, ErrorMessage))
	{
		ErrorMessage = FText::FromString(TEXT("This corruption interaction is not currently available.")); return false;
	}
	if (Action == ESovCorruptionInteraction::CompromisedMachinery)
	{
		return Profile->SourceKind == ESovCorruptionSourceKind::Machinery
			&& SovCorruptionMath::Falloff(FVector::Distance(GetOwner()->GetActorLocation(), Interactor->GetActorLocation()), Profile->Radius, Profile->bLinearFalloff) > 0.0f;
	}
	if (Action == ESovCorruptionInteraction::ProtectSignal) { return !ProtectingPlayer.IsValid() && ValidateSignal(Interactor); }
	if (Action == ESovCorruptionInteraction::Countermeasure && Profile->Escape == ESovCorruptionEscape::AuthoredCountermeasure)
	{
		const auto* Source = IsValid(SourceActor) ? SourceActor->FindComponentByClass<USovCorruptionSourceComponent>() : nullptr;
		return !SourceActor || (Source && Source->Profile == Profile);
	}
	return false;
}
void USovCorruptionInteractableComponent::OnInteract_Implementation(APawn* Interactor, UNarrativeInteractionComponent* InteractionComp)
{
	Super::OnInteract_Implementation(Interactor, InteractionComp);
	FName Mission;
	if (bApplying || !IsValid(InteractionComp) || !IsValid(Interactor)
		|| (InteractionComp->GetOwner() != Interactor && InteractionComp->GetOwner() != Interactor->GetController())
		|| !ValidateInteractor(Interactor, Mission)) { return; }
	TGuardValue<bool> Guard(bApplying, true);
	if (Action == ESovCorruptionInteraction::CompromisedMachinery)
	{
		if (Profile->SourceKind != ESovCorruptionSourceKind::Machinery) { return; }
		ConsumedMissions.Add(Mission);
		const float Falloff = SovCorruptionMath::Falloff(FVector::Distance(GetOwner()->GetActorLocation(), Interactor->GetActorLocation()), Profile->Radius, Profile->bLinearFalloff);
		Interactor->FindComponentByClass<USovCorruptionComponent>()->ApplyVerifiedPulse(Profile, GetOwner(), Falloff);
	}
	else if (Action == ESovCorruptionInteraction::Countermeasure && Profile->Escape == ESovCorruptionEscape::AuthoredCountermeasure)
	{
		ConsumedMissions.Add(Mission);
		CompleteRemedy(Interactor, ESovCorruptionEscape::AuthoredCountermeasure);
	}
	else if (Action == ESovCorruptionInteraction::ProtectSignal && !ProtectingPlayer.IsValid() && ValidateSignal(Interactor))
	{
		ProtectingPlayer = Interactor; ProtectingMission = Mission; ProtectionElapsed = 0.0f;
		ProtectionDamageSequence = SourceActor->FindComponentByClass<USovCorruptionSourceComponent>()->DamageSequence;
	}
}
bool USovCorruptionInteractableComponent::CompleteRemedy(APawn* Player, ESovCorruptionEscape Remedy)
{
	if (IsValid(SourceActor))
	{
		auto* Source = SourceActor->FindComponentByClass<USovCorruptionSourceComponent>();
		return Source && Source->Profile == Profile && Source->ResolveRemedy(Player, Remedy);
	}
	auto* Component = Player ? Player->FindComponentByClass<USovCorruptionComponent>() : nullptr;
	return Component && Component->ApplyVerifiedRemedy(Profile, Remedy);
}
void USovCorruptionInteractableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
	Super::TickComponent(DeltaTime, TickType, TickFunction);
	if (bApplying || !ProtectingPlayer.IsValid() || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	auto* Player = ProtectingPlayer.Get(); FName Mission;
	if (!ValidateInteractor(Player, Mission) || Mission != ProtectingMission || !ValidateSignal(Player))
	{
		ProtectingPlayer.Reset(); ProtectionElapsed = 0.0f; return;
	}
	auto* Source = SourceActor->FindComponentByClass<USovCorruptionSourceComponent>();
	const bool bDamaged = Source->DamageSequence != ProtectionDamageSequence;
	ProtectionDamageSequence = Source->DamageSequence;
	ProtectionElapsed = SovCorruptionMath::AdvanceProtection(ProtectionElapsed, DeltaTime, ProtectionSeconds, bDamaged);
	if (ProtectionElapsed >= ProtectionSeconds)
	{
		TGuardValue<bool> Guard(bApplying, true);
		ProtectingPlayer.Reset(); ConsumedMissions.Add(Mission);
		CompleteRemedy(Player, ESovCorruptionEscape::ProtectSignal);
	}
}
void USovCorruptionInteractableComponent::Load_Implementation()
{
	ProtectingPlayer.Reset(); ProtectingMission = NAME_None; ProtectionElapsed = 0.0f; ProtectionDamageSequence = 0;
}
void USovCorruptionInteractableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ProtectingPlayer.Reset(); Super::EndPlay(Reason);
}
