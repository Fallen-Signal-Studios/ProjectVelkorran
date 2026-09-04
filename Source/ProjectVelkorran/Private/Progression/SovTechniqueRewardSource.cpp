// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Progression/SovTechniqueRewardSource.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEncounterDirector.h"
#include "Framework/SovPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

bool USovTechniqueRewardSource::HasNativeProof(const ASovPlayerState* Player) const
{
	AActor* SourceActor = GetOwner();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!IsValid(Player) || !Player->HasAuthority() || !IsValid(SourceActor) || !SourceActor->HasAuthority()
		|| !IsValid(Pawn) || SourceActor->GetWorld() != Player->GetWorld() || RewardId.IsNone()
		|| !FMath::IsFinite(MaximumClaimDistance) || MaximumClaimDistance <= 0.f
		|| FVector::DistSquared(Pawn->GetActorLocation(), SourceActor->GetActorLocation()) > FMath::Square(MaximumClaimDistance)) { return false; }
	if (Proof == ESovTechniqueRewardProof::EncounterComplete)
	{
		return IsValid(Encounter) && Encounter->GetWorld() == Player->GetWorld()
			&& Encounter->GetEncounterState() == ESovEncounterState::Succeeded;
	}
	const AController* Controller = Pawn->GetController();
	const USovCampaignStateComponent* Campaign = Controller ? Controller->FindComponentByClass<USovCampaignStateComponent>() : nullptr;
	if (!Campaign || !Campaign->IsStateValid() || MissionId.IsNone()) { return false; }
	if (Proof == ESovTechniqueRewardProof::MissionComplete) { return Campaign->IsMissionComplete(MissionId); }
	return Proof == ESovTechniqueRewardProof::BeatComplete && !BeatId.IsNone() && Campaign->IsBeatComplete(MissionId, BeatId);
}
