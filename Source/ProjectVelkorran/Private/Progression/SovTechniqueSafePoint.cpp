// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Progression/SovTechniqueSafePoint.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Components/BoxComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/SovPlayerState.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "GameFramework/Pawn.h"
#include "NarrativeGameplayTags.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeTeamAgentInterface.h"

ASovTechniqueSafePoint::ASovTechniqueSafePoint()
{
	SafeBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SafeBounds"));
	SetRootComponent(SafeBounds);
	SafeBounds->SetBoxExtent(FVector(250.f, 250.f, 180.f));
	SafeBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SafeBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	SafeBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PrimaryActorTick.bCanEverTick = false;
}
bool ASovTechniqueSafePoint::AllowsModification(const ASovPlayerState* Player) const
{
	const APawn* Pawn = IsValid(Player) ? Player->GetPawn() : nullptr;
	if (!HasAuthority() || !IsValid(Player) || !Player->HasAuthority() || !IsValid(Pawn) || !SafeBounds
		|| GetWorld() != Player->GetWorld() || SafePointId.IsNone() || IsActorBeingDestroyed()
		|| !FMath::IsFinite(HostileExclusionRadius) || HostileExclusionRadius < 100.f) { return false; }
	const FVector Local = SafeBounds->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
	const FVector Extent = SafeBounds->GetUnscaledBoxExtent();
	if (Local.ContainsNaN() || FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || FMath::Abs(Local.Z) > Extent.Z) { return false; }
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ASovPlayerState*>(Player));
	if (!ASC || ASC->GetAvatarActor() != Pawn || ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) <= 0.f) { return false; }
	const auto& N = FNarrativeGameplayTags::Get(); const auto& S = FSovGameplayTags::Get();
	FGameplayTagContainer Blocked;
	Blocked.AddTag(N.State_IsDead); Blocked.AddTag(N.State_Busy); Blocked.AddTag(N.State_SequencerControlled);
	Blocked.AddTag(N.State_Interacting); Blocked.AddTag(N.State_Weapon_Equipping); Blocked.AddTag(N.State_Movement_Ragdoll);
	Blocked.AddTag(S.State_Fatal); Blocked.AddTag(S.State_EchoAbility_Active); Blocked.AddTag(S.State_Guarding);
	Blocked.AddTag(S.State_Deflecting); Blocked.AddTag(S.State_Guard_Broken); Blocked.AddTag(S.State_Poise_Broken);
	if (ASC->HasAnyMatchingGameplayTags(Blocked)) { return false; }
	for (TActorIterator<ASovEncounterDirector> It(GetWorld()); It; ++It)
	{
		if (It->GetEncounterState() == ESovEncounterState::Active || It->GetEncounterState() == ESovEncounterState::Restoring) { return false; }
	}
	const auto* Team = Cast<INarrativeTeamAgentInterface>(Pawn);
	if (!Team) { return false; }
	FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovTechniqueSafePoint), false, Pawn);
	TArray<FOverlapResult> Nearby;
	GetWorld()->OverlapMultiByObjectType(Nearby, Pawn->GetActorLocation(), FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(HostileExclusionRadius), Query);
	for (const FOverlapResult& Overlap : Nearby)
	{
		AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Team->GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile) { continue; }
		const UAbilitySystemComponent* OtherASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
		if (!OtherASC || (!OtherASC->HasMatchingGameplayTag(N.State_IsDead)
			&& OtherASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.f)) { return false; }
	}
	return true;
}
