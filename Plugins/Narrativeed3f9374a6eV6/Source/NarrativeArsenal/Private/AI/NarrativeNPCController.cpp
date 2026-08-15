// Copyright Narrative Tools 2024. 


#include "AI/NarrativeNPCController.h"
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

ANarrativeNPCController::ANarrativeNPCController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass("PathFollowingComponent", UNarrativePathFollowingComp::StaticClass()))
{
	NPCActivityComponent = CreateDefaultSubobject<UNPCActivityComponent>(TEXT("NPCActivityComponent"));
	InteractionComponent = CreateDefaultSubobject<UNPCInteractionComponent>(TEXT("InteractionComponent"));

}

void ANarrativeNPCController::BeginPlay()
{
	Super::BeginPlay();

	if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
	{
		PFC->OnRequestFinished.AddUObject(this, &ANarrativeNPCController::OnMoveComplete);
	}
}

void ANarrativeNPCController::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UPathFollowingComponent* PFC = GetPathFollowingComponent())
	{
		PFC->OnRequestFinished.RemoveAll(this);
	}
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
	Super::Destroyed();

	//If we're destroyed tell token granter to return token. 
	ReturnToken();
}

void ANarrativeNPCController::DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);

	FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
	DisplayDebugManager.SetDrawColor(FColor(255, 255, 0));

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
	Super::SetPawn(InPawn);

	if (ANarrativeNPCCharacter* NChar = Cast<ANarrativeNPCCharacter>(InPawn))
	{
		//cache this incase GetPawn changes 
		OwnedCharacter = NChar;

		if (UNarrativeAbilitySystemComponent* NASC = Cast<UNarrativeAbilitySystemComponent>(NChar->GetAbilitySystemComponent()))
		{
			NASC->OnDeathStateChanged.AddUniqueDynamic(this, &ThisClass::HandleDeath);
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

bool ANarrativeNPCController::RequestAttackToken(UNarrativeAbilitySystemComponent* TargetToAttack)
{
	if (TargetToAttack && GrantedToken != TargetToAttack)
	{
		if (TargetToAttack->TryClaimToken(this))
		{
			return true;
		}
	}

	return false; 
}

bool ANarrativeNPCController::ReturnToken()
{
	if (GrantedToken)
	{
		GrantedToken->ReturnToken(this);
	}

	return true; 
}

void ANarrativeNPCController::TokenStolen()
{

}

void ANarrativeNPCController::HandleDeath_Implementation(AActor* KilledActor, UNarrativeAbilitySystemComponent* KilledActorASC, const bool bIsDead)
{	
	//Mark the controller and pawn to be removed after 90s. TODO config option in settings 
	if (bIsDead)
	{
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