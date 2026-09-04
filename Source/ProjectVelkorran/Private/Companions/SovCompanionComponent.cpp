// Copyright Fallen Signal Studios. All Rights Reserved.
#include "Companions/SovCompanionComponent.h"
#include "Companions/SovCoActionAnchor.h"
#include "Companions/SovCoActionActivity.h"
#include "Campaign/SovEncounterDirector.h"
#include "Characters/SovPlayerCharacterBase.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "AI/NarrativeNPCController.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/NarrativeCharacterVisual.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "NarrativeGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "Sovereign/SovGameplayTags.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"

USovCompanionComponent::USovCompanionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USovCompanionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		BoundASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()));
		if (BoundASC) { BoundASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath); }
	}
}

void USovCompanionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelCoAction();
	if (IsValid(BoundASC)) { BoundASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath); }
	Super::EndPlay(EndPlayReason);
}

void USovCompanionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USovCompanionComponent, CommandState);
}

void USovCompanionComponent::OnRep_CommandState()
{
	OnCommandStateChanged.Broadcast(CommandState, FString());
}

bool USovCompanionComponent::ValidateRequest(ASovPlayerCharacterBase* Player, ASovCoActionAnchor* Anchor, bool bExisting, FString& Reason) const
{
	Reason.Reset();
	const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner());
	ANarrativeNPCController* Controller = NPC ? Cast<ANarrativeNPCController>(NPC->GetController()) : nullptr;
	UNarrativeAbilitySystemComponent* ASC = NPC ? Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<ANarrativeNPCCharacter*>(NPC))) : nullptr;
	if (!NPC || !NPC->HasAuthority() || !NPC->IsAlive() || !Controller || !ASC || ASC->GetAvatarActor() != NPC
		|| !IsValid(Anchor) || Anchor->GetWorld() != GetWorld() || CompanionId.IsNone()
		|| (!bExisting && CommandState == ESovCompanionCommandState::MovingToAnchor))
	{
		Reason = TEXT("Companion is unavailable for a new contextual action."); return false;
	}
	if (!Anchor->ValidatePermission(Player, CompanionId, Reason)) { return false; }
	if (FVector::DistSquared(Player->GetActorLocation(), Anchor->GetActorLocation()) > FMath::Square(Anchor->RequestRange))
	{
		Reason = TEXT("The player must remain near the co-action anchor."); return false;
	}
	const FNarrativeGameplayTags& Narrative = FNarrativeGameplayTags::Get();
	const FSovGameplayTags& Sov = FSovGameplayTags::Get();
	const FGameplayTag Blockers[] = { Narrative.State_IsDead, Narrative.State_SequencerControlled,
		Narrative.State_Movement_Ragdoll, Narrative.State_Movement_Lock, Narrative.State_Interacting, Sov.State_Fatal, Sov.State_Poise_Broken };
	for (const FGameplayTag Tag : Blockers)
	{
		if (ASC->HasMatchingGameplayTag(Tag)) { Reason = TEXT("Companion is interrupted or reserved by a scripted action."); return false; }
	}
	if ((!bExisting && ASC->HasMatchingGameplayTag(Narrative.State_Busy))
		|| (bExisting && ASC->GetTagCount(Narrative.State_Busy) > (bOwnsBusyTag ? 1 : 0)))
	{
		Reason = TEXT("Another action owns this companion."); return false;
	}
	for (TActorIterator<ANarrativeNPCCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == NPC) { continue; }
		if (const USovCompanionComponent* Other = It->FindComponentByClass<USovCompanionComponent>())
		{
			if (It->IsAlive() && Other->CompanionId == CompanionId) { Reason = TEXT("Companion IDs must identify one live actor."); return false; }
		}
	}
	UNPCActivityComponent* ActivityComp = Controller->GetActivityComponent();
	if (!ActivityComp || !ActivityComp->IsActive()) { Reason = TEXT("Narrative activities are unavailable."); return false; }
	if (!bExisting)
	{
		if (UNPCActivity* Current = ActivityComp->GetCurrentActivity())
		{
			if (!Current->IsInterruptable()) { Reason = TEXT("The current Narrative activity cannot be interrupted."); return false; }
		}
	}
	return true;
}

bool USovCompanionComponent::CanRequestCoAction(ASovPlayerCharacterBase* Player, ASovCoActionAnchor* Anchor, FString& Reason) const
{
	if (bMutation) { Reason = TEXT("Companion command is changing."); return false; }
	return ValidateRequest(Player, Anchor, false, Reason);
}

bool USovCompanionComponent::RequestCoAction(ASovPlayerCharacterBase* Player, ASovCoActionAnchor* Anchor, FString& Reason)
{
	if (!CanRequestCoAction(Player, Anchor, Reason)) { return false; }
	TGuardValue<bool> Mutation(bMutation, true);
	ANarrativeNPCCharacter* NPC = CastChecked<ANarrativeNPCCharacter>(GetOwner());
	ANarrativeNPCController* Controller = CastChecked<ANarrativeNPCController>(NPC->GetController());
	Activities = Controller->GetActivityComponent();
	Activity = Cast<USovCoActionActivity>(Activities->GetActivity(USovCoActionActivity::StaticClass()));
	if (!Activity) { Activity = Cast<USovCoActionActivity>(Activities->AddActivity(USovCoActionActivity::StaticClass(), false)); }
	if (!Activity) { Reason = TEXT("Native co-action activity could not be created."); return false; }
	ActiveAnchor = Anchor;
	RequestingPlayer = Player;
	RequestId = FGuid::NewGuid();
	RequestedAt = GetWorld()->GetTimeSeconds();
	ArrivedAt = -1.f;
	bUsedFallback = false;
	bPathFailed = false;
	bActivityInterrupted = false;
	CommandState = ESovCompanionCommandState::MovingToAnchor;
	ActiveGoal = NewObject<USovCoActionGoal>(Activities);
	ActiveGoal->Companion = this;
	ActiveGoal->Anchor = Anchor;
	ActiveGoal->RequestId = RequestId;
	ActiveGoal->GoalKey = Anchor;
	UNarrativeAbilitySystemComponent* NewASC = Cast<UNarrativeAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(NPC));
	if (IsValid(BoundASC) && BoundASC != NewASC) { BoundASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath); }
	BoundASC = NewASC;
	BoundASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
	BoundASC->CancelAllAbilities();
	if (!IsRequestCurrent(ActiveGoal) || !IsValid(BoundASC) || !NPC->IsAlive())
	{ FinishCommand(false, TEXT("Companion changed during ability cleanup.")); Reason = TEXT("Companion changed during ability cleanup."); return false; }
	BoundASC->AddLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
	bOwnsBusyTag = true;
	SetComponentTickEnabled(true);
	if (!Activities->AddGoal(ActiveGoal, true))
	{
		FinishCommand(false, TEXT("Narrative rejected the companion goal.")); Reason = TEXT("Narrative rejected the companion goal."); return false;
	}
	OnCommandStateChanged.Broadcast(CommandState, FString());
	NPC->ForceNetUpdate();
	return CommandState == ESovCompanionCommandState::MovingToAnchor;
}

bool USovCompanionComponent::IsRequestCurrent(const USovCoActionGoal* Goal) const
{
	return IsValid(Goal) && Goal == ActiveGoal && Goal->RequestId == RequestId && RequestId.IsValid()
		&& CommandState == ESovCompanionCommandState::MovingToAnchor && IsValid(ActiveAnchor)
		&& IsValid(RequestingPlayer) && IsValid(GetOwner());
}

void USovCompanionComponent::NotifyPathResult(USovCoActionGoal* Goal, bool bReached)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsRequestCurrent(Goal)) { return; }
	bPathFailed = !bReached;
	ArrivedAt = bReached ? GetWorld()->GetTimeSeconds() : -1.f;
}

void USovCompanionComponent::NotifyActivityInterrupted(USovCoActionGoal* Goal)
{
	// Narrative calls EndActivity while selecting its successor. Removing goals in
	// that call would recursively re-enter selection; resolve on our next tick.
	if (IsRequestCurrent(Goal)) { bActivityInterrupted = true; }
}

void USovCompanionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || bMutation || CommandState != ESovCompanionCommandState::MovingToAnchor) { return; }
	TGuardValue<bool> Mutation(bMutation, true);
	FString Reason;
	if (bActivityInterrupted) { FinishCommand(false, TEXT("Narrative activity was interrupted.")); return; }
	if (!ValidateRequest(RequestingPlayer, ActiveAnchor, true, Reason)) { FinishCommand(false, Reason); return; }
	if (!Activities || Activities->GetCurrentActivityGoal() != ActiveGoal)
	{
		FinishCommand(false, TEXT("Companion could not acquire the Narrative activity slot.")); return;
	}
	if (bPathFailed || GetWorld()->GetTimeSeconds() - RequestedAt >= ActiveAnchor->TimeoutSeconds)
	{
		if (!TryHiddenFallback()) { FinishCommand(false, TEXT("Companion cannot reach the permitted mark; retry the action.")); }
		return;
	}
	TryFinishArrival();
}

void USovCompanionComponent::TryFinishArrival()
{
	if (ArrivedAt < 0.f) { return; }
	if (!ActiveAnchor->IsCompanionAtMark(GetOwner()))
	{
		ArrivedAt = -1.f;
		bPathFailed = true;
		return;
	}
	if (GetWorld()->GetTimeSeconds() - ArrivedAt < ActiveAnchor->HoldAtMarkSeconds) { return; }
	const bool bCommitted = ActiveAnchor->CommitArrival(this, RequestingPlayer, RequestId);
	FinishCommand(bCommitted, bCommitted ? FString() : TEXT("Mission state rejected the co-action receipt."));
}

bool USovCompanionComponent::TryHiddenFallback()
{
	if (bUsedFallback || !IsValid(ActiveAnchor) || !IsValid(ActiveAnchor->HiddenFallbackAnchor)
		|| ActiveAnchor->HiddenFallbackAnchor->GetWorld() != GetWorld() || !Activity) { return false; }
	bUsedFallback = true;
	ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner());
	UCapsuleComponent* Capsule = NPC ? NPC->GetCapsuleComponent() : nullptr;
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Capsule || !Navigation) { return false; }
	FNavLocation Projected;
	const FVector Desired = ActiveAnchor->HiddenFallbackAnchor->GetActorLocation();
	if (!Navigation->ProjectPointToNavigation(Desired, Projected, FVector(75.f, 75.f, 150.f))) { return false; }
	const FVector Destination = Projected.Location + FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 2.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCompanionFallback), false, NPC);
	TArray<AActor*> Attachments; NPC->GetAttachedActors(Attachments, true, true); Query.AddIgnoredActors(Attachments);
	if (AActor* Visual = NPC->GetCharacterVisual())
	{
		Query.AddIgnoredActor(Visual); Attachments.Reset();
		Visual->GetAttachedActors(Attachments, true, true); Query.AddIgnoredActors(Attachments);
	}
	if (GetWorld()->OverlapBlockingTestByChannel(Destination, NPC->GetActorQuat(), Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query)) { return false; }
	if (!IsFallbackHiddenFromAllPlayers(Destination)) { return false; }
	NPC->GetCharacterMovement()->StopMovementImmediately();
	if (!NPC->SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics)) { return false; }
	RequestedAt = GetWorld()->GetTimeSeconds();
	ArrivedAt = -1.f;
	bPathFailed = false;
	return Activity->RestartOwnedMove();
}

bool USovCompanionComponent::IsFallbackHiddenFromAllPlayers(const FVector& Destination) const
{
	const ANarrativeNPCCharacter* NPC = Cast<ANarrativeNPCCharacter>(GetOwner());
	const UCapsuleComponent* Capsule = NPC ? NPC->GetCapsuleComponent() : nullptr;
	if (!Capsule || Destination.ContainsNaN()) { return false; }
	FCollisionQueryParams Query(SCENE_QUERY_STAT(SovCompanionFallbackVisibility), false, NPC);
	TArray<AActor*> Attachments; NPC->GetAttachedActors(Attachments, true, true); Query.AddIgnoredActors(Attachments);
	if (AActor* Visual = NPC->GetCharacterVisual())
	{
		Query.AddIgnoredActor(Visual); Attachments.Reset();
		Visual->GetAttachedActors(Attachments, true, true); Query.AddIgnoredActors(Attachments);
	}
	bool bHasViewer = false;
	TArray<FVector> Samples;
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	for (const FVector Center : { NPC->GetActorLocation(), Destination })
	{
		Samples.Append({ Center, Center + FVector(0.f, 0.f, HalfHeight), Center - FVector(0.f, 0.f, HalfHeight),
			Center + FVector(Radius, 0.f, 0.f), Center - FVector(Radius, 0.f, 0.f),
			Center + FVector(0.f, Radius, 0.f), Center - FVector(0.f, Radius, 0.f) });
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC || !PC->PlayerCameraManager) { return false; }
		bHasViewer = true;
		FVector View; FRotator Rotation; PC->GetPlayerViewPoint(View, Rotation);
		FCollisionQueryParams ViewQuery = Query;
		ViewQuery.AddIgnoredActor(PC->GetPawn());
		if (APawn* ViewingPawn = PC->GetPawn())
		{ Attachments.Reset(); ViewingPawn->GetAttachedActors(Attachments, true, true); ViewQuery.AddIgnoredActors(Attachments); }
		for (const FVector& Sample : Samples)
		{
			if (!GetWorld()->LineTraceTestByChannel(View, Sample, ECC_Visibility, ViewQuery)) { return false; }
		}
	}
	if (!bHasViewer) { return false; }
	return true;
}

void USovCompanionComponent::FinishCommand(bool bSucceeded, const FString& Reason)
{
	TGuardValue<bool> Mutation(bMutation, true);
	// Clear request identity before removing our goal; Narrative goal removal
	// can synchronously end the activity and call us again.
	USovCoActionGoal* PreviousGoal = ActiveGoal;
	ActiveGoal = nullptr;
	RequestId.Invalidate();
	ActiveAnchor = nullptr;
	RequestingPlayer = nullptr;
	SetComponentTickEnabled(false);
	CommandState = bSucceeded ? ESovCompanionCommandState::Succeeded : ESovCompanionCommandState::Failed;
	if (bOwnsBusyTag && IsValid(BoundASC))
	{
		BoundASC->RemoveLooseGameplayTag(FNarrativeGameplayTags::Get().State_Busy, 1, EGameplayTagReplicationState::TagAndCountToAll);
	}
	bOwnsBusyTag = false;
	if (IsValid(Activities) && IsValid(PreviousGoal)) { Activities->RemoveGoal(PreviousGoal); }
	if (GetOwner()) { GetOwner()->ForceNetUpdate(); }
	OnCommandStateChanged.Broadcast(CommandState, Reason);
}

void USovCompanionComponent::CancelCoAction()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || CommandState != ESovCompanionCommandState::MovingToAnchor) { return; }
	FinishCommand(false, TEXT("Companion action canceled."));
}

void USovCompanionComponent::HandleDeath(AActor* Actor, UNarrativeAbilitySystemComponent* ASC, bool bIsDead)
{
	if (!bIsDead || Actor != GetOwner() || ASC != BoundASC || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
	CancelCoAction();
	if (IsValid(RequiredEncounter)) { RequiredEncounter->FailEncounter(); }
}

void USovCompanionComponent::Load_Implementation()
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
	CancelCoAction();
	CommandState = ESovCompanionCommandState::Idle;
	bActivityInterrupted = false;
	bPathFailed = false;
	bUsedFallback = false;
	SetComponentTickEnabled(false);
	GetOwner()->ForceNetUpdate();
	OnRep_CommandState();
}
