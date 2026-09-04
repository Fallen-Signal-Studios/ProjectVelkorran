// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

USovEvidenceSourceComponent::USovEvidenceSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USovEvidenceSourceComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
#if WITH_EDITOR
	// Author a stable identity into the placed component; runtime-spawned sources must supply one.
	if (!SourceId.IsValid() && !IsTemplate() && GetOwner() && !GetOwner()->IsTemplate()
		&& GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
	{
		SourceId = FGuid::NewGuid();
	}
#endif
}

bool USovEvidenceSourceComponent::TryAcquire(APlayerController* Player)
{
	if (!IsValid(Player) || !Player->HasAuthority()) { return false; }
	USovCampaignStateComponent* State = Player->FindComponentByClass<USovCampaignStateComponent>();
	if (!State) { return false; }
	const ESovCampaignResult Result = State->AcquireEvidence(this);
	return Result == ESovCampaignResult::Applied || Result == ESovCampaignResult::AlreadyApplied;
}

#if WITH_EDITOR
void USovEvidenceSourceComponent::PostEditImport()
{
	Super::PostEditImport();
	if (!IsTemplate() && GetOwner() && !GetOwner()->IsTemplate()) { SourceId = FGuid::NewGuid(); }
}
void USovEvidenceSourceComponent::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	if (DuplicateMode == EDuplicateMode::Normal && !IsTemplate() && GetOwner() && !GetOwner()->IsTemplate())
	{ SourceId = FGuid::NewGuid(); }
}
#endif
