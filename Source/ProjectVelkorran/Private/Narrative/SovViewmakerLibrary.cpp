// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Narrative/SovViewmakerLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Campaign/SovEvidenceSourceComponent.h"
#include "Campaign/SovEvidencePolicy.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Components/SovWeakPointComponent.h"
#include "Components/SovCommandLinkComponent.h"
#include "Companions/SovCompanionComponent.h"
#include "GAS/NarrativeAttributeSetBase.h"
#include "Sovereign/SovGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Character/NarrativeCharacterVisual.h"
namespace
{
	bool VisibleToViewmaker(APlayerController* Player, AActor* Target)
	{
		if (!IsValid(Player) || !IsValid(Target) || !Player->GetPawn() || Target->GetWorld() != Player->GetWorld() || Target->IsActorBeingDestroyed()) { return false; }
		FVector Eye; FRotator View; Player->GetPlayerViewPoint(Eye, View);
		const FVector Offset = Target->GetActorLocation() - Eye;
		if (!SovEvidencePolicy::InScanCone(Offset.Size(), 1000.0f, Offset.IsNearlyZero() ? 1.0f : FVector::DotProduct(View.Vector(), Offset.GetSafeNormal()), 0.8660254)) { return false; }
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SovViewmakerLOS), false, Player->GetPawn());
		TArray<AActor*> Attached; Player->GetPawn()->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
		if (const auto* Character = Cast<ASovPlayerCharacterBase>(Player->GetPawn()))
		{ if (AActor* Visual = Character->GetCharacterVisual()) { Query.AddIgnoredActor(Visual); } }
		Query.AddIgnoredActor(Target); Attached.Reset(); Target->GetAttachedActors(Attached, true, true); Query.AddIgnoredActors(Attached);
		FHitResult Hit;
		return !Player->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target->GetActorLocation(), ECC_Visibility, Query);
	}
}
bool USovViewmakerLibrary::ScanTarget(APlayerController* Player, AActor* Target, FSovViewmakerScanResult& OutResult)
{
	OutResult = FSovViewmakerScanResult();
	if (!IsValid(Player) || !Player->HasAuthority() || !VisibleToViewmaker(Player, Target)) { return false; }
	auto* Pawn = Cast<ASovPlayerCharacterBase>(Player->GetPawn());
	const auto* State = Player->FindComponentByClass<USovCampaignStateComponent>();
	if (!Pawn || !Pawn->IsCharacterReady() || !Pawn->IsAlive() || Pawn->GetProtagonistIdentityTag() != FSovGameplayTags::Get().Character_Player_Selene
		|| !State || !State->IsStateValid() || State->GetActiveProtagonist() != Pawn->GetProtagonistIdentityTag()) { return false; }
	FSovViewmakerScanResult Result; Result.Target = Target;
	if (const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		Result.bLiveSystem = ASC->GetAvatarActor() == Target && ASC->GetNumericAttribute(UNarrativeAttributeSetBase::GetHealthAttribute()) > 0.0f;
	}
	if (auto* Evidence = Target->FindComponentByClass<USovEvidenceSourceComponent>())
	{
		Result.bEvidenceAcquired = Evidence->TryScan(Player);
		if (Result.bEvidenceAcquired) { Result.AuthoredTraceIds = Evidence->AuthoredTraceIds; Result.RouteHint = Evidence->AuthoredRouteHint; }
		if (Player->GetPawn() != Pawn || !Pawn->IsCharacterReady() || !Pawn->IsAlive() || !IsValid(Target) || !State->IsStateValid()) { return false; }
	}
	if (Result.bLiveSystem)
	{
		if (auto* WeakPoints = Target->FindComponentByClass<USovWeakPointComponent>()) { Result.bWeakPointsRevealed = WeakPoints->RevealWeakPoints(5.0f, Pawn); }
		if (auto* Link = Target->FindComponentByClass<USovCommandLinkComponent>(); Link && Link->IsCommandLinkActive())
		{
			Result.LinkId = Link->GetLinkId();
			for (AActor* Member : Link->GetLinkedActors()) { if (VisibleToViewmaker(Player, Member)) { Result.VisibleLinkedActors.AddUnique(Member); } }
		}
	}
	if (!Result.bLiveSystem && !Result.bEvidenceAcquired) { return false; }
	OutResult = MoveTemp(Result); return true;
}
bool USovViewmakerLibrary::RequestCompanionAnalysis(APlayerController* Player, AActor* Target, USovCompanionComponent* Companion, FString& OutReason)
{
	FSovViewmakerScanResult Result;
	if (!IsValid(Companion) || !ScanTarget(Player, Target, Result)) { OutReason = TEXT("The analysis target is not currently visible to Selene's viewmaker."); return false; }
	return Companion->RequestCommand(Cast<ASovPlayerCharacterBase>(Player->GetPawn()), ESovCompanionCommand::Interact, Target, OutReason);
}
