// Copyright Narrative Tools 2024. 


#include "AI/NarrativeNPCController.h"
#include "AI/NarrativeAIStartupDiagnostics.h"
#include "AI/NPCDefinition.h"
#include "AI/Activities/NPCActivityComponent.h"
#include "GAS/NarrativeAbilitySystemComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "UnrealFramework/NarrativeCharacter.h"
#include "UnrealFramework/NarrativeNPCCharacter.h"
#include "Interaction/NPCInteractionComponent.h"
#include <Engine/Canvas.h>
#include <DisplayDebugHelpers.h>
#include "Kismet/KismetMathLibrary.h"
#include "AI/NarrativePathFollowingComp.h"
#include "Perception/AIPerceptionComponent.h"
#include "Engine/World.h"

ANarrativeNPCController::ANarrativeNPCController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass("PathFollowingComponent", UNarrativePathFollowingComp::StaticClass()))
{
	NPCActivityComponent = CreateDefaultSubobject<UNPCActivityComponent>(TEXT("NPCActivityComponent"));
	InteractionComponent = CreateDefaultSubobject<UNPCInteractionComponent>(TEXT("InteractionComponent"));

}

void ANarrativeNPCController::BeginPlay()
{
	FNarrativeAIStartupDiagnostics::Record(this, TEXT("controller_begin_play_enter"));
	FNarrativeAIStartupDiagnostics::Snapshot(this, true);
	Super::BeginPlay();
	RefreshThreatMemory();

	if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
	{
		PFC->OnRequestFinished.AddUObject(this, &ANarrativeNPCController::OnMoveComplete);
	}
}

void ANarrativeNPCController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	FNarrativeAIStartupDiagnostics::Record(this, TEXT("controller_end_play"));
	ClearThreatMemory();
	if (ThreatPerception.IsValid())
	{
		FNarrativeAIStartupDiagnostics::Record(this, TEXT("perception_detached"), ThreatPerception->GetPathName());
		ThreatPerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &ThisClass::HandleThreatPerception);
		ThreatPerception->OnComponentActivated.RemoveDynamic(this, &ThisClass::HandleThreatPerceptionActivated);
		ThreatPerception->OnComponentDeactivated.RemoveDynamic(this, &ThisClass::HandleThreatPerceptionDeactivated);
	}
	ThreatPerception.Reset();
	if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
	{
		PFC->OnRequestFinished.RemoveAll(this);
	}

	// Streaming removal and teardown do not always pass through Destroyed().
	// Release the finite attacker slot while the target ASC is still available.
	ForceReleaseAttackToken();
	Super::EndPlay(EndPlayReason);
}

class UAbilitySystemComponent* ANarrativeNPCController::GetAbilitySystemComponent() const
{
	if (OwnedCharacter)
	{
		return OwnedCharacter->GetAbilitySystemComponent();
	}

	return nullptr;
}

FGameplayTagContainer ANarrativeNPCController::GetFactions() const
{
	if (OwnedCharacter)
	{
		return OwnedCharacter->GetFactions();
	}

	return FGameplayTagContainer::EmptyContainer;
}

void ANarrativeNPCController::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if(OwnedCharacter)
	{
		OwnedCharacter->GetOwnedGameplayTags(TagContainer);
	}
}

bool ANarrativeNPCController::HasMatchingGameplayTag(FGameplayTag TagToCheck) const
{
	if(OwnedCharacter)
	{
		return OwnedCharacter->HasMatchingGameplayTag(TagToCheck);
	}
	return true;
}

bool ANarrativeNPCController::HasAllMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if(OwnedCharacter)
	{
		return OwnedCharacter->HasAllMatchingGameplayTags(TagContainer);
	}
	return true;
}

bool ANarrativeNPCController::HasAnyMatchingGameplayTags(const FGameplayTagContainer& TagContainer) const
{
	if(OwnedCharacter)
	{
		return OwnedCharacter->HasAnyMatchingGameplayTags(TagContainer);
	}
	return true; 
}

void ANarrativeNPCController::Destroyed()
{
	// Explicit destruction may precede EndPlay. This is intentionally idempotent.
	ForceReleaseAttackToken();
	Super::Destroyed();
}

void ANarrativeNPCController::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor(255, 255, 0));
	if (DebugDisplay.IsDisplayOn("Threat"))
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		for (const FNarrativeThreatMemory& Memory : GetThreatDebugSnapshot())
		{
			DisplayDebugManager.DrawString(FString::Printf(TEXT("Threat %s source=%s strength=%.2f confidence=%.2f direct=%d position=%s sharedBy=%s factions=%s expiry=%.2fs"),
				*GetNameSafe(Memory.Target.Get()), *UEnum::GetValueAsString(Memory.Source), Memory.Strength, Memory.Confidence,
				Memory.bDirectObservation, *Memory.LastKnownPosition.ToCompactString(), *GetNameSafe(Memory.SharedBy.Get()),
				*Memory.SharedFactions.ToString(), Memory.ExpiresAt - Now));
		}
	}

	if (UNPCActivityComponent* ActivityComp = NPCActivityComponent)
	{
		DisplayDebugManager.DrawString("- GOALS -");

		for (auto& GoalKVP : ActivityComp->Goals)
		{
			for (auto& Goal : GoalKVP.Value.Goals)
			{
				if (Goal)
				{
					FString GoalString = Goal->GetDebugString();

					DisplayDebugManager.DrawString(*GoalString);
				}
			}
		}

		if (ActivityComp->CurrentActivity)
		{
			DisplayDebugManager.DrawString("CURRENT ACTIVITY: " + ActivityComp->CurrentActivity->GetActivityName().ToString());
		}
		else
		{
			DisplayDebugManager.DrawString("CURRENT ACTIVITY: NONE");
		}
	}

	if (DebugDisplay.IsDisplayOn("Factions"))
	{
		FGameplayTagContainer Factions = GetFactions();

		if (Factions.IsValid())
		{
			DisplayDebugManager.DrawString(FString::Printf(TEXT("Factions: %s"), *Factions.ToString()));
		}
	}
	
}

void ANarrativeNPCController::SetPawn(APawn* InPawn)
{
	const uint64 Assignment = ++PawnAssignmentGeneration;
	const bool bChangingPawn = GetPawn() != InPawn;
	const TWeakObjectPtr<ANarrativeNPCController> Self = this;
	const TWeakObjectPtr<APawn> RequestedPawn = InPawn;
	// Vehicle possession preserves Narrative's separate NPC identity. Unpossessing
	// must clear it, and possessing a different NPC installs that NPC instead.
	ANarrativeNPCCharacter* IncomingCharacter = Cast<ANarrativeNPCCharacter>(InPawn);
	if (!IncomingCharacter && InPawn) { IncomingCharacter = OwnedCharacter; }
	const TWeakObjectPtr<ANarrativeNPCCharacter> RequestedCharacter = IncomingCharacter;
	if (bChangingPawn)
	{
		if (IsValid(OwnedCharacter))
		{
			if (auto* OldASC = Cast<UNarrativeAbilitySystemComponent>(OwnedCharacter->GetAbilitySystemComponent()))
			{ OldASC->OnDeathStateChanged.RemoveDynamic(this, &ThisClass::HandleDeath); }
		}
		// Native resets cannot invoke Blackboard or Perception callbacks. Do not emit
		// cleanup while the outgoing Pawn and incoming OwnedCharacter disagree.
		OwnedCharacter = nullptr;
		++ThreatMemoryGeneration;
		ThreatMemory.Reset();
		ThreatSuspensionOwners.Reset();
		ThreatUpdateAccumulator = 0.f;
		bPawnThreatCleanupPending = true;
	}
	Super::SetPawn(InPawn);
	const auto StillOwnsAssignment = [Self, RequestedPawn, Assignment]()
	{
		return Self.IsValid() && !Self->IsActorBeingDestroyed() && !RequestedPawn.IsStale()
			&& Self->PawnAssignmentGeneration == Assignment && Self->GetPawn() == RequestedPawn.Get();
	};
	// Super can synchronously broadcast pawn changes. A nested assignment is newer
	// and owns all subsequent state; the older call must never install its NPC.
	if (!StillOwnsAssignment()) { return; }
	OwnedCharacter = RequestedCharacter.Get();
	if (IsValid(OwnedCharacter))
	{
		if (auto* NASC = Cast<UNarrativeAbilitySystemComponent>(OwnedCharacter->GetAbilitySystemComponent()))
		{
			NASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
		}
	}
	if (bPawnThreatCleanupPending)
	{
		bPawnThreatCleanupPending = false;
		// Memory callbacks see consistent Pawn/OwnedCharacter and occur after the
		// final Super call. Reentry can replace us without an older Super overwriting it.
		ClearThreatMemory();
		if (!StillOwnsAssignment()) { return; }
		if (!InPawn)
		{
			ClearFocus(EAIFocusPriority::Gameplay);
			if (!StillOwnsAssignment()) { return; }
			StopMovement();
		}
	}
}

bool ANarrativeNPCController::ShouldPostponePathUpdates() const
{
	if (OwnedCharacter && OwnedCharacter->IsPlayingAttachWarpMontage)
	{
		return true; 
	}

	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		if (ASC->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_PostponePathUpdates))
		{
			return true; 
		}
	}

	return Super::ShouldPostponePathUpdates();
}

ANarrativeCharacter* ANarrativeNPCController::GetNarrativeCharacter() const
{
	return GetOwnedNPC();
}

#if ENABLE_VISUAL_LOG

void ANarrativeNPCController::GrabDebugSnapshot(FVisualLogEntry* Snapshot) const
{
	Super::GrabDebugSnapshot(Snapshot);

	if (UNPCActivityComponent* NPCA = NPCActivityComponent)
	{
		NPCA->DescribeSelfToVisLog(Snapshot);
	}
}

#endif 

class UBehaviorTree* ANarrativeNPCController::GetCurrentTree()
{
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent))
	{
		return BTComp->GetCurrentTree();
	}

	return nullptr; 
}

void ANarrativeNPCController::StopBehaviorTree()
{
	CleanupBrainComponent();
}

class UNPCDefinition* ANarrativeNPCController::GetNPCData() const
{
	if (ANarrativeNPCCharacter* NPCChar = Cast<ANarrativeNPCCharacter>(GetPawn()))
	{
		return NPCChar->GetNPCDefinition();
	}

	return nullptr;
}

FText ANarrativeNPCController::GetNPCName() const
{
	if (UNPCDefinition* NPCD = GetNPCData())
	{
		return NPCD->NPCName;
	}

	return FText::GetEmpty();
}

bool ANarrativeNPCController::IsAlive() const
{
	if (ANarrativeNPCCharacter* NPCChar = Cast<ANarrativeNPCCharacter>(GetPawn()))
	{
		return NPCChar->IsAlive();
	}

	return false; 
}

ANarrativeNPCCharacter* ANarrativeNPCController::GetControlledNPC() const
{
	return Cast<ANarrativeNPCCharacter>(GetPawn());
}

ANarrativeNPCCharacter* ANarrativeNPCController::GetOwnedNPC() const
{
	return OwnedCharacter;
}

bool ANarrativeNPCController::CanAcquireAttackTokenFor(
	const UNarrativeAbilitySystemComponent* TargetToAttack) const
{
	if (!HasAuthority() || !IsValid(TargetToAttack))
	{
		return false;
	}

	if (ReservedAttackTokenLeaseSerial != 0
		&& IsAttackTokenLeaseCurrent(
			ReservedAttackTokenLeaseSerial,
			GrantedToken.Get()))
	{
		return false;
	}

	const UNarrativeAbilitySystemComponent* ExistingTarget = GrantedToken.Get();
	const bool bHasReciprocalToken = IsValid(ExistingTarget)
		&& ExistingTarget->HasAttackTokenFor(this);
	if (ExistingTarget == TargetToAttack && bHasReciprocalToken)
	{
		return true;
	}

	// A controller may own only one target's token. Do not silently abandon a
	// token held by another behavior just because a direct ability was issued.
	// The target ASC remains the final authority: its claim may consume a free
	// slot or apply Narrative's existing token-steal policy.
	return !bHasReciprocalToken;
}

bool ANarrativeNPCController::TryAcquireAttackTokenFor(
	UNarrativeAbilitySystemComponent* TargetToAttack,
	uint64& OutLeaseSerial,
	bool& bOutNewlyAcquired)
{
	OutLeaseSerial = 0;
	bOutNewlyAcquired = false;
	UNarrativeAbilitySystemComponent* ExistingTarget = GrantedToken.Get();
	if (ExistingTarget != nullptr
		&& (!IsValid(ExistingTarget)
			|| !ExistingTarget->HasAttackTokenFor(this)))
	{
		// Normalize local state before preflight. Otherwise a still-valid ASC
		// pointer with no reciprocal token entry can block every future target.
		SetGrantedAttackToken(nullptr);
	}
	if (ReservedAttackTokenLeaseSerial != 0)
	{
		if (IsAttackTokenLeaseCurrent(
				ReservedAttackTokenLeaseSerial,
				GrantedToken.Get()))
		{
			return false;
		}
		ReservedAttackTokenLeaseSerial = 0;
		bReturnAttackTokenWhenReservationEnds = false;
	}
	if (!CanAcquireAttackTokenFor(TargetToAttack))
	{
		return false;
	}

	if (GrantedToken == TargetToAttack)
	{
		// Every direct reservation receives a fresh epoch, even when borrowing a
		// token the Behavior Tree already owns. A delayed/double release from an
		// earlier ability can therefore never match and release this reservation.
		AdvanceAttackTokenLeaseSerial();
		OutLeaseSerial = AttackTokenLeaseSerial;
		ReservedAttackTokenLeaseSerial = AttackTokenLeaseSerial;
		bReturnAttackTokenWhenReservationEnds = false;
		return true;
	}
	if (!RequestAttackToken(TargetToAttack)
		|| GrantedToken != TargetToAttack
		|| !TargetToAttack->HasAttackTokenFor(this)
		|| AttackTokenLeaseSerial == 0)
	{
		return false;
	}

	OutLeaseSerial = AttackTokenLeaseSerial;
	bOutNewlyAcquired = true;
	ReservedAttackTokenLeaseSerial = AttackTokenLeaseSerial;
	bReturnAttackTokenWhenReservationEnds = false;
	return true;
}

bool ANarrativeNPCController::IsAttackTokenLeaseCurrent(
	const uint64 LeaseSerial,
	const UNarrativeAbilitySystemComponent* ExpectedTarget) const
{
	return LeaseSerial != 0
		&& LeaseSerial == AttackTokenLeaseSerial
		&& LeaseSerial == ReservedAttackTokenLeaseSerial
		&& IsValid(ExpectedTarget)
		&& GrantedToken == ExpectedTarget
		&& ExpectedTarget->HasAttackTokenFor(this);
}

bool ANarrativeNPCController::IsAttackTokenReservedFor(
	const UNarrativeAbilitySystemComponent* ExpectedTarget) const
{
	return IsAlive()
		&& IsValid(GetPawn())
		&& IsAttackTokenLeaseCurrent(
			ReservedAttackTokenLeaseSerial,
			ExpectedTarget);
}

bool ANarrativeNPCController::ReleaseAttackTokenLease(
	const uint64 LeaseSerial,
	const bool bReturnTokenAfterRelease)
{
	UNarrativeAbilitySystemComponent* ReservedTarget = GrantedToken.Get();
	if (!HasAuthority()
		|| !IsAttackTokenLeaseCurrent(LeaseSerial, ReservedTarget))
	{
		return false;
	}

	const bool bShouldReturnToken = bReturnTokenAfterRelease
		|| bReturnAttackTokenWhenReservationEnds;
	ReservedAttackTokenLeaseSerial = 0;
	bReturnAttackTokenWhenReservationEnds = false;
	return !bShouldReturnToken || ReturnToken();
}

void ANarrativeNPCController::SetGrantedAttackToken(
	UNarrativeAbilitySystemComponent* NewGrantedToken)
{
	if (GrantedToken == NewGrantedToken)
	{
		return;
	}

	ReservedAttackTokenLeaseSerial = 0;
	bReturnAttackTokenWhenReservationEnds = false;
	GrantedToken = NewGrantedToken;
	AdvanceAttackTokenLeaseSerial();
}

void ANarrativeNPCController::AdvanceAttackTokenLeaseSerial()
{
	++AttackTokenLeaseSerial;
	// Zero is reserved for "no captured lease" even after uint64 wraparound.
	if (AttackTokenLeaseSerial == 0)
	{
		++AttackTokenLeaseSerial;
	}
}

void ANarrativeNPCController::ForceReleaseAttackToken()
{
	ReservedAttackTokenLeaseSerial = 0;
	bReturnAttackTokenWhenReservationEnds = false;
	ReturnToken();
}

bool ANarrativeNPCController::RequestAttackToken(UNarrativeAbilitySystemComponent* TargetToAttack)
{
	if (TargetToAttack && GrantedToken != TargetToAttack)
	{
		// A controller owns at most one target token. The public Blueprint path
		// predates native lease tracking, so make its documented retarget cleanup
		// explicit as well: never overwrite GrantedToken and strand the old
		// target's FAttackToken entry.
		if (GrantedToken.Get() != nullptr)
		{
			ReturnToken();
		}

		if (GrantedToken == nullptr
			&& TargetToAttack->TryClaimToken(this))
		{
			return true;
		}
	}

	return false; 
}

bool ANarrativeNPCController::ReturnToken()
{
	if (ReservedAttackTokenLeaseSerial != 0
		&& IsAttackTokenLeaseCurrent(
			ReservedAttackTokenLeaseSerial,
			GrantedToken.Get()))
	{
		// A Behavior Tree may finish the action that originally owned this token
		// while a direct ability still reserves it. Defer physical return until
		// that reservation ends so the attack cannot outlive its attacker slot.
		bReturnAttackTokenWhenReservationEnds = true;
		return true;
	}

	UNarrativeAbilitySystemComponent* TokenToReturn = GrantedToken.Get();
	const uint64 LeaseSerialToReturn = AttackTokenLeaseSerial;
	if (IsValid(TokenToReturn))
	{
		TokenToReturn->ReturnToken(this);
	}

	// ReturnToken normally clears this through the target ASC. Also clear a
	// stale local pointer if that target was torn down or its token array was
	// already repaired. Do not clear a newer lease installed by a callback.
	if (GrantedToken == TokenToReturn
		&& AttackTokenLeaseSerial == LeaseSerialToReturn)
	{
		SetGrantedAttackToken(nullptr);
	}

	return true; 
}

void ANarrativeNPCController::TokenStolen()
{

}

void ANarrativeNPCController::HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{
	if (KilledActor != OwnedCharacter) { return; }
	//Mark the controller and pawn to be removed after 90s. TODO config option in settings 
	if (bIsDead)
	{
		ClearThreatMemory();
		FString RoleStr = HasAuthority() ? "Server" : "Client";

		UE_LOG(LogTemp, Warning, TEXT("%s HANDLE DEATH, ASKING FOR CLEANUP 90s"), *RoleStr);
		
		CleanUp(90.f);

		if (InteractionComponent)
		{
			InteractionComponent->StopInteractBehavior(false);
		}
	}
}

void ANarrativeNPCController::OnMoveComplete(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (UNarrativePathFollowingComp* PFC = Cast<UNarrativePathFollowingComp>(GetPathFollowingComponent()))
	{
		if (Result.IsSuccess())
		{
			//FString Reached = PFC->DidMoveReachGoal() ? "Yes" : "No";
			//UE_LOG(LogTemp, Display, TEXT("Move successfully! %s"), *PFC->CachedLastDestination.ToString());

			//If we succeeded, but didn't reach our goal, ie we had a partial path, try traverse 
			if (!PFC->DidMoveReachGoal() && !PFC->CachedLastDestination.IsNearlyZero())
			{
				if (OwnedCharacter)
				{
					const FVector Offset = ((PFC->CachedLastDestination) - OwnedCharacter->GetActorLocation());

					//TODO need to override acceleration in trytraversal to point it at the target, or figure out how to set acceleration 
					if (!OwnedCharacter->TryAttachWarp(true, FVector2D(0.f, 1.f), -1))
					{
						FRotator CharRot = OwnedCharacter->GetActorRotation();
						
						//Only jump down, up should be mantle.
						if (PFC->CachedLastDestination.Z < OwnedCharacter->GetActorLocation().Z)
						{
							// when jumping off, abort any current move, the move should be re evaluated
							PFC->AbortMove(*this, FPathFollowingResultFlags::UserAbort);
							
							const FVector LaunchVect = Offset.GetSafeNormal2D() * 300.f + FVector(0.f, 0.f, 250.f);

							// set the focus to be the direction of the launch
							SetFocalPoint(OwnedCharacter->GetActorLocation() + LaunchVect, EAIFocusPriority::Move);

							// only launch when the rotation of the character and the launch vector is within 25 degrees of each other 
							const float Dot = FVector::DotProduct(CharRot.Vector().GetSafeNormal(), LaunchVect.GetSafeNormal());
							if (Dot < FMath::Cos(FMath::DegreesToRadians(25.0)))
							{
								//If traversal failed, we could also try a jump down, because you cannot traverse downwards. 
								OwnedCharacter->LaunchCharacter(LaunchVect, true, true);

								// clear focus after
								ClearFocus(EAIFocusPriority::Move);
							}
						}
						else
						{
							// mantle was intended but did not happen, set the rotation to face where we need to go to try let the mantle pass
							CharRot.Yaw = Offset.Rotation().Yaw;
							OwnedCharacter->SetActorRotation(CharRot);
						}
					}
				}
			}

		}

	}
}


void ANarrativeNPCController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (ANarrativeNPCCharacter* MyPawn = GetControlledNPC())
	{

		FRotator NewControlRotation = GetControlRotation();

		// Look toward focus - but not if we're postponing pathing or being controlled by sequencer - that would usually cause fighting between the focus system and sequencer and look jank. 
		const FVector FocalPoint = GetFocalPoint();
		const bool bSkipFocusRotation = MyPawn->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_SequencerControlled);

		if (!bSkipFocusRotation && FAISystem::IsValidLocation(FocalPoint))
		{
			NewControlRotation = (FocalPoint - MyPawn->GetPawnViewLocation()).Rotation();
		}
		else if (bSetControlRotationFromPawnOrientation)
		{
			NewControlRotation = MyPawn->GetActorRotation();
		}

		//	//We dont want this in Narrative Pro, because we want to support focal points with a Z Component. 
		// Don't pitch view unless looking at another pawn
		//if (NewControlRotation.Pitch != 0 && Cast<APawn>(GetFocusActor()) == nullptr)
		//{

		//	//NewControlRotation.Pitch = 0.f;
		//}

		SetControlRotation(NewControlRotation);

		//Smooth and change the pawn rotation - TODO rewrite to be a little more bespoke 
		if (bUpdatePawn)
		{
			//Get Pawn current rotation
			const FRotator CurrentPawnRotation = MyPawn->GetActorRotation();

			//Calculate smoothed rotation
			SmoothTargetRotation = FMath::RInterpConstantTo(MyPawn->GetActorRotation(), ControlRotation, DeltaTime, SmoothFocusInterpSpeed);
			//Check if we need to change
			if (CurrentPawnRotation.Equals(SmoothTargetRotation, 1e-3f) == false)
			{
				//Change rotation using the Smooth Target Rotation
				MyPawn->FaceRotation(SmoothTargetRotation, DeltaTime);
			}
		}
	}
}
