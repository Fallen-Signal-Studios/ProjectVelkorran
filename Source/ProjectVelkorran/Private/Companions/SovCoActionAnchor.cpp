// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovCoActionAnchor.h"
#include "Companions/SovCompanionComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

ASovCoActionAnchor::ASovCoActionAnchor()
{
	CompanionMark = CreateDefaultSubobject<USceneComponent>(TEXT("CompanionMark"));
	SetRootComponent(CompanionMark);
	PrimaryActorTick.bCanEverTick = false;
}

FVector ASovCoActionAnchor::GetCompanionLocation() const
{
	return CompanionMark ? CompanionMark->GetComponentLocation() : GetActorLocation();
}

bool ASovCoActionAnchor::IsCompanionAtMark(const AActor* CompanionActor) const
{
	const APawn* Pawn = Cast<APawn>(CompanionActor);
	const FVector Position = Pawn ? Pawn->GetNavAgentLocation() : FVector::ZeroVector;
	const FVector Mark = GetCompanionLocation();
	return IsValid(Pawn) && !Position.ContainsNaN() && !Mark.ContainsNaN()
		&& FVector::DistSquared(Position, Mark) <= FMath::Square(ReachRadius);
}

bool ASovCoActionAnchor::ValidatePermission(ASovPlayerCharacterBase* Player, FName CompanionId, FString& Reason) const
{
	if (!HasAuthority() || !IsValid(Player) || !Player->IsCharacterReady() || !Player->IsAlive() || Player->GetWorld() != GetWorld()
		|| AnchorId.IsNone() || MissionId.IsNone() || CompletionBeat.IsNone() || RequiredCompanionId.IsNone()
		|| CompanionId != RequiredCompanionId || !Player->GetController() || Player->GetController()->GetPawn() != Player
		|| Player->GetActorLocation().ContainsNaN() || GetCompanionLocation().ContainsNaN()
		|| !FMath::IsFinite(RequestRange) || RequestRange <= 0.f || !FMath::IsFinite(ReachRadius) || ReachRadius <= 0.f
		|| !FMath::IsFinite(TimeoutSeconds) || TimeoutSeconds <= 0.f || !FMath::IsFinite(HoldAtMarkSeconds)
		|| HoldAtMarkSeconds < 0.f || HoldAtMarkSeconds >= TimeoutSeconds)
	{
		Reason = TEXT("Co-action identity, player, or anchor tuning is invalid."); return false;
	}
	for (TActorIterator<ASovCoActionAnchor> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->AnchorId == AnchorId) { Reason = TEXT("Co-action anchor IDs must be unique."); return false; }
	}
	USovCampaignStateComponent* State = Player->GetController()->FindComponentByClass<USovCampaignStateComponent>();
	USovCampaignDefinition* Mission = State ? State->GetActiveMission() : nullptr;
	const FSovCampaignBeatDefinition* Beat = Mission ? Mission->FindBeat(CompletionBeat) : nullptr;
	if (!State || !State->IsStateValid() || !Mission || Mission->MissionId != MissionId
		|| State->GetActiveProtagonist() != Player->GetProtagonistIdentityTag() || !Beat || !Beat->bRequiresCoActionProof
		|| Beat->RequiredCompanionId != RequiredCompanionId || Beat->RequiredCoActionAnchorId != AnchorId
		|| !Beat->CinematicId.IsNone() || State->IsBeatComplete(MissionId, CompletionBeat))
	{
		Reason = TEXT("This companion action is not available in the current mission beat."); return false;
	}
	for (const FName Prior : Beat->PrerequisiteBeats)
	{
		if (!State->IsBeatComplete(MissionId, Prior)) { Reason = TEXT("A prerequisite beat has not completed."); return false; }
	}
	for (const FSovCampaignStateWrite& Required : Beat->RequiredState)
	{
		if (State->GetStateValue(Required.Key) != Required.Value) { Reason = TEXT("Mission state does not permit this co-action."); return false; }
	}
	if (!State->HasKnowledge(State->GetActiveProtagonist(), Beat->RequiredKnowledge)) { Reason = TEXT("The protagonist lacks required knowledge."); return false; }
	return true;
}

bool ASovCoActionAnchor::CommitArrival(USovCompanionComponent* Companion, ASovPlayerCharacterBase* Player, FGuid RequestId)
{
	FString Reason;
	if (bReceiptAvailable || !IsValid(Companion) || !Companion->GetOwner() || !RequestId.IsValid()
		|| !ValidatePermission(Player, Companion->CompanionId, Reason)
		|| !IsCompanionAtMark(Companion->GetOwner())) { return false; }
	ReceiptCompanion = Companion;
	ReceiptPlayer = Player;
	ReceiptRequestId = RequestId;
	bReceiptAvailable = true;
	USovCampaignStateComponent* State = Player->GetController()->FindComponentByClass<USovCampaignStateComponent>();
	const ESovCampaignResult Result = State->CompleteCoAction(this);
	bReceiptAvailable = false;
	ReceiptCompanion = nullptr;
	ReceiptPlayer = nullptr;
	ReceiptRequestId.Invalidate();
	return Result == ESovCampaignResult::Applied;
}

bool ASovCoActionAnchor::ConsumeReceipt(USovCampaignStateComponent* State)
{
	FString Reason;
	if (!bReceiptAvailable || !IsValid(State) || !IsValid(ReceiptCompanion) || !IsValid(ReceiptPlayer)
		|| ReceiptPlayer->GetController() != State->GetOwner() || !ReceiptRequestId.IsValid()
		|| ReceiptCompanion->RequestId != ReceiptRequestId || ReceiptCompanion->ActiveAnchor != this
		|| ReceiptCompanion->CommandState != ESovCompanionCommandState::MovingToAnchor
		|| !ValidatePermission(ReceiptPlayer, ReceiptCompanion->CompanionId, Reason)
		|| !ReceiptCompanion->GetOwner()
		|| !IsCompanionAtMark(ReceiptCompanion->GetOwner())
		|| !ReceiptCompanion->ValidateRequest(ReceiptPlayer, this, true, Reason)) { return false; }
	bReceiptAvailable = false;
	return true;
}
