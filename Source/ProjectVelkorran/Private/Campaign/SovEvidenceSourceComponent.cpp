// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Campaign/SovEvidencePolicy.h"
#include "Sovereign/SovGameplayTags.h"
#include "GameFramework/Pawn.h"
#include "Characters/SovPlayerCharacterBase.h"

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
	if (bAcquiring || bRequiresViewmaker || !IsValid(Player) || !Player->HasAuthority()) { return false; }
	USovCampaignStateComponent* State = Player->FindComponentByClass<USovCampaignStateComponent>();
	if (!State) { return false; }
	TGuardValue<bool> Guard(bAcquiring, true);
	const ESovCampaignResult Result = State->AcquireEvidence(this);
	return Result == ESovCampaignResult::Applied || Result == ESovCampaignResult::AlreadyApplied;
}

bool USovEvidenceSourceComponent::ValidateAcquisitionMode(APlayerController* Player) const
{
	if (!IsValid(Player) || !Player->HasAuthority() || !Player->GetPawn() || !IsValid(GetOwner())) { return false; }
	const auto* State = Player->FindComponentByClass<USovCampaignStateComponent>();
	if (!State || !State->HasKnowledge(State->GetActiveProtagonist(), RequiredQueryKnowledge)) { return false; }
	if (!bRequiresViewmaker && ScanningPlayer.Get() != Player) { return true; }
	const auto* Pawn = Cast<ASovPlayerCharacterBase>(Player->GetPawn());
	if (!Pawn || !Pawn->IsCharacterReady() || !Pawn->IsAlive() || ScanningPlayer.Get() != Player || !bAcquiring || State->GetActiveProtagonist() != FSovGameplayTags::Get().Character_Player_Selene
		|| !FMath::IsFinite(ScanHalfAngleDegrees) || ScanHalfAngleDegrees < 1.0f || ScanHalfAngleDegrees > 80.0f) { return false; }
	FVector Location; FRotator Rotation; Player->GetPlayerViewPoint(Location, Rotation);
	const FVector Offset = GetOwner()->GetActorLocation() - Location;
	return SovEvidencePolicy::InScanCone(Offset.Size(), InteractionRange,
		Offset.IsNearlyZero() ? 1.0f : FVector::DotProduct(Rotation.Vector(), Offset.GetSafeNormal()), FMath::Cos(FMath::DegreesToRadians(ScanHalfAngleDegrees)));
}
bool USovEvidenceSourceComponent::TryScan(APlayerController* Player)
{
	if (bAcquiring || !IsValid(Player) || !Player->HasAuthority()) { return false; }
	auto* State = Player->FindComponentByClass<USovCampaignStateComponent>();
	if (!State || State->GetActiveProtagonist() != FSovGameplayTags::Get().Character_Player_Selene) { return false; }
	TGuardValue<bool> Guard(bAcquiring, true);
	ScanningPlayer = Player;
	const auto Result = State->AcquireEvidence(this);
	ScanningPlayer.Reset();
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


bool USovEvidenceSourceComponent::ValidateConfiguration(FString& OutError) const
{
	const auto Fail = [&OutError]() { OutError = TEXT("Evidence source requires a unique identity, bounded acquisition metadata and a provenance-compatible canonical definition."); return false; };
	if (EvidenceId.IsNone() || !SourceId.IsValid() || AcquisitionMission.IsNone() || AllowedProtagonists.IsEmpty()
		|| RequestedStage < ESovEvidenceStage::Observed || RequestedStage > ESovEvidenceStage::Distributed
		|| Publicity > ESovRecordPublicity::Public || WitnessIds.Num() > 32 || AuthoredTraceIds.Num() > 64
		|| !FMath::IsFinite(InteractionRange) || InteractionRange <= 0.f || InteractionRange > 5000.f
		|| !FMath::IsFinite(ScanHalfAngleDegrees) || ScanHalfAngleDegrees < 1.f || ScanHalfAngleDegrees > 80.f) { return Fail(); }
	const auto& Tags = FSovGameplayTags::Get();
	for (FGameplayTag Hero : AllowedProtagonists)
	{ if (Hero != Tags.Character_Player_Tarrik && Hero != Tags.Character_Player_Selene) { return Fail(); } }
	for (const auto* List : { &WitnessIds, &AuthoredTraceIds })
	{
		TSet<FName> Seen;
		for (FName Id : *List) { if (Id.IsNone() || Seen.Contains(Id)) { return Fail(); } Seen.Add(Id); }
	}
	if (!Definition)
	{
		if (RequestedStage != ESovEvidenceStage::Observed || !SourceLocationId.IsNone() || !CustodianId.IsNone()
			|| !SupportingEvidenceId.IsNone() || !CopyDestination.IsNone() || !WitnessIds.IsEmpty() || Publicity != ESovRecordPublicity::Private) { return Fail(); }
	}
	else
	{
		if (!Definition->ValidateDefinition(OutError) || Definition->EvidenceId != EvidenceId
			|| !Definition->RelevantMissions.Contains(AcquisitionMission) || SourceLocationId.IsNone() || !Definition->SourceCustodians.Contains(CustodianId)) { return Fail(); }
		if (RequestedStage == ESovEvidenceStage::Corroborated)
		{ if (!Definition->SupportingEvidenceIds.Contains(SupportingEvidenceId)) { return Fail(); } }
		else if (!SupportingEvidenceId.IsNone()) { return Fail(); }
		if (RequestedStage == ESovEvidenceStage::Distributed)
		{ if (CopyDestination != CustodianId || !Definition->CopyDestinations.Contains(CopyDestination)) { return Fail(); } }
		else if (!CopyDestination.IsNone()) { return Fail(); }
	}
	OutError.Reset(); return true;
}
