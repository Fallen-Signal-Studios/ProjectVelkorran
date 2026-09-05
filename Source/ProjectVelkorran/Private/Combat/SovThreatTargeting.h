// Copyright Fallen Signal Studios. All Rights Reserved.
#pragma once
#include "AI/NarrativeNPCController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "NarrativeGameplayTags.h"
namespace SovThreatTargeting
{
/** Deliberate acquisition/steering only. Never use this as a damage immunity predicate. */
inline bool CanTrack(AActor* Source, AActor* Target)
{
	if (!IsValid(Source) || !IsValid(Target)) { return false; }
	const auto* Pawn = Cast<APawn>(Source);
	const auto* Controller = Pawn ? Cast<ANarrativeNPCController>(Pawn->GetController()) : nullptr;
	if (Controller) { return Controller->CanDirectlyTargetThreat(Target); }
	const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	return !Target->IsHidden() && (!ASC || !ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_InvisibleToEnemies));
}
inline bool CanUseActorFocus(AActor* Source)
{
	const auto* Pawn = Cast<APawn>(Source);
	const auto* Controller = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	AActor* Focus = Controller ? Controller->GetFocusActor() : nullptr;
	// A fixed focal position is permitted for suppression/investigation; it does not track a hidden actor.
	return !Focus || CanTrack(Source, Focus);
}
}
