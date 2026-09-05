// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Campaign/SovCampaignHandoffAnchor.h"
#include "Campaign/SovCampaignStateComponent.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "Framework/SovPlayerController.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"

ASovCampaignHandoffAnchor::ASovCampaignHandoffAnchor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Destination = CreateDefaultSubobject<USceneComponent>(TEXT("Destination")); Destination->SetupAttachment(RootComponent);
}

bool ASovCampaignHandoffAnchor::ValidateRequest(const ASovPlayerController* Controller, FTransform& OutDestination, FString& OutError) const
{
	const auto Fail = [&OutError](const TCHAR* Message) { OutError = Message; return false; };
	const ASovPlayerCharacterBase* Player = Controller ? Cast<ASovPlayerCharacterBase>(Controller->GetPawn()) : nullptr;
	const USovCampaignStateComponent* State = Controller ? Controller->GetCampaignState() : nullptr;
	const USovCampaignDefinition* Mission = State ? State->GetActiveMission() : nullptr;
	const auto* Beat = Mission ? Mission->FindBeat(HandoffBeat) : nullptr;
	if (!HasAuthority() || !IsValid(Controller) || !IsValid(Player) || Player->GetWorld() != GetWorld()
		|| !State || !State->IsStateValid() || State->IsMutationInProgress() || !Beat || !Destination
		|| Mission->MissionId != MissionId || AnchorId.IsNone() || Beat->RequiredHandoffAnchorId != AnchorId
		|| !Beat->HandoffToProtagonist.IsValid() || Beat->RequiredProtagonist != State->GetActiveProtagonist()
		|| State->IsBeatComplete(MissionId, HandoffBeat) || Player->GetProtagonistIdentityTag() != State->GetActiveProtagonist()
		|| !FMath::IsFinite(RequestRange) || RequestRange <= 0.f || !Player->IsAlive() || !Player->IsCharacterReady())
	{ return Fail(TEXT("The live mission, protagonist and native handoff anchor do not match.")); }
	for (TActorIterator<ASovCampaignHandoffAnchor> It(GetWorld()); It; ++It)
	{ if (*It != this && It->MissionId == MissionId && It->AnchorId == AnchorId) { return Fail(TEXT("Duplicate handoff anchor identity.")); } }
	for (FName Prior : Beat->PrerequisiteBeats)
	{ if (!State->IsBeatComplete(MissionId, Prior)) { return Fail(TEXT("Handoff prerequisite is incomplete.")); } }
	for (const auto& Required : Beat->RequiredState)
	{ if (State->GetStateValue(Required.Key) != Required.Value) { return Fail(TEXT("Handoff state requirement is unmet.")); } }
	if (!State->HasKnowledge(State->GetActiveProtagonist(), Beat->RequiredKnowledge)
		|| FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(RequestRange))
	{ return Fail(TEXT("Handoff is outside its valid range or knowledge context.")); }
	FHitResult Hit;
	const FCollisionQueryParams Query = Player->GetIgnoreCharacterParams();
	if (GetWorld()->LineTraceSingleByChannel(Hit, Player->GetPawnViewLocation(), GetActorLocation(), ECC_Visibility, Query)
		&& Hit.GetActor() != this) { return Fail(TEXT("Handoff anchor is occluded.")); }
	UClass* PawnClass = Mission->ResolvePawnClass(Beat->HandoffToProtagonist).LoadSynchronous();
	const ASovPlayerCharacterBase* Defaults = PawnClass ? Cast<ASovPlayerCharacterBase>(PawnClass->GetDefaultObject()) : nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Defaults || !Defaults->GetCapsuleComponent() || !Navigation) { return Fail(TEXT("Handoff requires a loaded destination kit and navigation.")); }
	OutDestination = Destination->GetComponentTransform();
	if (OutDestination.ContainsNaN()) { return Fail(TEXT("Invalid handoff destination transform.")); }
	const float HalfHeight = Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FNavLocation Projected;
	const FVector Feet = OutDestination.GetLocation() - FVector(0, 0, HalfHeight);
	if (!Navigation->ProjectPointToNavigation(Feet, Projected, FVector(60, 60, 120))
		|| FVector::DistSquared(Feet, Projected.Location) > FMath::Square(120.f))
	{ return Fail(TEXT("Handoff destination has no nearby safe navigation point.")); }
	OutDestination.SetLocation(Projected.Location + FVector(0, 0, HalfHeight));
	OutDestination.SetScale3D(FVector::OneVector);
	OutError.Reset(); return true;
}

bool ASovCampaignHandoffAnchor::RequestHandoff(ASovPlayerController* Controller, FString& OutError)
{
	return IsValid(Controller) && Controller->RequestAuthoredHandoff(this, OutError);
}
