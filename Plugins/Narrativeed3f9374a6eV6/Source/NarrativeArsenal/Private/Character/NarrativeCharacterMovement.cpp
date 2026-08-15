// Copyright Narrative Tools 2024. 


#include "Character/NarrativeCharacterMovement.h"
#include <UnrealFramework/NarrativeCharacter.h>
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include <GameFramework/Character.h>
#include "NarrativeGameplayTags.h"
#include <DrawDebugHelpers.h>
#include <Engine/Engine.h>
#include <Engine/World.h>
#include "MotionWarpingComponent.h"
#include "AbilitySystemComponent.h"
#include "ArsenalStatics.h"
#include "AI/Navigation/NarrativeNavigationSystem.h"
#include "AI/Navigation/NarrativeRecastNavMesh.h"
#include "GameFramework/PhysicsVolume.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "UnrealFramework/NarrativeAnimInstance.h"
#include "UnrealFramework/NarrativePlayerCharacter.h"
#include "Engine/SkinnedAsset.h"
#include "AI/Cover/CoverTypes.h"
#include "ChooserFunctionLibrary.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "KismetTraceUtils.h"
#include "Camera/CameraComponent.h"

static const float CHAR_MESH_OFFSET = 2.f;
static const FName NAME_PelvisBone("pelvis");

#define FLAG_SPRINT FSavedMove_Character::FLAG_Custom_0
#define FLAG_SLOWWALK FSavedMove_Character::FLAG_Custom_1

static TAutoConsoleVariable<bool> CVarClimbDebug(
	TEXT("n.cmc.Climb.Debug"),
	false,
	TEXT("Debug Actions 0=Off 1=On"),
	ECVF_Default);

static TAutoConsoleVariable<bool> CVarCoverDebug(
	TEXT("n.cmc.Cover.Debug"),
	false,
	TEXT("Debug Cover 0=Off 1=On"),
	ECVF_Default);

template<class T>
static FString EnumToString(T& Value)
{
	static_assert(TIsEnum<T>::Value, "Should only call this with enum types");
	FString StringValue = UEnum::GetValueAsString(Value);
	int CutoffIndex;
	StringValue.FindLastChar(':', CutoffIndex);
	return StringValue.RightChop(CutoffIndex + 1);
}

FTransform UNarrativeCharacterMovement::InvalidCoverTransform = FTransform(FRotator::ZeroRotator, FVector(TNumericLimits<double>::Max()), FVector::OneVector); 

UNarrativeCharacterMovement::UNarrativeCharacterMovement()
{
	SlowWalkSpeed = 250.f;
	SprintSpeed = 900.f;
	OrientToMovementInterpSpeed = 360.f; 

	CoverState.CoverTransform = UNarrativeCharacterMovement::InvalidCoverTransform;
	FindCoverForwardSearchDist = 500.f;
	PlayerOffsetFromCover = 60.0f;
	LeanFromCoverDist = 80.0f;
	NextCoverTraceSpacing = 60.0f;
	NextCoverTraceDepth = 100.0f;
	CoverInterpSpeed = 3.0f;
	bCanWalkAroundCornersInCover = true; 
	bOrientCrouchToCoverHeight = true; 

	bWantsSprint = false;
	bIsWarping = false;

	NarrativeCharacterOwner = nullptr;

	//Traversal
	bDisableClimbAndMantles = false; 
	StartFallingLedgeCheckCooldown = 0.5f;
	bCheckLedgeWhilstFalling = true;
	TraversalTraceForwardDistance = 150.f;
	TraversalTraceForwardDistanceFalling = 50.f;
	TraversalTraceHeightMax = 225.f;
	TraversalTraceHeightMin = 45.f;
	TraversalTraceCapsuleHalfHeightScale = 1.5f;
	TraversalTraceCapsuleHalfHeightScaleFalling = 1.f;

	EnterRagdollFallZThreshold = 1000.f;
	EnterRagdollFallZImpactSlopeThreshold = 200.f;
	EnterRagdollFallZImpactGroundThreshold = -1200.f;
}

void UNarrativeCharacterMovement::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	bWantsSlowWalk = NarrativeCharacterOwner ? NarrativeCharacterOwner->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Movement_SlowWalking) : false;
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UNarrativeCharacterMovement::InitializeComponent()
{
	Super::InitializeComponent();
}

void UNarrativeCharacterMovement::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (MovementMode == MOVE_Swimming)
	{
		OnEnterSwimming();
	}
	else if (PreviousMovementMode == MOVE_Swimming)
	{
		OnExitSwimming();
	}

	if (MovementMode == MOVE_Walking)
	{
		if (GetNarrativeCharacterOwner()->AttachWarpProps.ActionType == ETraversalActionType::ExitClimb)
		{
			GetNarrativeCharacterOwner()->AttachWarpProps.ActionType = ETraversalActionType::None;
		}
	}

	//If we leave walking cover is no longer allowed. 
	if (!IsMovingOnGround() && HasCover())
	{
		InvalidateCover();
	}

	if (MovementMode == MOVE_Falling)
	{
		BeginFallingTime = GetWorld()->GetTimeSeconds();

		//Fix some rare issues where climbing motion warps could have you fall through map. 
		GetNarrativeCharacterOwner()->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::Type::QueryAndPhysics);
	}

	if (IsClimbing())
	{
		OnEnterClimbing();
	}
	else if (PreviousCustomMode == CMOVE_Climb)
	{
		OnExitClimbing();
	}

	if (IsRagdoll())
	{
		OnEnterRagdoll();
	}
	else if (PreviousCustomMode == CMOVE_Ragdoll)
	{
		OnExitRagdoll();
	}
}

FRotator UNarrativeCharacterMovement::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	if (HasCover())
	{
		if (!IsAiming())
		{	
			return FMath::RInterpConstantTo(CurrentRotation, CoverState.CoverPlayerRotation, DeltaTime, OrientToMovementInterpSpeed);
			//return CoverState.CoverPlayerRotation;
		}

	}

	if (!Velocity.IsNearlyZero() && Acceleration.Length() > 5.f)
	{
		// Rotate toward direction of velocity, not acceleration which is default CMC logic.
		return FMath::RInterpConstantTo(CurrentRotation, Velocity.GetSafeNormal().Rotation(), DeltaTime, OrientToMovementInterpSpeed);
	}
	else
	{
		return CurrentRotation;
	}

	//Gracefully interp rotation towards velocity rather than snapping 


}

void UNarrativeCharacterMovement::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	switch (CustomMovementMode)
	{
	case CMOVE_Climb:
		PhysClimb(deltaTime, Iterations);
		break;
	case CMOVE_Ragdoll:
		PhysRagdoll(deltaTime, Iterations);
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("Invalid Movement Mode"))
	}
}

float UNarrativeCharacterMovement::VisualizeMovement() const
{

		float HeightOffset = 0.f;
	const float OffsetPerElement = 10.0f;
	if (CharacterOwner == nullptr)
	{
		return HeightOffset;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	const FVector TopOfCapsule = GetActorLocation() + CharacterOwner->GetSimpleCollisionHalfHeight() * -GetGravityDirection();
	

	//Forward direction 
	{
		const FColor DebugColor = FColor::Red;
		HeightOffset += 50.f;

		const FVector FwdVect = GetForwardVector();

		FVector DebugLocation = GetActorLocation();//TopOfCapsule + HeightOffset * -GetGravityDirection();

		DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + FwdVect * 100.f, 100.f, DebugColor, false, -1.f, (uint8)'\000', 10.f);

		if (CharacterOwner)
		{
			if (AController* Controller = CharacterOwner->GetController())
			{
				const FVector CtrlFwd = Controller->GetControlRotation().Vector();

				DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + CtrlFwd * 100.f, 100.f, FColor::Purple, false, -1.f, (uint8)'\000', 10.f);

			}
		}

		DrawDebugCapsule(GetWorld(), DebugLocation, CapHH(), CapR(), FwdVect.ToOrientationQuat(), FColor::Red);

		//Nudge local text forward so you can see it in first person still 
		if (ANarrativePlayerCharacter* PChar = Cast<ANarrativePlayerCharacter>(CharacterOwner))
		{
			if (PChar->IsLocallyControlled() && PChar->IsCameraInsideHead())
			{
				const FVector CtrlFwd = PChar->GetControlRotation().Vector();

				DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + CtrlFwd * 100.f, 100.f, FColor::Blue, false, -1.f, (uint8)'\000', 10.f);

				DebugLocation += FwdVect * 100.f;
			}
		}

		FString OrientToMovement = bOrientRotationToMovement ? "Yes" : "No";
		FString UseControllerRotYaw = NarrativeCharacterOwner->bUseControllerRotationYaw ? "Yes - Snapped" : bUseControllerDesiredRotation ? "Yes - Smoothed" : "No";
		FString AttachParent = *GetNameSafe(UpdatedComponent->GetAttachParentActor());
		FString DebugText = FString::Printf(TEXT("Forward Vector: %s\n Orient To Movement: %s \n Use Controller Yaw: %s \n Attach: %s"), *FwdVect.ToCompactString(), *OrientToMovement, *UseControllerRotYaw, *AttachParent);
		DrawDebugString(GetWorld(), DebugLocation, DebugText, nullptr, DebugColor, 0.f, true);

		

		//FString UseControllerRotYaw = NarrativeCharacterOwner->bUseControllerRotationYaw ? "Yes" : "No";
		//DebugText = FString::Printf(TEXT("Use Controller Rot Yaw: %s"), *UseControllerRotYaw);
		//DrawDebugString(GetWorld(), DebugLocation - FVector(0.f, 0.f, 10.f), DebugText, nullptr, DebugColor, 0.f, true);

		//FString OrientToMovement = bOrientRotationToMovement ? "Yes" : "No";
		//DebugText = FString::Printf(TEXT("Orient to Movement: %s"), *OrientToMovement);
		//DrawDebugString(GetWorld(), DebugLocation - FVector(0.f, 0.f, 20.f), DebugText, nullptr, DebugColor, 0.f, true);

	}

	#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	return Super::VisualizeMovement();
}

bool UNarrativeCharacterMovement::CanAttemptJump() const
{
	if (CharacterOwner && CharacterOwner->IsMoveInputIgnored())
	{
		return false; 
	}

	return Super::CanAttemptJump();
}

void UNarrativeCharacterMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	NarrativeCharacterOwner = Cast<ANarrativeCharacter>(CharacterOwner);
}

void UNarrativeCharacterMovement::RequestPathMove(const FVector& MoveInput)
{
	FVector AdjustedMoveInput(MoveInput);

	// preserve magnitude when moving on ground/falling and requested input has Z component
	// see ConstrainInputAcceleration for details
	if (MoveInput.Z != 0.f && (IsMovingOnGround() || IsFalling()))
	{
		const float Mag = MoveInput.Size();
		AdjustedMoveInput = MoveInput.GetSafeNormal2D() * Mag;
	}

	/**NOTE: the only reason we've overriden this function is that UEs default version doesn't pass bForce as true. 
	This is a problem because we lock input whilst trying to Move players around, but because UE doesnt force, this stops
	player moving. So we override, and use bForce. */
	if (PawnOwner)
	{
		PawnOwner->Internal_AddMovementInput(MoveInput, true);
	}
}

FString UNarrativeCharacterMovement::GetMovementName() const
{

	if (IsClimbing())
	{
		return FString("Climbing");
	}

	if (IsRagdoll())
	{
		return FString("Ragdoll");
	}

	return Super::GetMovementName();

}

bool UNarrativeCharacterMovement::CanWalkOffLedges() const
{
	//If we're in cover, peek will move us out, potentially pushing us off an edge, which is frustrating, so prevent that. 
	if (HasCover())
	{
		return false;
	}

	return Super::CanWalkOffLedges();
}

void UNarrativeCharacterMovement::StartNewPhysics(float deltaTime, int32 Iterations)
{
	// No override for ground friction so we place it here
	GroundFriction = FMath::GetMappedRangeValueClamped(FVector2d(0, 500), FVector2d(5, 3), Velocity.Size2D());
		
	Super::StartNewPhysics(deltaTime, Iterations);
}

float UNarrativeCharacterMovement::GetMaxAcceleration() const
{
	float AdjustedAcceleration = Super::GetMaxAcceleration();
	if (bWantsSprint)
	{
		AdjustedAcceleration = FMath::GetMappedRangeValueClamped(FVector2d(300, 700), FVector2d(800, 300), Velocity.Size2D());
	}
	
	return AdjustedAcceleration;
}

float UNarrativeCharacterMovement::GetMaxBrakingDeceleration() const
{
	float BrakingDeceleration = Super::GetMaxBrakingDeceleration();
	return Acceleration.Length() > 0.f ? 500.f : BrakingDeceleration;
}

void UNarrativeCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	//Update wants sprint from the compressed flag 
	bWantsSprint = (Flags & FLAG_SPRINT) != 0;
	bWantsSlowWalk = (Flags & FLAG_SLOWWALK) != 0;
}

void UNarrativeCharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (HasCover())
	{
		if (ShouldInvalidateCover())
		{
			InvalidateCover();
		}

		//Perform top-left-right sweeps. Do them from where the player offsets from cover, not from the players position, because otherwise leaning would move these traces which we dont want. 
		//Do use players Z though, as cover Z changes depending on where capsule hit, which we dont really want - we want the ground instead. 

		//Top sweep. 
		FCollisionQueryParams QueryParams = GetIgnoreCharacterParams();

		FVector PlayerCov;
		FRotator PlayerCovRot;
		CoverToPlayer(PlayerCov, PlayerCovRot, false);

		FVector StartLoc = PlayerCov;
		StartLoc.Z = CapB(CapHH() + 90.f).Z;
		FVector EndLoc = (-CoverState.CoverTransform.GetRotation().Vector() * NextCoverTraceDepth) + StartLoc;

		FHitResult ShortHit;
		CoverState.bIsShortCover = !GetWorld()->SweepSingleByChannel(
			ShortHit,
			StartLoc,
			EndLoc,
			FQuat::Identity,
			TraceChannel_NarrativeCover,
			FCollisionShape::MakeSphere(CapR()),
			QueryParams
		);

#if ENABLE_DRAW_DEBUG
		if (CVarCoverDebug.GetValueOnAnyThread())
		{
			FString RoleStr = IsServer() ? "Server" : "Client";
			FString CoverStr = CoverState.bIsShortCover ? "Short" : "Tall";
			GEngine->AddOnScreenDebugMessage(-1, 0.001f, FColor::Red, FString::Printf(TEXT("%s: we hit %s"), *RoleStr, *CoverStr));

			DrawDebugSphereTraceSingle(GetWorld(), StartLoc, EndLoc, CapR(), EDrawDebugTrace::Type::ForOneFrame, CoverState.bIsShortCover, ShortHit, FLinearColor::Green, FLinearColor::Red, 5.f);

		}
		#endif 

		//TODO instead of capsule - 40.f, we need to find ground Z and come up 40 from that to better account for slopes
		// 
		//Left check
		StartLoc = (CoverRight() * 100.f) + PlayerCov;
		StartLoc.Z = CapB(CapHH() + 40.f).Z;
		EndLoc = (CoverFwd() * -NextCoverTraceDepth) + StartLoc;

		FHitResult LeftHit;
		CoverState.bIsLeftOpen = !GetWorld()->SweepSingleByChannel(
			ShortHit,
			StartLoc,
			EndLoc,
			FQuat::Identity,
			TraceChannel_NarrativeCover,
			FCollisionShape::MakeCapsule(CapR(), CapHH() * 0.75f),
			QueryParams
		);

#if ENABLE_DRAW_DEBUG
		if (CVarCoverDebug.GetValueOnAnyThread())
		{
			FString RoleStr = IsServer() ? "Server" : "Client";
			FString CoverStr = CoverState.bIsLeftOpen ? "free" : "blocked";
			GEngine->AddOnScreenDebugMessage(-1, 0.001f, FColor::Red, FString::Printf(TEXT("%s: left side is %s"), *RoleStr, *CoverStr));

			DrawDebugCapsuleTraceSingle(GetWorld(), StartLoc, EndLoc, CapR(), CapHH() * 0.75f, EDrawDebugTrace::Type::ForOneFrame, CoverState.bIsLeftOpen, LeftHit, FLinearColor::Green, FLinearColor::Red, 5.f);
		}
#endif

		//Right check
		//StartLoc = (-CoverState.CoverTransform.GetRotation().GetRightVector() * 100.f) + UpdatedComponent->GetComponentLocation();
		StartLoc = (CoverRight() * -100.f) + PlayerCov;
		StartLoc.Z = CapB(CapHH() + 40.f).Z;
		EndLoc = (CoverFwd() * -NextCoverTraceDepth) + StartLoc;

		FHitResult RightHit;
		CoverState.bIsRightOpen = !GetWorld()->SweepSingleByChannel(
			ShortHit,
			StartLoc,
			EndLoc,
			FQuat::Identity,
			TraceChannel_NarrativeCover,
			FCollisionShape::MakeCapsule(CapR(), CapHH() * 0.75f),
			QueryParams
		);

#if ENABLE_DRAW_DEBUG
		if (CVarCoverDebug.GetValueOnAnyThread())
		{
			FString RoleStr = IsServer() ? "Server" : "Client";
			FString CoverStr = CoverState.bIsRightOpen ? "free" : "blocked";
			GEngine->AddOnScreenDebugMessage(-1, 0.001f, FColor::Red, FString::Printf(TEXT("%s: Right side is %s"), *RoleStr, *CoverStr));
			DrawDebugCapsuleTraceSingle(GetWorld(), StartLoc, EndLoc, CapR(), CapHH() * 0.75f, EDrawDebugTrace::Type::ForOneFrame, CoverState.bIsRightOpen, RightHit, FLinearColor::Green, FLinearColor::Red, 5.f);

		}
#endif 

		if (IsAiming())
		{
			bWantsToCrouch = false;
		}
		else if(bOrientCrouchToCoverHeight)
		{
			//If we're already crouched, dont stand us up, but do crouch if we enter short cover. 
			if (!bWantsToCrouch)
			{
				bWantsToCrouch = CoverState.bIsShortCover;
			}

		}
	}
	else if(ShouldCheckForLedgeWhilstFalling() && MovementMode == MOVE_Falling)
	{
		GetNarrativeCharacterOwner()->TryAttachWarp(false, GetLocalInputVector(), -1.f);			
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

class FNetworkPredictionData_Client* UNarrativeCharacterMovement::GetPredictionData_Client() const
{
	//Boilerplate prediction data allocation 
	if (!ClientPredictionData)
	{
		UNarrativeCharacterMovement* MutableThis = const_cast<UNarrativeCharacterMovement*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_NarrativeCharacter(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}

	return ClientPredictionData;
}

void UNarrativeCharacterMovement::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	
	if (NewMovementMode == MovementMode && (MovementMode != MOVE_Custom || CustomMovementMode == NewCustomMode)) return; // already in the proposed movement mode
		
	if (ANarrativeCharacter* NChar = GetNarrativeCharacterOwner())
	{
		//Dont let move mode change if we're in ragdoll, except to enter ragdoll in first place. 
		if (NChar->IsRagdoll() && IsCustomMovementMode(CMOVE_Ragdoll))
		{
			//Swimming is an exception - ragdolling into water should exit ragdoll, however we need do this through our owner so it replicates properly 
			if (NewMovementMode == MOVE_Swimming)
			{
				NChar->SetRagdoll(false);	
			}

			return;
		}

		Super::SetMovementMode(NewMovementMode, NewCustomMode);
	}
}

float UNarrativeCharacterMovement::GetMaxSpeed() const
{
	if (IsValid(NarrativeCharacterOwner))
	{		
		if (IsSlowWalking())
		{
			return SlowWalkSpeed;
		}
	}

	if (bWantsSprint && !IsCrouching())
	{
		return SprintSpeed;
	}

	return Super::GetMaxSpeed();
}

bool UNarrativeCharacterMovement::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && !IsClimbing();
}

bool UNarrativeCharacterMovement::DoJump(bool bReplayingMoves, float DeltaTime)
{
	const FVector2D LocalVec = GetLocalInputVector();

	if (!bDisableClimbAndMantles)
	{
		//Drop from climbing if jump and correct input/bool are active
		if (IsClimbing() && GetLocalInputVector().Y <= -1.f && !GetNarrativeCharacterOwner()->IsPlayingAttachWarpMontage && !TryFindClimbTransform(GetNarrativeCharacterOwner()->AttachWarpProps, true, GetLocalInputVector(), -1.f))
		{
			if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
			{
				if (AnimInstance->GetOverrideLayerTag() == GetClimbOverrideLayerTag())
				{
					AnimInstance->RemoveOverrideLayer(0.3f);
				}
			}
			SetMovementMode(MOVE_Falling);
			return true;
		}

		if (GetNarrativeCharacterOwner()->TryAttachWarp(true, LocalVec, -1.f))
		{
			return true;
		}
	}

	if(MovementMode == MOVE_Walking && ShouldEnterDive())
	{
		//Todo liam add  FNarrativeGameplayTags::Get().Camera_FirstPerson_Follow3PHeadLocation to character, make OnDiveMontageEnded and remove it in there 
		GetNarrativeCharacterOwner()->IsPlayingAttachWarpMontage = true; 
		GetNarrativeCharacterOwner()->GetCharacterAnimInstance()->Montage_Play(EnterDive);
		FOnMontageEnded BlendOutDelegate;
		BlendOutDelegate.BindUObject(this, &UNarrativeCharacterMovement::OnMontageEnded);
		GetNarrativeCharacterOwner()->GetCharacterAnimInstance()->Montage_SetEndDelegate(BlendOutDelegate, EnterDive);
	}
	
	return Super::DoJump(bReplayingMoves, DeltaTime);
}

void UNarrativeCharacterMovement::HandleImpact(const FHitResult& Hit, float TimeSlice /*= 0.f*/, const FVector& MoveDelta /*= FVector::ZeroVector*/)
{
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	// Never ragdoll from landing bashing against vertical surfaces.
	const FVector::FReal ImpactNormalZ = GetGravitySpaceZ(Hit.ImpactNormal);
	if (ImpactNormalZ < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	//falling on something steep should ragdoll 
	if (GetNetMode() == NM_Standalone && Velocity.Z < -EnterRagdollFallZImpactSlopeThreshold && !IsWalkable(Hit))
	{
		GetNarrativeCharacterOwner()->RagdollForDuration(3.f);
	}
}

void UNarrativeCharacterMovement::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}
		
	// Move along the current floor
	FVector Delta = ProjectToGravityFloor(InVelocity) * DeltaSeconds;
	FHitResult Hit(1.f);

	if (HasCover())
	{

		//const FVector ControlNormal = GetPawnOwner()->GetControlRotation().Vector();
		const FVector LocalAcceleration = GetPawnOwner()->GetControlRotation().UnrotateVector(Acceleration);
		const FVector2D LocalInput(LocalAcceleration);
		const FVector2D NormalizedInput = LocalInput.GetSafeNormal();

		FString RoleStr = IsServer() ? "Server" : "Client";
		UE_LOG(LogTemp, Warning, TEXT("%s: input vector is %s"), *RoleStr, *LocalInput.ToString());
			
		//Player is pulling away from the wall - they probably want to leave cover. 
		if (LocalInput.X < -500.f)
		{
			InvalidateCover();
		} // update only when attempting to move either side - can reintroduce aiming soon but for now block that whilst in cover
		else if (FMath::Abs(LocalInput.Y) > 5.f) 
		{
			const bool bMoved = CoverMove(LocalInput);

			//Need to set acceleration manually to make GASP motion matched anims display correctly. 
			if (!bMoved)
			{
				Acceleration = FVector::ZeroVector;
			}
			else
			{
				Acceleration = Velocity.GetSafeNormal() * 800.f;

				//Make player face velocity when we move along it, but not at any other time we move player. 
				CoverState.CoverPlayerRotation = Velocity.GetSafeNormal().Rotation();
			}
		}


		//TODO interp speed should use MaxWalkSpeed to figure out the most we can interp per frame. 
		FVector LocTarget;
		FRotator RotTarget;
		CoverToPlayer(LocTarget, RotTarget);
		const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();

		//TODO need to figure out why GetMaxSpeed() is not being respected by the interp function, needs multiplied up to get closer. 
		Delta = FMath::VInterpConstantTo(CurrentLocation, LocTarget, DeltaSeconds, GetMaxSpeed() * CoverInterpSpeed) - CurrentLocation;

	}
		
	FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);
	float LastMoveTimeSlice = DeltaSeconds;
	
	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (GetGravitySpaceZ(Hit.Normal) > UE_KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), true, Hit);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit) || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
				if (!StepUp(GetGravityDirection(), Delta * (1.f - PercentTimeApplied), Hit, OutStepDownResult))
				{
					// TODO: logging is important
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
				}
				else
				{
					// TODO: logging is important
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					if (!bMaintainHorizontalGroundVelocity)
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && StepUpTimeSlice >= UE_KINDA_SMALL_NUMBER)
						{
							Velocity = (UpdatedComponent->GetComponentLocation() - PreStepUpLocation) / StepUpTimeSlice;
							Velocity = ProjectToGravityFloor(Velocity);
						}
					}
				}
			}
			else if ( Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner) )
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}
		}
	}
}

bool UNarrativeCharacterMovement::GetUseAccelerationForPaths() const
{
	return NavMovementProperties.bUseAccelerationForPaths;
}

void UNarrativeCharacterMovement::SetUseAccelerationForPaths(const bool bNewAccelerationForPaths)
{
	//Need to stop any pathing requests - a new one needs to be made for property to work. 
	//StopActiveMovement();
	NavMovementProperties.bUseAccelerationForPaths = bNewAccelerationForPaths;
}

void UNarrativeCharacterMovement::StartSprinting()
{
	if(bOrientRotationToMovement)
	{
		bWantsSprint = true;
	}
	else if(UpdatedComponent->GetForwardVector().Dot(Acceleration.GetSafeNormal()) > 0.2f)
	{
		bWantsSprint = true;
	}
}

void UNarrativeCharacterMovement::StopSprinting()
{
	bWantsSprint = false;
}

bool UNarrativeCharacterMovement::IsMovingForward(const float ForwardAngleTolerance) const
{
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		FVector Forward = OwnerChar->GetActorForwardVector();
		FVector MoveDirection = Velocity.GetSafeNormal();

		//Ignore vertical movement
		Forward.Z = 0.0f;
		MoveDirection.Z = 0.0f;

		//Ensure velocity is near enough to forward direction 
		return FMath::Abs(FMath::Acos(FMath::RadiansToDegrees(FVector::DotProduct(Forward, MoveDirection)))) < ForwardAngleTolerance;
	}
	return false; 
}

bool UNarrativeCharacterMovement::IsSprinting() const
{
	return bWantsSprint && !IsCrouching() && IsWalking() && !Velocity.IsNearlyZero();
}

bool UNarrativeCharacterMovement::IsSlowWalking() const
{
	if (MirrorMovement)
	{
		return MirrorMovement->IsSlowWalking();
	}

	return NarrativeCharacterOwner && bWantsSlowWalk;
}

void UNarrativeCharacterMovement::SetMirrorComponent(class UNarrativeCharacterMovement* MirrorCMC)
{
	MirrorMovement = MirrorCMC;
}

bool UNarrativeCharacterMovement::ShouldCheckForLedgeWhilstFalling() const
{
	if (bCheckLedgeWhilstFalling)
	{
		if (IsFalling())
		{
			if (GetWorld()->TimeSince(BeginFallingTime) < StartFallingLedgeCheckCooldown)
			{
				return false; 
			}
		}

		return GetNarrativeCharacterOwner()->AttachWarpProps.ActionType != ETraversalActionType::ExitClimb;
	}
	return bCheckLedgeWhilstFalling;
}

//Swiming
void UNarrativeCharacterMovement::PhysSwimming(float deltaTime, int32 Iterations)
 {
 	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	
	RestorePreAdditiveRootMotionVelocity();
	float Depth = ImmersionDepth();
	
	AController* OwnerController = CharacterOwner->GetController();
	float UnWoundControllerPitch = 0.f;
	if (OwnerController)
	{
		if(OwnerController->GetControlRotation().Pitch > 180)
		{
			UnWoundControllerPitch = OwnerController->GetControlRotation().Pitch - 360; 
		}
		else
		{
			UnWoundControllerPitch = OwnerController->GetControlRotation().Pitch;
		}
	}
	
	//Project our acceleration to water surface when we are at the surface to stop us swimming out of the water volume
	if(UnWoundControllerPitch < 0.f && Depth < 1.f && Acceleration.Z > 0)
	{
		Acceleration = FVector::VectorPlaneProject(Acceleration, GetGravityDirection());
	}
	if(UnWoundControllerPitch > 0.f && Depth < 1.f && Acceleration.Z > 0)
	{
		Acceleration = FVector::VectorPlaneProject(Acceleration, GetGravityDirection());
	}
	

	float NetFluidFriction  = 0.f;
	float NetBuoyancy = Buoyancy * Depth;
	float OriginalAccelZ = GetGravitySpaceZ(Acceleration);
	bool bLimitedUpAccel = false;

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (GetGravitySpaceZ(Velocity) > 0.33f * MaxSwimSpeed) && (NetBuoyancy != 0.f))
	{
		//damp positive Z out of water
		SetGravitySpaceZ(Velocity, FMath::Max<FVector::FReal>(0.33f * MaxSwimSpeed, GetGravitySpaceZ(Velocity) * Depth*Depth));
	}
	else if (Depth < 0.65f)
	{
		bLimitedUpAccel = (OriginalAccelZ > 0.f);
		SetGravitySpaceZ(Acceleration, FMath::Min<FVector::FReal>(0.1f, OriginalAccelZ));
	}

	Iterations++;
	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	bJustTeleported = false;
	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction * Depth;
		CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
		Velocity += (GetGravityZ() * deltaTime * (1.f - NetBuoyancy)) * -GetGravityDirection();
	}

	ApplyRootMotionToVelocity(deltaTime);

	FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	const float remainingTime = deltaTime * Swim(Adjusted, Hit);

	//may have left water - if so, script might have set new physics mode
	if ( !IsSwimming() )
	{
		StartNewPhysics(remainingTime, Iterations);
		return;
	}

	if ( Hit.Time < 1.f && CharacterOwner)
	{
		HandleSwimmingWallHit(Hit, deltaTime);
		if (bLimitedUpAccel && (GetGravitySpaceZ(Velocity) >= 0.f))
		{
			// allow upward velocity at surface if against obstacle
			Velocity += OriginalAccelZ * deltaTime * -GetGravityDirection();
			Adjusted = Velocity * (1.f - Hit.Time)*deltaTime;
			Swim(Adjusted, Hit);
			if (!IsSwimming())
			{
				StartNewPhysics(remainingTime, Iterations);
				return;
			}
		}

		const FVector VelDir = Velocity.GetSafeNormal();
		const float UpDown = VelDir | GetGravityDirection();

		bool bSteppedUp = false;
		if( (FMath::Abs(GetGravitySpaceZ(Hit.ImpactNormal)) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
		{
			const float StepZ = GetGravitySpaceZ(UpdatedComponent->GetComponentLocation());
			const FVector RealVelocity = Velocity;
			SetGravitySpaceZ(Velocity, 1.f);	// HACK: since will be moving up, in case pawn leaves the water
			bSteppedUp = StepUp(GetGravityDirection(), Adjusted * (1.f - Hit.Time), Hit);
			if (bSteppedUp)
			{
				//may have left water - if so, script might have set new physics mode
				if (!IsSwimming())
				{
					StartNewPhysics(remainingTime, Iterations);
					return;
				}
				SetGravitySpaceZ(OldLocation, GetGravitySpaceZ(UpdatedComponent->GetComponentLocation()) + (GetGravitySpaceZ(OldLocation) - StepZ));
			}
			Velocity = RealVelocity;
		}

		if (!bSteppedUp)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}
	}

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && !bJustTeleported && ((deltaTime - remainingTime) > UE_KINDA_SMALL_NUMBER) && CharacterOwner )
	{
		const bool bWaterJump = !GetPhysicsVolume()->bWaterVolume;
		const FVector::FReal VelZ = GetGravitySpaceZ(Velocity);
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / (deltaTime - remainingTime);
		if (bWaterJump)
		{
			SetGravitySpaceZ(Velocity, VelZ);
		}
	}

	if ( !GetPhysicsVolume()->bWaterVolume && IsSwimming() )
	{
		SetMovementMode(MOVE_Falling); //in case script didn't change it (w/ zone change)
	}

	//may have left water - if so, script might have set new physics mode
	if ( !IsSwimming() )
	{
		StartNewPhysics(remainingTime, Iterations);
	}
 }

void UNarrativeCharacterMovement::PhysFalling(float deltaTime, int32 Iterations)
 {
	 Super::PhysFalling(deltaTime, Iterations);

	 //If we start falling quickly, start ragdolling
	 if (Velocity.Z < -EnterRagdollFallZThreshold)
	 {
		 GetNarrativeCharacterOwner()->SetRagdoll(true);
	 }
 }

 void UNarrativeCharacterMovement::PhysWalking(float deltaTime, int32 Iterations)
 {

	if (HasCover())
	{
		//Sometimes, we need to interp our player to the cover/peek spot, but the player is not pressing WASD, and so won't have acceleration. 
		//To get around this, we pipe a small acceleration in if we have none so MoveAlongGround then gets called, handling interp. 
		if (UpdatedComponent && Acceleration.IsNearlyZero())
		{

			FVector LocTarget;
			FRotator RotTarget;
			CoverToPlayer(LocTarget, RotTarget);

			FVector Offset = UpdatedComponent->GetComponentLocation() - LocTarget;

			//Basically if we're not at the cover loc, pipe some acceleration in to force WalkAlongGround to be called even if we're not pressing WASD. 
			//Issue is, this causes CoverMove() to be called, which we actually dont want. So we use a small value of 2.f so CoverMove can ignore it. 
			Acceleration  = UpdatedComponent->GetForwardVector() * 2.f;
		}
	}

	Super::PhysWalking(deltaTime, Iterations);
 }

static void DrawDebugShape(UWorld* World, FTransform Transform, FCollisionShape Shape, FColor Color, float Duration)
{
	if (Shape.IsBox())
	{
		DrawDebugBox(World, Transform.GetLocation(), Shape.GetExtent(), Transform.GetRotation(), Color, false, Duration);
	}
	else if (Shape.IsCapsule())
	{
		DrawDebugCapsule(World, Transform.GetLocation(), Shape.GetCapsuleHalfHeight(), Shape.GetCapsuleRadius(), Transform.GetRotation(), Color, false, Duration);
	}
}

void UNarrativeCharacterMovement::PhysClimb(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		Velocity = FVector::ZeroVector;
	}

	ApplyRootMotionToVelocity(deltaTime);

	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		const FVector VelDir = Velocity.GetSafeNormal();
		const float UpDown = VelDir | GetGravityDirection();

		bool bSteppedUp = false;
		if ((FMath::Abs(GetGravitySpaceZ(Hit.ImpactNormal)) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && CanStepUp(Hit))
		{
			const FVector::FReal StepZ = GetGravitySpaceZ(UpdatedComponent->GetComponentLocation());
			bSteppedUp = StepUp(GetGravityDirection(), Adjusted * (1.f - Hit.Time), Hit);
			if (bSteppedUp)
			{
				const FVector::FReal LocationZ = GetGravitySpaceZ(UpdatedComponent->GetComponentLocation()) + (GetGravitySpaceZ(OldLocation) - StepZ);
				SetGravitySpaceZ(OldLocation, LocationZ);
			}
		}

		if (!bSteppedUp)
		{
			//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
		}
	}

	if( !bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

}


void UNarrativeCharacterMovement::PhysRagdoll(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	//In here, we basically just want the capsule to be updated such that it follows the pelvis bone of the character
	if (ANarrativeCharacter* CharOwner = GetNarrativeCharacterOwner())
	{
		if (USkeletalMeshComponent* CharMesh = CharOwner->GetMesh())
		{
			if (const FBodyInstance* PelvisBody = CharMesh->GetBodyInstance(NAME_PelvisBone))
			{
				FVector PelvisLocation;
				FVector DesiredVelocity;

				FPhysicsCommand::ExecuteRead(PelvisBody->ActorHandle, [this, &PelvisLocation, &DesiredVelocity](const FPhysicsActorHandle& ActorHandle)
					{
						PelvisLocation = FPhysicsInterface::GetTransform_AssumesLocked(ActorHandle, true).GetLocation();
						DesiredVelocity = FPhysicsInterface::GetLinearVelocity_AssumesLocked(ActorHandle);
					});


				Velocity = DesiredVelocity;

				//UE_LOG(LogTemp, Warning, TEXT("we're at %s, pelvis is at  %s"), *UpdatedComponent->GetComponentLocation().ToString(), *PelvisLocation.ToString());

				//Move me once without a sweep - this gets my XY correct, but Z will likely penetrate the ground
				FHitResult Hit(1.f);
				FVector XYDelta = PelvisLocation - UpdatedComponent->GetComponentLocation();
				XYDelta.Z = 0.f;
				SafeMoveUpdatedComponent(XYDelta, UpdatedComponent->GetComponentQuat(), false, Hit);

				//Move again with a sweep, this causes the Z to be resolved. 
				FHitResult ZHit(1.f);
				const FVector ZDelta = PelvisLocation - UpdatedComponent->GetComponentLocation();
				SafeMoveUpdatedComponent(ZDelta, UpdatedComponent->GetComponentQuat(), true, ZHit);

				//Falling 
				if (ZHit.bBlockingHit)
				{
					// todo override CharacterOwner->ShouldNotifyLanded(ZHit)) to allow return true for ragdoll case, it will fail as checks falling only 
					if (CharacterOwner)
					{
						CharacterOwner->Landed(ZHit);
					}
				}
			}
		}
	}
}

void UNarrativeCharacterMovement::OnEnterSwimming()
{
	Velocity = FVector::ZeroVector;

	//TODO liam left these as 0.2f for blend in time, you need to pass through values you want 
	if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
	{
		//Not character agnostic - biped will work, others will fail todo liam 
		
		// only allow dive animation on standalone
		if (GetNetMode() == NM_Standalone)
		{
			AnimInstance->Montage_Play(EnterWater);
		}
		
		AnimInstance->ApplyOverrideLayer(FNarrativeGameplayTags::Get().Narrative_Anim_OverrideLayer_Swimming, 0.2f);
	}
}

void UNarrativeCharacterMovement::OnExitSwimming()
{
	//TODO liam left these as 0.2f for blend in time, you need to pass through values you want 
	if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
	{
		if (AnimInstance->GetOverrideLayerTag() == FNarrativeGameplayTags::Get().Narrative_Anim_OverrideLayer_Swimming)
		{
 			AnimInstance->RemoveOverrideLayer(0.2f);
		}
	}
}

void UNarrativeCharacterMovement::OnEnterClimbing()
{
	if (GetNarrativeCharacterOwner()->AttachWarpProps.ActionType == ETraversalActionType::Climb)
	{
		if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
		{
			AnimInstance->ApplyOverrideLayer(GetClimbOverrideLayerTag(), GetNarrativeCharacterOwner()->AttachWarpProps.OverrideLayerBlendIn);
		}
	}
	if (GetNarrativeCharacterOwner()->AttachWarpProps.ActionType == ETraversalActionType::ExitClimb)
	{
		if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
		{
			AnimInstance->RemoveOverrideLayer(0.3f);
		}
	}
}

void UNarrativeCharacterMovement::OnExitClimbing()
{

}

void UNarrativeCharacterMovement::OnEnterRagdoll()
{
	if (ANarrativeCharacter* CharOwner = GetNarrativeCharacterOwner())
	{
		if (USkeletalMeshComponent* CharMesh = CharOwner->GetMesh())
		{
			CharOwner->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

			CharMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			CharMesh->ResetAllBodiesSimulatePhysics();
			CharMesh->SetSimulatePhysics(true);
			CharMesh->SetCollisionObjectType(ECC_PhysicsBody);
			CharMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			//Can enable this when we want physical anims 
			CharMesh->bUpdateJointsFromAnimation = true;

			//Stops us falling through objects at high velocity 
			CharMesh->SetAllUseCCD(true);

			if (UNarrativeAnimInstance* AnimInst = GetCharacterAnimInstance())
			{
				AnimInst->ApplyOverrideLayer(FNarrativeGameplayTags::Get().Narrative_Anim_OverrideLayer_Ragdoll, 0.2f);
			}

			NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
			bIgnoreClientMovementErrorChecksAndCorrection = true;
		}
	}
}

void UNarrativeCharacterMovement::OnExitRagdoll()
{
	if (ANarrativeCharacter* CharOwner = GetNarrativeCharacterOwner())
	{
		if (USkeletalMeshComponent* CharMesh = CharOwner->GetMesh())
		{
			CharOwner->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

			CharMesh->SetSimulatePhysics(false);
			// Uncomment to get snapshot pelvis bone reset 
			//CharMesh->SetAllBodiesBelowSimulatePhysics(NAME_PelvisBone, false, true);
			CharMesh->SetCollisionObjectType(ECC_Pawn);
			CharMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			CharMesh->SetAllUseCCD(false);
			CharMesh->SetPhysicsBlendWeight(0.f);

			const FTransform PelvisTransform = CharMesh->GetSocketTransform(NAME_PelvisBone);

			const bool bRagdollFacingUpward = FMath::UnwindDegrees(PelvisTransform.Rotator().Roll) <= 0.0f;

			//We need to point the capsule in the direction pelvis is facing our biped turn in place plays nicely 
			FRotator NewActorRotation = CharOwner->GetActorRotation();
			NewActorRotation.Yaw = bRagdollFacingUpward ? PelvisTransform.Rotator().Yaw - 180.0f : PelvisTransform.Rotator().Yaw;
			CharOwner->SetActorRotation(NewActorRotation);
			
			// Cache the current camera location/rotation so we can reapply it after reattaching
			FVector CachedCameraLocation = FVector::ZeroVector;
			FRotator CachedCameraRotation = FRotator::ZeroRotator;
			USceneComponent* CameraComp = nullptr;
			
			TArray<USceneComponent*> Children;
			CharMesh->GetChildrenComponents(true, Children);
			for (USceneComponent* Child : Children)
			{
				if (Child->IsA<UCameraComponent>())
				{
					CameraComp = Child;
					CachedCameraLocation = Child->GetComponentLocation();
					CachedCameraRotation = Child->GetComponentRotation();
					break;
				}
			}
			
			CharMesh->AttachToComponent(CharOwner->GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			CharMesh->SetRelativeLocationAndRotation(CharOwner->GetBaseTranslationOffset(), CharOwner->GetBaseRotationOffset());

			if (UNarrativeAnimInstance* AnimInst = GetCharacterAnimInstance())
			{
				if (URagdollAnimInstance* RagdollInst = Cast<URagdollAnimInstance>(AnimInst->GetOverrideLayerAnimInstance()))
				{
					FPoseSnapshot& RagdollSnapshot = RagdollInst->CreateRagdollSnapshot();

					FReferenceSkeleton ReferenceSkeleton = CharMesh->GetSkinnedAsset()->GetRefSkeleton();

					const int32 PelvisBoneIndex = ReferenceSkeleton.FindBoneIndex(NAME_PelvisBone);

					if (PelvisBoneIndex >= 0)
					{
						// Now that we've moved mesh back to capsule, we can get pelvis relative to root 
						RagdollInst->RagdollGetUpSnapshot.LocalTransforms[PelvisBoneIndex] = PelvisTransform.GetRelativeTransform(CharMesh->GetComponentTransform());

						UAnimMontage* GetUpMontage = bRagdollFacingUpward ? RagdollGetUpFromBackMontage : RagdollGetUpFromFrontMontage;

						//Play get up as an attach warp, because this will make camera do what we want 
						GetNarrativeCharacterOwner()->IsPlayingAttachWarpMontage = true;
						GetNarrativeCharacterOwner()->GetMesh()->GetAnimInstance()->Montage_Play(GetUpMontage);
						FOnMontageEnded BlendOutDelegate;
						BlendOutDelegate.BindUObject(this, &UNarrativeCharacterMovement::OnMontageEnded);
						GetNarrativeCharacterOwner()->GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(BlendOutDelegate, GetUpMontage);
					}

					// Have mesh component tick and update transforms to fix frame-1 offset
					CharMesh->TickAnimation(0.0f, false);
					CharMesh->RefreshBoneTransforms();
					
					// Apply the cached camera location/rotation
					if (CameraComp)
					{
						CameraComp->SetWorldLocationAndRotation(CachedCameraLocation, CachedCameraRotation);
					} 
				}

				if (AnimInst->GetOverrideLayerTag() == FNarrativeGameplayTags::Get().Narrative_Anim_OverrideLayer_Ragdoll)
				{
					//Networked ragdolls never want to blend out because they are only used for death since they don't net-sync. 
					const float BlendOutTime = GetNetMode() == NM_Standalone ? 0.3f : 0.f;
					AnimInst->RemoveOverrideLayer(BlendOutTime);
				}
			}

			NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
			bIgnoreClientMovementErrorChecksAndCorrection = false;
		}
	}
}

bool UNarrativeCharacterMovement::IsCustomMovementMode(ENarrativeCustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}

bool UNarrativeCharacterMovement::IsMovementMode(EMovementMode InMovementMode) const
{
	return InMovementMode == MovementMode;
}

bool UNarrativeCharacterMovement::IsServer() const
{
	return CharacterOwner->HasAuthority();
}

float UNarrativeCharacterMovement::CapR() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

float UNarrativeCharacterMovement::CapHH() const
{
	return CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

FVector UNarrativeCharacterMovement::CapB(const float ZOffset) const
{
	return UpdatedComponent->GetComponentLocation() + FVector(0.f, 0.f, ZOffset + (-CapHH()));
}

FVector UNarrativeCharacterMovement::CapLoc() const
{
	return UpdatedComponent->GetComponentLocation();
}

FVector UNarrativeCharacterMovement::CapG(const float ZOffset) const
{
	return CapB(ZOffset) - FVector(0.f, 0.f, UCharacterMovementComponent::MIN_FLOOR_DIST);
}

FVector2D UNarrativeCharacterMovement::GetLocalInputVector() const
{

	//Can just read local input vector. 
	if (GetCharacterOwner() && GetCharacterOwner()->IsLocallyControlled())
	{
		//FVector LocalAcceleration = GetCharacterOwner()->GetControlRotation().UnrotateVector(Acceleration).GetSafeNormal2D();
		//FVector2D LocalInput(LocalAcceleration);
		//LocalInput.X = FMath::Sign(LocalAcceleration.X) * FMath::Sqrt(FMath::Pow(LocalAcceleration.X, 2.f) + FMath::Pow(LocalAcceleration.Z, 2.f));

		if (ANarrativePlayerCharacter* PChar = Cast<ANarrativePlayerCharacter>(GetCharacterOwner()))
		{
			return FVector2D(PChar->MovementVector.X, PChar->MovementVector.Y);
		}
		else // TODOthis should not be 1,0, but clearly is used to force NPCs to jump upwards
		{
			return FVector2D(1.f, 0.f);
		}
	}
	else
	{
		FVector LocalAcceleration = GetCharacterOwner()->GetControlRotation().UnrotateVector(Acceleration).GetSafeNormal2D();
		return FVector2D(LocalAcceleration);
	}

}

class UNarrativeAnimInstance* UNarrativeCharacterMovement::GetCharacterAnimInstance() const
{
	if (ANarrativeCharacter* NChar = GetNarrativeCharacterOwner())
	{
		return NChar->GetCharacterAnimInstance();
	}

	return nullptr; 
}

FCollisionQueryParams UNarrativeCharacterMovement::GetIgnoreCharacterParams() const
{
	FCollisionQueryParams CQP;

	if (NarrativeCharacterOwner)
	{
		CQP = NarrativeCharacterOwner->GetIgnoreCharacterParams();
	}

	return CQP;
}



void FSavedMove_NarrativeCharacter::FSavedMove_NarrativeCharacter::Clear()
{
	Super::Clear();

	bSavedWantsSlowWalk = false;
	bSavedWantsSprint = false;
}

uint8 FSavedMove_NarrativeCharacter::FSavedMove_NarrativeCharacter::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bSavedWantsSprint)
	{
		Result |= FLAG_SPRINT;
	}
	
	if (bSavedWantsSlowWalk)
	{
		Result |= FLAG_SLOWWALK;
	}

	return Result;
}



bool FSavedMove_NarrativeCharacter::FSavedMove_NarrativeCharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const
{
	if (bSavedWantsSprint != ((FSavedMove_NarrativeCharacter*)&NewMove)->bSavedWantsSprint)
	{
		return false;
	}
	
	if (bSavedWantsSlowWalk != ((FSavedMove_NarrativeCharacter*)&NewMove)->bSavedWantsSlowWalk)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, Character, MaxDelta);
}

void FSavedMove_NarrativeCharacter::FSavedMove_NarrativeCharacter::SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

	if (UNarrativeCharacterMovement* MovementComp = Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()))
	{
		bSavedWantsSprint = MovementComp->bWantsSprint;
		bSavedWantsSlowWalk = MovementComp->bWantsSlowWalk;
	}
}

void FSavedMove_NarrativeCharacter::FSavedMove_NarrativeCharacter::PrepMoveFor(class ACharacter* Character)
{
	Super::PrepMoveFor(Character);
	
	if (UNarrativeCharacterMovement* MovementComp = Cast<UNarrativeCharacterMovement>(Character->GetCharacterMovement()))
	{
		MovementComp->bWantsSprint = bSavedWantsSprint;
		MovementComp->bWantsSlowWalk = bSavedWantsSlowWalk;
	}
}

FNetworkPredictionData_Client_NarrativeCharacter::FNetworkPredictionData_Client_NarrativeCharacter(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement)
{

}

FSavedMovePtr FNetworkPredictionData_Client_NarrativeCharacter::FNetworkPredictionData_Client_NarrativeCharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_NarrativeCharacter());
}



FGameplayTag UNarrativeCharacterMovement::GetClimbOverrideLayerTag() const
{
	if (ANarrativeCharacter* NChar = GetNarrativeCharacterOwner())
	{
		if (AActor* Climbable = NChar->AttachWarpProps.ClimbableActor)
		{
			if (Climbable && Climbable->Implements<UClimbableActor>())
			{
				return IClimbableActor::Execute_GetClimbOverrideTag(Climbable);
			}
		}
	}

	return FNarrativeGameplayTags::Get().Narrative_Anim_OverrideLayer_Climbing;
}

bool UNarrativeCharacterMovement::TryFindAttachTransform(FAttachWarpProps &OutAttachWarpProps, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const
{
	if(!GetOwner())
	{
		return false;
	}
	
	AActor* Climbable = OutAttachWarpProps.ClimbableActor; 

	FAttachWarpProps OldProps = OutAttachWarpProps;

	FAttachWarpProps::ClearProps(OutAttachWarpProps);

	if (ANarrativeCharacter* NChar = GetNarrativeCharacterOwner())
	{
		if (IsClimbing())
		{
			//If we are on a climbable interface object like monkeybars, a pole, ladder etc, ask that for the warp props instead of using hardcoded logic as we used to have. 
			if (Climbable && Climbable->Implements<UClimbableActor>())
			{
				return IClimbableActor::Execute_TryFindClimbTransform(Climbable, OutAttachWarpProps, OldProps, GetNarrativeCharacterOwner(), PressedJump, InputVector, OptionalBlendInTime);
			} // If we are on a legacy climable, we use liams old logic of selecting via chooser. This could eventually be moved into a climbable interface too - cleaner. 
			else if (PressedJump || FMath::Abs(InputVector.X) > 0.8f) 
			{
				const bool bFoundClimb = TryFindClimbTransform(OutAttachWarpProps, PressedJump, InputVector, OptionalBlendInTime);

				//if (InputVector.Y <= -1.f && !GetNarrativeCharacterOwner()->IsPlayingAttachWarpMontage)
				//{
				//	if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
				//	{
				//		if (AnimInstance->GetOverrideLayerTag() == GetClimbOverrideLayerTag())
				//		{
				//			AnimInstance->RemoveOverrideLayer(0.3f);
				//		}
				//	}

				//	//SetMovementMode(MOVE_Falling);
				//	return true;
				//}


				return bFoundClimb;
			}
		}
		else if (MovementMode == MOVE_Walking || MovementMode == MOVE_Falling || MovementMode == MOVE_Swimming)
		{
			return TryFindTraversalTransform(OutAttachWarpProps, PressedJump, InputVector, OptionalBlendInTime);
		}
	}

	return false;
}

bool UNarrativeCharacterMovement::TryFindClimbTransform(FAttachWarpProps& OutAttachWarpProps, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const
{

	if (bDisableClimbAndMantles)
	{
		return false; 
	}
	
	//init our trace params
	const AActor* OwnerActor = GetOwner();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);
	Params.bTraceComplex = false;

	//init our forward trace start and end point points, this trace will be used to find the front face of any climbable object
	FVector ForwardTraceStart = FVector::ZeroVector;
	FVector ForwardTraceEnd = FVector::ZeroVector;

	float ForwardTraceCapsuleHalfHeight;
	const float TraceCapsuleRadius = CapR() - 1.0f;
		
	const bool bIsUpwardJump = InputVector.Y >= 0.f;

	if(PressedJump)
	{
		ForwardTraceStart = UpdatedComponent->GetComponentLocation() + (bIsUpwardJump ? FVector::UpVector * ClimbTraceJumpVerticalDistance : -FVector::UpVector * ClimbTraceJumpVerticalDistance) 
		+ (FMath::IsNearlyZero(InputVector.X) ? FVector::ZeroVector : UpdatedComponent->GetRightVector() * FMath::Sign(InputVector.X) * ClimbTraceJumpHorizontalDistance);
		ForwardTraceEnd = ForwardTraceStart + UpdatedComponent->GetForwardVector() * 50;
		ForwardTraceCapsuleHalfHeight = CapHH();
	}
	else
	{
		ForwardTraceStart = UpdatedComponent->GetComponentLocation() + FVector::UpVector * 20 ;
		ForwardTraceEnd = ForwardTraceStart + UpdatedComponent->GetForwardVector() * CapR();
		ForwardTraceCapsuleHalfHeight = CapHH() * 0.5f;
	}

#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		if(IsMovingOnGround())
		{
			DrawDebugCapsule(GetWorld(), ForwardTraceStart, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false, 5.f);
			DrawDebugCapsule(GetWorld(), ForwardTraceEnd, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false, 5.f);
		}
		else
		{
			DrawDebugCapsule(GetWorld(), ForwardTraceStart, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false);
			DrawDebugCapsule(GetWorld(), ForwardTraceEnd, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Magenta, false);
		}
	}
#endif
	//the forward Trace for Climbable Objects
	FHitResult ClimbableTraceHit;
    	GetWorld()->SweepSingleByChannel(ClimbableTraceHit,
    		ForwardTraceStart,
    		ForwardTraceEnd,
    		FQuat::Identity,
    		TraceChannel_NarrativeClimbable,
    		FCollisionShape::MakeCapsule(TraceCapsuleRadius, ForwardTraceCapsuleHalfHeight),
    		Params);
	
	if(ClimbableTraceHit.IsValidBlockingHit() || ClimbableTraceHit.bBlockingHit)
	{
		OutAttachWarpProps.IsClimbableObject = true;
		
		//Our trace returned a hit, we can use this hit to identify our forward face but we also need a top face
		//to find a ledge, we do this with a downward line trace, we init the start and end locations here
		const FVector TargetDirection = -ClimbableTraceHit.ImpactNormal.GetSafeNormal2D();
		const FVector TargetLocationOffset = TargetDirection * 15;
		const FVector DownwardTraceStart
		{
			ClimbableTraceHit.ImpactPoint.X + TargetLocationOffset.X,
			ClimbableTraceHit.ImpactPoint.Y + TargetLocationOffset.Y,
			ClimbableTraceHit.ImpactPoint.Z + 80
		};
		const FVector DownwardTraceEnd
		{
			DownwardTraceStart.X,
			DownwardTraceStart.Y,
			DownwardTraceStart.Z - 120
		};

		//this is where we trace for the top face of a climbable object
		FHitResult ClimbableDownwardTrace;
		GetWorld()->LineTraceSingleByChannel(
			ClimbableDownwardTrace,
			DownwardTraceStart,
			DownwardTraceEnd,
			TraceChannel_NarrativeClimbable,
			Params);

		//our climbable object does not have a top face we can use because the geo is to thin so we early out
		if(!ClimbableDownwardTrace.IsValidBlockingHit())
		{
			return false;
		}

		//the flat direction between our front face hit and our top face hit
		FVector FlatDirectionBetweenHits = ClimbableTraceHit.ImpactPoint - ClimbableDownwardTrace.ImpactPoint;
		FlatDirectionBetweenHits.Z = 0;

		//when we call FindLedgeEdge(); our vertical segment will always be a projection along the Up axis,
		//but for our Horizontal segment we need to project along the X axis of the transform below
		FTransform TopFaceTransform = FTransform(
			FRotationMatrix::MakeFromXZ(FlatDirectionBetweenHits, FVector::UpVector).Rotator(),
			ClimbableDownwardTrace.ImpactPoint,
			FVector(1,1,1));

		//This is the ledge location of the climbable object
		FVector FrontLedgeLocation = FindLedgeEdge(ClimbableTraceHit, ClimbableDownwardTrace,  TopFaceTransform, true);
		
		//here we set the rotation of our LedgeTransform, we will use this transform inside the motion warping window of our montage
		//to orient the character 
		const FVector ProjectedDirectionBetweenHits = ClimbableDownwardTrace.ImpactPoint + FVector::VectorPlaneProject(FlatDirectionBetweenHits * 200, ClimbableDownwardTrace.ImpactNormal);
		FVector DirectionToLedge = ClimbableDownwardTrace.ImpactPoint - ProjectedDirectionBetweenHits;
		DirectionToLedge.Z = 0;
		DirectionToLedge.Normalize();
		FRotator WarpTargetRot = DirectionToLedge.Rotation();
		OutAttachWarpProps.LedgeTransform.SetRotation(WarpTargetRot.Quaternion());

		//This is where we handle strafing along a ledge, we create a matrix from our traces in order to  trace left or right
		//along the ledge edge 
		FMatrix LedgeMatrix(FRotationMatrix::MakeFromZX(ClimbableDownwardTrace.Normal, ClimbableTraceHit.Normal));
		
		FHitResult FrontLedgeCheckHit;
		GetWorld()->SweepSingleByChannel(
			FrontLedgeCheckHit,
			FrontLedgeLocation + LedgeMatrix.GetUnitAxis(EAxis::Y) * 150 * -InputVector.X,
			FrontLedgeLocation,
			FQuat::Identity,
			TraceChannel_NarrativeClimbable,
			FCollisionShape::MakeSphere(3),
			Params);
		

		//Check if ledge is too short for our move to get the distance
		bool bShortClimbStrafe = false;

		const float CornerTurnLength = 41.f;
		const float ShortStrafeLength = 141.f;

		if(InputVector.X != 0 && !PressedJump)
		{
			//check for corner turn 
			if((FrontLedgeCheckHit.ImpactPoint - FrontLedgeLocation).Length() < CornerTurnLength)
			{
				FrontLedgeLocation = FrontLedgeCheckHit.ImpactPoint - LedgeMatrix.GetUnitAxis(EAxis::X) * 40;
				OutAttachWarpProps.LedgeTransform.SetRotation(WarpTargetRot.Quaternion() * FQuat(FVector::UpVector, -FMath::Sign(InputVector.X) * (PI / 2)));
			}
			//check for short strafe
			else if((FrontLedgeCheckHit.ImpactPoint - FrontLedgeLocation).Length() < ShortStrafeLength)
			{
				FrontLedgeLocation = FrontLedgeCheckHit.ImpactPoint - LedgeMatrix.GetUnitAxis(EAxis::Y) * 40 * -InputVector.X;
				bShortClimbStrafe = true;	
			}
			//check if our ledge is angled up or down
			else
			{
				float OldZ = FrontLedgeLocation.Z;
				float NewZ = (FrontLedgeLocation + LedgeMatrix.GetUnitAxis(EAxis::Y) * 70 * -InputVector.X).Z;
				FrontLedgeLocation = FrontLedgeLocation + LedgeMatrix.GetUnitAxis(EAxis::Y) * 70 * -InputVector.X;
				if(!FMath::IsWithin(NewZ, OldZ - 5, OldZ + 5))
				{
					bShortClimbStrafe = true;
				}
			}
		} 

		//Fill out the rest of our AttachWarpProps
		OutAttachWarpProps.YawRotationToLedge = (UpdatedComponent->GetComponentTransform().InverseTransformVectorNoScale(
			(OutAttachWarpProps.LedgeTransform.GetLocation() - UpdatedComponent->GetComponentLocation()).
			GetSafeNormal2D())).ToOrientationRotator().Yaw;
		OutAttachWarpProps.LedgeTransform.SetLocation(FrontLedgeLocation);
		OutAttachWarpProps.CurrentLedgeTransform = OutAttachWarpProps.LedgeTransform;

		if (bShortClimbStrafe)
		{
			OutAttachWarpProps.OptionalBlendInTime = .3f;
		}
		else
		{
			OutAttachWarpProps.OptionalBlendInTime = OptionalBlendInTime;
		}
		OutAttachWarpProps.HasFrontLedge = true;
		OutAttachWarpProps.CurrentMovementMode = MOVE_Custom;
		
#if ENABLE_DRAW_DEBUG
		//Draw debug shapes/lines
		if (CVarClimbDebug.GetValueOnAnyThread())
		{
			DrawDebugLine(GetWorld(), DownwardTraceStart, DownwardTraceEnd, FColor::Red, false, 5.f);
			DrawDebugBox(GetWorld(), ClimbableTraceHit.ImpactPoint, FVector(3,3,3), FColor::Magenta, false, 5.f);
			DrawDebugBox(GetWorld(), ClimbableDownwardTrace.ImpactPoint, FVector(3,3,3), FColor::Red, false, 5.f);
			DrawDebugBox(GetWorld(), FrontLedgeLocation, FVector(4, 4, 4), FColor::Orange, false, 5.f);
		}
	#endif 

		OutAttachWarpProps.CurrentLedgeTransform.SetLocation(FrontLedgeLocation);

		//Test the the climb location to see if our character can get there without overlapping with geo
		if (IsClimbLocationIsValidForCharacter(OutAttachWarpProps.LedgeTransform))
		{
			OutAttachWarpProps.ClimbableActor = ClimbableTraceHit.GetActor();
			return true;
		}
		return false;
	}
	
	//this is where we check for climb to standing 
	if(InputVector.Y >= 1 && IsTraversalLocationIsValidForCharacter( OutAttachWarpProps.CurrentLedgeTransform)) 
	{
		OutAttachWarpProps.LedgeTransform = OutAttachWarpProps.CurrentLedgeTransform;
		OutAttachWarpProps.HasFrontLedge = true;
		OutAttachWarpProps.ObstacleDepth = 100;
		OutAttachWarpProps.CurrentMovementMode = MOVE_Custom;
		OutAttachWarpProps.NewMovementMode = MOVE_Walking;
		OutAttachWarpProps.bIsCameraInsideHead = GetNarrativeCharacterOwner()->IsCameraInsideHead();
		
		if (UNarrativeAnimInstance* AnimInstance = GetNarrativeCharacterOwner()->GetCharacterAnimInstance())
		{
			AnimInstance->RemoveOverrideLayer(0.2f);	
		}
		
		return true;
	}
	return false;
}

bool UNarrativeCharacterMovement::TryFindTraversalTransform(FAttachWarpProps& OutAttachWarpProps, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const
{

	//cache some constants for use throughout the function
	const AActor* OwnerActor = GetOwner();
	const FVector ActorLocation = OwnerActor->GetActorLocation();
	const float TraceCapsuleRadius = CapR() - 1.0f;
	const FVector CapsuleBottomLocation = FVector(ActorLocation.X, ActorLocation.Y, ActorLocation.Z - CapHH());

	//init our trace params
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerActor);
	Params.bTraceComplex = false;

	//init our forward sweep start and end point points and capsule size, this sweep will be used to find the front face of any object we hit
	FVector ForwardTraceStart = FVector::ZeroVector;
	FVector ForwardTraceEnd = FVector::ZeroVector;
	FVector ForwardTraceEndClimbCheck = FVector::ZeroVector;
	float ForwardTraceCapsuleHalfHeight;

	if(MovementMode == MOVE_Swimming)
	{
		ForwardTraceStart = CapsuleBottomLocation - UpdatedComponent->GetForwardVector() * CapR();
		ForwardTraceCapsuleHalfHeight = CapHH() *( IsMovingOnGround() ? TraversalTraceCapsuleHalfHeightScale : TraversalTraceCapsuleHalfHeightScaleFalling);
		ForwardTraceStart.Z +=  ForwardTraceCapsuleHalfHeight + MaxStepHeight * 2;
		ForwardTraceEnd = ForwardTraceStart + OwnerActor->GetActorForwardVector() * TraversalTraceForwardDistanceSwimming;
	}
	else if(IsMovingOnGround())
	{
		ForwardTraceStart = CapsuleBottomLocation - UpdatedComponent->GetForwardVector() * CapR();
		ForwardTraceCapsuleHalfHeight = CapHH() * TraversalTraceCapsuleHalfHeightScale;
		ForwardTraceStart.Z +=  ForwardTraceCapsuleHalfHeight + MaxStepHeight;
		ForwardTraceEnd = Acceleration.IsNearlyZero() ?
		ForwardTraceStart + OwnerActor->GetActorForwardVector() * (CapR() + (TraversalTraceForwardDistance * 0.5 + 1.f)):
		ForwardTraceStart + Acceleration.GetSafeNormal() * (CapR() + (TraversalTraceForwardDistance + 1.f));
		ForwardTraceEndClimbCheck = ForwardTraceEnd + OwnerActor->GetActorForwardVector() * TraversalTraceForwardDistanceClimbCheck;
	}
	else
	{
		ForwardTraceStart = CapsuleBottomLocation - UpdatedComponent->GetForwardVector() * CapR();
		ForwardTraceCapsuleHalfHeight = CapHH() * TraversalTraceCapsuleHalfHeightScaleFalling;
		ForwardTraceStart.Z +=  ForwardTraceCapsuleHalfHeight + MaxStepHeight;
		ForwardTraceEnd = Acceleration.IsNearlyZero() ?
		ForwardTraceStart + OwnerActor->GetActorForwardVector() * (CapR() + (TraversalTraceForwardDistanceFalling * 0.5 + 1.f)) :
		ForwardTraceStart + Acceleration.GetSafeNormal() * (CapR() + (IsMovingOnGround() ? TraversalTraceForwardDistance : TraversalTraceForwardDistanceFalling + 1.f));
		ForwardTraceStart.Z = UpdatedComponent->GetComponentLocation().Z + (Velocity.Z * 0.1f);
		ForwardTraceEnd.Z = UpdatedComponent->GetComponentLocation().Z + (Velocity.Z * 0.1f);
		ForwardTraceEndClimbCheck = ForwardTraceEnd + OwnerActor->GetActorForwardVector() * TraversalTraceForwardDistanceClimbCheck;
	}

	//First we sweep for any objects blocking the TraceChannel_NarrativeClimbable, this is because we need to play a traversal montage in order to get
	//to the climbing state, we also prioritise climbing over traversing, hence it being the first trace - TODO may need changed, as you teleport through traversables to jump onto climbable which is jank. 
	FHitResult ClimbableTraceHit;
	GetWorld()->SweepSingleByChannel(ClimbableTraceHit,
    		ForwardTraceStart,
    		IsMovingOnGround() ? ForwardTraceEndClimbCheck : ForwardTraceEnd,
    		FQuat::Identity,
    		TraceChannel_NarrativeClimbable,
    		FCollisionShape::MakeCapsule(TraceCapsuleRadius, ForwardTraceCapsuleHalfHeight),
    		Params);
	
	if (ClimbableTraceHit.IsValidBlockingHit() || ClimbableTraceHit.bBlockingHit)
	{
		//Our trace returned a hit, we can use this hit to identify our forward face but we also need a top face
		//to find a ledge, we do this with a downward line trace, we init the start and end locations here
		OutAttachWarpProps.IsClimbableObject = true;

		AActor* HitClimbable = ClimbableTraceHit.GetActor();

		const FVector TargetDirection = -ClimbableTraceHit.ImpactNormal.GetSafeNormal2D();
		const FVector TargetLocationOffset = TargetDirection * 15;
		const FVector DownwardTraceStart
		{
			ClimbableTraceHit.ImpactPoint.X + TargetLocationOffset.X,
			ClimbableTraceHit.ImpactPoint.Y + TargetLocationOffset.Y,
			ClimbableTraceHit.ImpactPoint.Z + 80
		};
		const FVector DownwardTraceEnd
		{
			DownwardTraceStart.X,
			DownwardTraceStart.Y,
			DownwardTraceStart.Z - 120
		};

		//this is where we trace for the top face of a climbable object
		FHitResult ClimbableDownwardTrace;
		GetWorld()->LineTraceSingleByChannel(
			ClimbableDownwardTrace,
			DownwardTraceStart,
			DownwardTraceEnd,
			TraceChannel_NarrativeClimbable,
			Params);

		////our climbable object does not have a top face we can use because the geo is to thin so we early out
		//if (!ClimbableDownwardTrace.IsValidBlockingHit())
		//{
		//	return false;
		//}

#if ENABLE_DRAW_DEBUG
		if (CVarClimbDebug.GetValueOnAnyThread())
		{
			DrawDebugLine(GetWorld(), DownwardTraceStart, DownwardTraceEnd, FColor::Red, false, 5.f);
			DrawDebugBox(GetWorld(), ClimbableDownwardTrace.ImpactPoint, FVector(3, 3, 3), FColor::Red, false, 5.f);
		}
#endif 
		//the flat direction between our front face hit and our top face hit
		FVector FlatDirectionBetweenHits = ClimbableTraceHit.ImpactPoint - ClimbableDownwardTrace.ImpactPoint;
		FlatDirectionBetweenHits.Z = 0;

		//when we call FindLedgeEdge(); our vertical segment will always be a projection along the Up axis,
		//but for our Horizontal segment we need to project along the X axis of the transform below
		FTransform TopFaceTransform = FTransform(
			FRotationMatrix::MakeFromXZ(FlatDirectionBetweenHits, FVector::UpVector).Rotator(),
			ClimbableDownwardTrace.ImpactPoint,
			FVector(1, 1, 1));

		//This is the ledge location of the climbable object
		FVector FrontLedgeLocation = FindLedgeEdge(ClimbableTraceHit, ClimbableDownwardTrace, TopFaceTransform, true);

		//here we set the rotation of our LedgeTransform, we will use this transform inside the motion warping window of our montage
		//to orient the character
		const FVector ProjectedDirectionBetweenHits = ClimbableDownwardTrace.ImpactPoint + FVector::VectorPlaneProject(FlatDirectionBetweenHits * 200, ClimbableDownwardTrace.ImpactNormal);
		FVector DirectionToLedge = ClimbableDownwardTrace.ImpactPoint - ProjectedDirectionBetweenHits;
		DirectionToLedge.Z = 0;
		DirectionToLedge.Normalize();
		FRotator WarpTargetRot = DirectionToLedge.Rotation();
		OutAttachWarpProps.LedgeTransform.SetRotation(WarpTargetRot.Quaternion());
		OutAttachWarpProps.LedgeTransform.SetLocation(FrontLedgeLocation);
		OutAttachWarpProps.HasFrontLedge = true;
		OutAttachWarpProps.CurrentLedgeTransform = OutAttachWarpProps.LedgeTransform;
		OutAttachWarpProps.CurrentMovementMode = MovementMode;
		OutAttachWarpProps.OverrideLayerBlendIn = 0.f;
		OutAttachWarpProps.OverrideLayerBlendOut = 0.f;
		OutAttachWarpProps.OptionalBlendInTime = -1.f;
		OutAttachWarpProps.bIsCameraInsideHead = GetNarrativeCharacterOwner()->IsCameraInsideHead();
	
		

		if (HitClimbable->Implements<UClimbableActor>())
		{
			if (!IClimbableActor::Execute_AdjustInitialWarpProps(HitClimbable, OutAttachWarpProps, ClimbableTraceHit, GetNarrativeCharacterOwner(), PressedJump, InputVector, OptionalBlendInTime))
			{
				return false; 
			}

			OutAttachWarpProps.ClimbableActor = HitClimbable;

			return true; 
		}
		else
		{
			//For  climbable object does not have a top face we cannot use because the geo is to thin so we early out
			if (!ClimbableDownwardTrace.IsValidBlockingHit())
			{
				return false;
			}

			//Climbable actors dont need this check as AdjustInitialWarpProps acts as the check. 
			if (IsClimbLocationIsValidForCharacter(OutAttachWarpProps.LedgeTransform))
			{
				OutAttachWarpProps.ClimbableActor = HitClimbable;

				return true;
			}
		}

	}
	
	// We didnt hit any climbable objects so now  Trace for Traversable objects
	FHitResult ForwardTraceHit;
	GetWorld()->SweepSingleByChannel(ForwardTraceHit,
		ForwardTraceStart,
		ForwardTraceEnd,
		FQuat::Identity,
		TraceChannel_NarrativeTraversable,
		FCollisionShape::MakeCapsule(TraceCapsuleRadius, ForwardTraceCapsuleHalfHeight),
		Params);
	
#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		if(IsMovingOnGround())
		{
			DrawDebugCapsule(GetWorld(), ForwardTraceStart, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false, 5.f);
			DrawDebugCapsule(GetWorld(), ForwardTraceEnd, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false, 5.f);
			DrawDebugCapsule(GetWorld(), ForwardTraceEndClimbCheck, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Magenta, false, 5.f);
		}
		else
		{
			DrawDebugCapsule(GetWorld(), ForwardTraceStart, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false);
			DrawDebugCapsule(GetWorld(), ForwardTraceEnd, ForwardTraceCapsuleHalfHeight, TraceCapsuleRadius, UpdatedComponent->GetComponentQuat(), FColor::Blue, false);
		}
	}
#endif 

	//Query the hit primitive component to see if its walkable and not moving to fast	
	UPrimitiveComponent* TargetPrimitive = ForwardTraceHit.GetComponent();
	if(!ForwardTraceHit.IsValidBlockingHit() || !IsValid(TargetPrimitive) ||
		TargetPrimitive->GetComponentVelocity().SizeSquared() > FMath::Square(10) ||
		!TargetPrimitive->CanCharacterStepUp(PawnOwner) || IsWalkable(ForwardTraceHit))
	{
		return false;
	}

	//we need to find the top face, we do this be offsetting from the front face
	const auto TargetDirection = -ForwardTraceHit.ImpactNormal.GetSafeNormal2D();
	const FVector TargetLocationOffset = TargetDirection * 15;
	
	//init our downward trace start and end
	FVector2D LedgeHeight;
	LedgeHeight = FVector2D(TraversalTraceHeightMin, TraversalTraceHeightMax);
	float LedgeHeightDelta = LedgeHeight.Y - LedgeHeight.X;

	const FVector DownwardTraceStart{
		ForwardTraceHit.ImpactPoint.X + TargetLocationOffset.X,
		ForwardTraceHit.ImpactPoint.Y + TargetLocationOffset.Y,
		CapsuleBottomLocation.Z + LedgeHeightDelta + 2.5f * TraceCapsuleRadius + UCharacterMovementComponent::MIN_FLOOR_DIST};
	
	const FVector DownwardTraceEnd{
		DownwardTraceStart.X,
		DownwardTraceStart.Y,
		CapsuleBottomLocation.Z + LedgeHeight.X + TraceCapsuleRadius - UCharacterMovementComponent::MAX_FLOOR_DIST};
	
	//Trace down to find the top of the object hit by the forward trace and see if the slope is walkable
	FHitResult DownwardTraceHit;
	GetWorld()->SweepSingleByChannel(
		DownwardTraceHit,
		DownwardTraceStart,
		DownwardTraceEnd,
		FQuat::Identity,
		TraceChannel_NarrativeTraversable,
		FCollisionShape::MakeSphere(TraceCapsuleRadius),
		Params);

	//test the slope of the top face to check if we can 
	const float SlopeAngleCos(DownwardTraceHit.ImpactNormal.Z);
	auto ApproximateSlopeNormal = DownwardTraceHit.Location - DownwardTraceHit.ImpactPoint;
	ApproximateSlopeNormal.Normalize();
	float ApproximateSlopeAngleCos(ApproximateSlopeNormal.Z);
	if(SlopeAngleCos < 0.81f || ApproximateSlopeAngleCos < 0.81f || !IsWalkable(DownwardTraceHit))
	{
		return false;
	}

	//the flat direction between our front face hit and our top face hit
	FVector FlatDirectionBetweenHits = ForwardTraceHit.ImpactPoint - DownwardTraceHit.ImpactPoint;
	FlatDirectionBetweenHits.Z = 0;

	//when we call FindLedgeEdge(); our vertical segment will always be a projection along the Up axis,
	//but for our Horizontal segment we need to project along the X axis of the transform below
	FTransform TopFaceTransform = FTransform(
		FRotationMatrix::MakeFromXZ(FlatDirectionBetweenHits, FVector::UpVector).Rotator(),
		DownwardTraceHit.ImpactPoint,
		FVector(1,1,1));
	
	//This is the ledge location of the climbable object
	FVector FrontLedgeLocation = FindLedgeEdge(ForwardTraceHit, DownwardTraceHit, TopFaceTransform, true);
	
	//here we set the rotation of our LedgeTransform, we will use this transform inside the motion warping window of our montage
	//to orient the character 
	const FVector ProjectedDirectionBetweenHits = DownwardTraceHit.ImpactPoint + FVector::VectorPlaneProject(TopFaceTransform.GetUnitAxis(EAxis::X) * 200, DownwardTraceHit.ImpactNormal);
	FVector DirectionToLedge = DownwardTraceHit.ImpactPoint - ProjectedDirectionBetweenHits;
	DirectionToLedge.Z = 0;
	DirectionToLedge.Normalize();
	FRotator WarpTargetRot = DirectionToLedge.Rotation();
	OutAttachWarpProps.LedgeTransform.SetRotation(WarpTargetRot.Quaternion());
	
	//trace down to find if there is a back floor a given distance from the ledge 
	FHitResult BackFloorCheckHitResult;
	FVector LastTopFaceHit = FVector::ZeroVector;
	FVector LastTopFaceHitNormal = FVector::ZeroVector;
	FVector BackFaceTraceStart = FVector::ZeroVector;
	bool bDoBackLedgeTrace = false;
	for (int i = 0; i < 6; ++i)
	{
		GetWorld()->LineTraceSingleByChannel(
			BackFloorCheckHitResult,
			DownwardTraceHit.ImpactPoint + FVector::UpVector * CapHH() + WarpTargetRot.Vector() * (i * 50),
			DownwardTraceHit.ImpactPoint + FVector::UpVector * CapHH() + WarpTargetRot.Vector() * (i * 50) + FVector::DownVector * 120,
			TraceChannel_NarrativeTraversable);

		#if ENABLE_DRAW_DEBUG
		if (CVarClimbDebug.GetValueOnAnyThread())
		{
			DrawDebugLine(
				GetWorld(),
				DownwardTraceHit.ImpactPoint + FVector::UpVector * CapHH() + WarpTargetRot.Vector() * (i * 50),
				DownwardTraceHit.ImpactPoint + FVector::UpVector * CapHH() + WarpTargetRot.Vector() * (i * 50) + FVector::DownVector * 120,
				FColor::Green,
				true);
		}
		#endif 

		if(!BackFloorCheckHitResult.IsValidBlockingHit())
		{
			BackFaceTraceStart = DownwardTraceHit.ImpactPoint + FVector::UpVector * CapHH() + WarpTargetRot.Vector() * (i * 50) + FVector::DownVector * 120;
			bDoBackLedgeTrace = true;
			break;
		}
		LastTopFaceHit = BackFloorCheckHitResult.ImpactPoint;
		LastTopFaceHitNormal = BackFloorCheckHitResult.Normal;
	}
	
	//Trace to find the Back face of the object we are traversing, this is used to perform the same kind of line segment intersection as we did
	//for the front ledge, but this time its to find the back ledge
	if(bDoBackLedgeTrace)
	{
		FHitResult BackFaceHitResult;
		GetWorld()->LineTraceSingleByChannel(
			BackFaceHitResult,
			BackFaceTraceStart,
			BackFaceTraceStart - WarpTargetRot.Vector() * 100,
			TraceChannel_NarrativeTraversable);
		
		if(BackFaceHitResult.IsValidBlockingHit())
		{
			OutAttachWarpProps.BackLedgeLocation = FindLedgeEdge(BackFaceHitResult, DownwardTraceHit, TopFaceTransform, false);
			OutAttachWarpProps.HasBackLedge = true;
		}
	}
	else
	{
		OutAttachWarpProps.HasBackLedge = false;
	}
	
	// trace to find the floor on the other side of the traversal object, used for moves like vault where we ned a reference to the floor
	// for motion warping 
	FHitResult BackFloorHit;
	GetWorld()->LineTraceSingleByChannel(
		BackFloorHit,
		OutAttachWarpProps.BackLedgeLocation + -TopFaceTransform.GetUnitAxis(EAxis::X) * 120,
		OutAttachWarpProps.BackLedgeLocation + -TopFaceTransform.GetUnitAxis(EAxis::X) * 120 + FVector::DownVector * 120 + FVector::DownVector * 100,
		TraceChannel_NarrativeTraversable);
	if(BackFloorHit.IsValidBlockingHit())
	{
		OutAttachWarpProps.HasBackFloor = true;
		OutAttachWarpProps.BackFloorLocation = BackFloorHit.ImpactPoint;
	}
	else
	{
		OutAttachWarpProps.HasBackFloor = false;
	}
	
	//fill in the rest of our AttachWarpProps 
	OutAttachWarpProps.LedgeTransform.SetLocation(FrontLedgeLocation);
	OutAttachWarpProps.HasFrontLedge = true;
	OutAttachWarpProps.NewMovementMode = MOVE_Walking;
	OutAttachWarpProps.IsClimbableObject = false;
	OutAttachWarpProps.ObstacleHeight = FrontLedgeLocation.Z - (UpdatedComponent->GetComponentLocation() + FVector::DownVector * CapHH()).Z + MAX_FLOOR_DIST;
	OutAttachWarpProps.BackLedgeHeight = OutAttachWarpProps.BackLedgeLocation.Z - (UpdatedComponent->GetComponentLocation() + FVector::DownVector * CapHH()).Z + MAX_FLOOR_DIST;
	OutAttachWarpProps.Speed = Velocity.Length();
	OutAttachWarpProps.ObstacleDepth = (FrontLedgeLocation - OutAttachWarpProps.BackLedgeLocation).Length();
	OutAttachWarpProps.CurrentMovementMode = MovementMode;
	OutAttachWarpProps.bIsCameraInsideHead = GetNarrativeCharacterOwner()->IsCameraInsideHead();

	#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		
		DrawDebugBox(GetWorld(), OutAttachWarpProps.LedgeTransform.GetLocation(), FVector(3,3,3), FColor::Magenta, false, 5.f);
		DrawDebugBox(GetWorld(), OutAttachWarpProps.BackLedgeLocation, FVector(5,5,5), FColor::Orange, false, 5.f);
		DrawDebugBox(GetWorld(), OutAttachWarpProps.BackFloorLocation, FVector(3,3,3), FColor::Magenta, false, 5.f);

		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Traversal climbing onto %s"), *GetNameSafe(ForwardTraceHit.GetActor())));
	}
	#endif 
	if(IsTraversalLocationIsValidForCharacter(TopFaceTransform))
	{
		return true;
	}
	return false;
}

bool UNarrativeCharacterMovement::IsClimbLocationIsValidForCharacter(const FTransform& LedgeTransform) const
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetNarrativeCharacterOwner());

	const FVector CapsuleLocation = LedgeTransform.GetLocation() - LedgeTransform.GetUnitAxis(EAxis::X) * 45 - LedgeTransform.
		GetUnitAxis(EAxis::Z) * 120 + FVector::UpVector * CapHH();

#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		DrawDebugCapsule(GetWorld(), CapsuleLocation, CapHH(), CapR(), FQuat::Identity, FColor::Purple, false, 5.f);
	}
#endif 

	FHitResult Hit;
	GetWorld()->SweepSingleByChannel(
		Hit,
		CapsuleLocation,
		CapsuleLocation,
		FQuat::Identity,
		ECC_WorldStatic,
		FCollisionShape::MakeCapsule(CapR(), CapHH()),
		Params);
	if (!Hit.bBlockingHit)
	{
		return true;
	}
	return false;
}

bool UNarrativeCharacterMovement::IsTraversalLocationIsValidForCharacter(const FTransform& LedgeTransform) const
{
	FCollisionQueryParams Params = GetIgnoreCharacterParams();

#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		DrawDebugCapsule(GetWorld(), LedgeTransform.GetLocation() + FVector::UpVector * CapHH(), CapHH(), CapR(),
		                 FQuat::Identity,
		                 FColor::Red, false, 5.f);
		DrawDebugCapsule(
			GetWorld(),
			LedgeTransform.GetLocation() + FVector::UpVector * CapHH() + -LedgeTransform.GetUnitAxis(EAxis::X) * CapR(),
			CapHH(), CapR(), FQuat::Identity, FColor::Red, false, 5.f);
	}
#endif 

	FHitResult Hit;
	GetWorld()->SweepSingleByChannel(
		Hit,
		LedgeTransform.GetLocation() + FVector::UpVector * CapHH() + MAX_FLOOR_DIST,
		LedgeTransform.GetLocation() + FVector::UpVector * CapHH() + MAX_FLOOR_DIST + -LedgeTransform.
		GetUnitAxis(EAxis::X) *
		CapR(),
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeCapsule(CapR(), CapHH()),
		Params
	);
	if (!Hit.bBlockingHit)
	{
		return true;
	}
	return false;
}

FVector UNarrativeCharacterMovement::FindLedgeEdge(const FHitResult& ForwardHit, const FHitResult& DownwardHit, const FTransform& ReferenceTransform, bool bFrontLedge) const
{
	
	const FVector FrontLedgeA = DownwardHit.ImpactPoint;
	const FVector FrontLedgeB = DownwardHit.ImpactPoint + FVector::VectorPlaneProject(ReferenceTransform.GetUnitAxis(EAxis::X) * (bFrontLedge ? 200 : - 200), DownwardHit.ImpactNormal);
	
#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		FColor Col;
		bFrontLedge ? Col = FColor::Green : Col = FColor::Red;
		DrawDebugLine(GetWorld(), FrontLedgeA, FrontLedgeB, Col, false, 5.f);
	}
#endif 

	const FVector FrontLedgeC = ForwardHit.ImpactPoint;
	const FVector FrontLedgeD = ForwardHit.ImpactPoint + FVector::VectorPlaneProject(FVector::UpVector * 300, ForwardHit.ImpactNormal);

#if ENABLE_DRAW_DEBUG
	if (CVarClimbDebug.GetValueOnAnyThread())
	{
		FColor Col;
		bFrontLedge ? Col = FColor::Green : Col = FColor::Red;
		DrawDebugLine(GetWorld(), FrontLedgeC, FrontLedgeD, Col, false, 5.f);
	}
#endif

	FVector P1 = FVector::ZeroVector;
	FVector P2 = FVector::ZeroVector;
	FMath::SegmentDistToSegmentSafe(FrontLedgeA, FrontLedgeB, FrontLedgeC, FrontLedgeD, P1, P2);
	return  P1;
}



void UNarrativeCharacterMovement::PlayTraversalAnim(FAttachWarpProps InTraversalProps)
{
	if(UNarrativeAnimInstance* AnimInstance = Cast<UNarrativeAnimInstance>(GetCharacterOwner()->GetMesh()->GetAnimInstance()))
	{
		if (ANarrativeCharacter* NChar = GetNarrativeCharacterOwner())
		{
			NChar->IsPlayingAttachWarpMontage = true;
			NChar->OnStartTraversal.Broadcast(InTraversalProps);

			if (InTraversalProps.OptionalBlendInTime >= 0.f && InTraversalProps.bPressedJump == false)
			{
				FAlphaBlendArgs Args;
				Args.BlendTime = InTraversalProps.OptionalBlendInTime;
				AnimInstance->Montage_PlayWithBlendIn(InTraversalProps.SelectedMontage, Args, InTraversalProps.PlayRate, EMontagePlayReturnType::MontageLength, InTraversalProps.StartTime);
			}
			else
			{
				AnimInstance->Montage_Play(InTraversalProps.SelectedMontage, InTraversalProps.PlayRate, EMontagePlayReturnType::MontageLength, InTraversalProps.StartTime);
			}

			if (UMotionWarpingComponent* MotionWarpingComponent = NChar->GetMotionWarpingComponent())
			{

				if (FVector::Dist(NChar->GetActorLocation(), InTraversalProps.LedgeTransform.GetLocation()) < 2000.f)
				{
					MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
						FName("FrontLedge"),
						InTraversalProps.LedgeTransform.GetLocation(),
						InTraversalProps.LedgeTransform.GetRotation().Rotator());
				}

				if (InTraversalProps.ActionType == ETraversalActionType::Hurdle || InTraversalProps.ActionType == ETraversalActionType::Vault)
				{
					if (FVector::Dist(NChar->GetActorLocation(), InTraversalProps.BackLedgeLocation) < 2000.f)
					{
						MotionWarpingComponent->AddOrUpdateWarpTargetFromLocation(
							FName("BackLedge"),
							InTraversalProps.BackLedgeLocation);
					}

				}
				else
				{
					MotionWarpingComponent->RemoveWarpTarget(FName("BackLedge"));
				}
				if (InTraversalProps.ActionType == ETraversalActionType::Hurdle)
				{
					if (FVector::Dist(NChar->GetActorLocation(), InTraversalProps.BackFloorLocation) < 2000.f)
					{
						MotionWarpingComponent->AddOrUpdateWarpTargetFromLocation(
							FName("BackFloor"),
							InTraversalProps.BackFloorLocation);
					}
				}
				else
				{
					MotionWarpingComponent->RemoveWarpTarget(
						FName("BackFloor"));
				}


			}
			FOnMontageEnded BlendOutDelegate;

			BlendOutDelegate.BindLambda([this, InTraversalProps](UAnimMontage* Montage, bool bInterrupted)
				{
					OnTraversalMontageEndedTraversal(Montage, bInterrupted, InTraversalProps);
				});

			AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, InTraversalProps.SelectedMontage);

			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &UNarrativeCharacterMovement::OnMontageEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, InTraversalProps.SelectedMontage);
			
			SetMovementMode(MOVE_Custom, CMOVE_Climb);
			GetNarrativeCharacterOwner()->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			
			bIgnoreClientMovementErrorChecksAndCorrection = true;
			bServerAcceptClientAuthoritativePosition = true;
		}

	}
}

void UNarrativeCharacterMovement::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	GetNarrativeCharacterOwner()->IsPlayingAttachWarpMontage = false;

	//TODO here we need to read attach warp props to see if you're X tolerance units from the target location to warp to.
	//If not, something has gone wrong and we should correct the character to the intended location. 
}

void UNarrativeCharacterMovement::OnTraversalMontageEndedTraversal(UAnimMontage* Montage, bool bInterrupted, FAttachWarpProps InTraversalProps)
{
	GetNarrativeCharacterOwner()->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	
	FAttachWarpProps WarpProps = GetNarrativeCharacterOwner()->AttachWarpProps;

	FHitResult Hit(CurrentFloor.HitResult);
	Hit.TraceEnd = Hit.TraceStart + MAX_FLOOR_DIST * -GetGravityDirection();
	const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
	ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat());
	bForceNextFloorCheck = true;
	
	//GetNarrativeCharacterOwner()->IsPlayingAttachWarpMontage = false;

	if (WarpProps.NewMovementMode != MOVE_None)
	{
		//Its unlikely for new movement to be swim or ragdoll, so just assume custom means climb, so we can avoid further bloating WarpProps. 
		if (WarpProps.NewMovementMode == MOVE_Custom)
		{
			SetMovementMode(MOVE_Custom, CMOVE_Climb);
		}
		else
		{
			SetMovementMode(WarpProps.NewMovementMode);
		}
	}
	else // Old liam way of doing it, keep for legacy... 
	{
		const ETraversalActionType ActionType = WarpProps.ActionType;

		switch (ActionType)
		{
		case ETraversalActionType::Vault:
			SetMovementMode(MOVE_Falling);
			break;
		case ETraversalActionType::Mantle:
			SetMovementMode(MOVE_Walking);
			break;
		case ETraversalActionType::Hurdle:
			SetMovementMode(MOVE_Walking);
			break;
		case  ETraversalActionType::Climb:
			SetMovementMode(MOVE_Custom, CMOVE_Climb);
			break;
		default:
			SetMovementMode(MOVE_Walking);
		}
	}

	bIgnoreClientMovementErrorChecksAndCorrection = false;
	bServerAcceptClientAuthoritativePosition = false;
}

bool UNarrativeCharacterMovement::ShouldEnterDive()
{
	// disable diving on non-standalone modes
	if (GetNetMode() != NM_Standalone) { return false; }
	
	FPredictProjectilePathParams PredictParams;
	PredictParams.StartLocation = GetNarrativeCharacterOwner()->GetActorLocation();
	PredictParams.LaunchVelocity = Velocity + FVector::UpVector * JumpZVelocity;
	PredictParams.bTraceWithCollision = true;
	PredictParams.ProjectileRadius = 0;
	PredictParams.MaxSimTime = 5;
	PredictParams.bTraceWithChannel = true;
	PredictParams.TraceChannel = ECC_WorldStatic;
	PredictParams.SimFrequency = 20;
	PredictParams.OverrideGravityZ = 0;
	//PredictParams.DrawDebugType = EDrawDebugTrace::Persistent;
	PredictParams.DrawDebugTime = 3;
	PredictParams.bTraceComplex = false;
	PredictParams.ObjectTypes = 
	{
		UEngineTypes::ConvertToObjectType(ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECC_WorldDynamic) 
	};
	FPredictProjectilePathResult Result;
	UArsenalStatics::PredictCharacterPath(GetNarrativeCharacterOwner(),PredictParams, Result);

	return UArsenalStatics::IsPointInWaterVolume(GetWorld(), Result.HitResult.ImpactPoint);
	
}

bool UNarrativeCharacterMovement::TryEnterCover()
{
	const FVector CoverNormal = CoverState.CoverTransform.Rotator().Vector();
	const FVector ControlNormal = GetPawnOwner()->GetControlRotation().Vector();
	const FVector StartLoc = CapB(CapHH() + 20.f);
	const FVector EndLoc = StartLoc + (ControlNormal.GetSafeNormal2D() * FindCoverForwardSearchDist);

	FCollisionQueryParams QueryParams = GetIgnoreCharacterParams();

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		StartLoc,
		EndLoc,
		FQuat::Identity,
		TraceChannel_NarrativeCover,
		FCollisionShape::MakeCapsule(CapR(), CapHH() * 0.75f),
		QueryParams
	);

#if ENABLE_DRAW_DEBUG
	if (CVarCoverDebug.GetValueOnAnyThread())
	{
		FString RoleStr = IsServer() ? "Server" : "Client";
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("%s: Cover initial trace %s"), *RoleStr, *GetNameSafe(Hit.GetActor())));
		DrawDebugCapsuleTraceSingle(GetWorld(), StartLoc, EndLoc, CapR(), CapHH(), EDrawDebugTrace::Type::ForDuration, bHit, Hit, FLinearColor::Green, FLinearColor::Red, 5.f);

	}
#endif 

	if (bHit)
	{
		if (IsCoverValid(Hit))
		{
			if (SetCoverFromHit(Hit))
			{
				//First time entering cover, set our direction to look in, either left or right. 
				const FVector CoverRight = CoverState.CoverTransform.GetRotation().GetRightVector();
				const FVector CoverFwd = CoverState.CoverTransform.GetRotation().GetForwardVector();

				if (FVector::DotProduct(CoverRight, GetForwardVector()) < 0.f)
				{
					CoverState.CoverPlayerRotation = (-CoverRight).Rotation();
				}
				else
				{
					CoverState.CoverPlayerRotation = (CoverRight).Rotation();
				}

				OnEnterCover.Broadcast();

				return true;
			}
		}
	}

	return false; 
}

bool UNarrativeCharacterMovement::SetCoverFromHit(const FHitResult& Hit)
{
	if (!IsCoverValid(Hit))
	{
		return false; 
	}

	if (UpdatedComponent && GetNarrativeCharacterOwner())
	{
		CoverState.CoverTransform = FTransform{ Hit.ImpactNormal.ToOrientationRotator(), Hit.ImpactPoint, FVector::OneVector };
		CoverState.CoverHit = Hit;

		return true;
	}

	return false; 
}

bool UNarrativeCharacterMovement::IsCoverValid(const FHitResult& Hit)
{
	if (!Hit.bBlockingHit)
	{
#if ENABLE_DRAW_DEBUG
		if (CVarCoverDebug.GetValueOnAnyThread())
		{
			FString RoleStr = IsServer() ? "Server" : "Client";
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("%s: Cover invalid no blocking hit"), *RoleStr));
		}
	#endif 
		return false;
	}

	//Ensure surface is relatively vertical
	if (FMath::Abs(Hit.ImpactNormal.ToOrientationRotator().Pitch) > 10.f)
	{
#if ENABLE_DRAW_DEBUG
		if (CVarCoverDebug.GetValueOnAnyThread())
		{
			FString RoleStr = IsServer() ? "Server" : "Client";
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("%s: Cover invalid impact pitch was %f, limit 10. "), *RoleStr, FMath::Abs(Hit.ImpactNormal.ToOrientationRotator().Pitch)));
		}
		#endif 
		return false;
	}

	return true; 
}

void UNarrativeCharacterMovement::InvalidateCover()
{
	if (HasCover())
	{
		CoverState.CoverTransform = UNarrativeCharacterMovement::InvalidCoverTransform; 
		OnExitCover.Broadcast();
	}
}

void UNarrativeCharacterMovement::CoverToPlayer(FVector& Location, FRotator& Rotation, bool bIncludeLeaning/*=true*/) const
{
	Rotation = CoverState.CoverTransform.Rotator();

	FVector LeanOutOfCoverOffset = FVector::ZeroVector;

	if (bIncludeLeaning)
	{
		bool bIsAiming = IsAiming();
		//If we're aiming, push our player back from the cover a bit. 
		if (bIsAiming)
		{
			LeanOutOfCoverOffset += CoverState.CoverTransform.GetRotation().GetForwardVector() * LeanFromCoverDist;

			//Peek left or right, but only if we're not in short cover. 
			if (!CoverState.bIsShortCover)
			{
				if (CoverState.bIsLeftOpen && !CoverState.bIsRightOpen)
				{
					LeanOutOfCoverOffset += CoverState.CoverTransform.GetRotation().GetRightVector() * LeanFromCoverDist;
				}
				else if (!CoverState.bIsLeftOpen && CoverState.bIsRightOpen)
				{
					LeanOutOfCoverOffset += CoverState.CoverTransform.GetRotation().GetRightVector() * -LeanFromCoverDist;
				}					//If both are open lean in the direction we're facing
				else if (CoverState.bIsLeftOpen && CoverState.bIsRightOpen)
				{
					if (IsCoverStrafingLeft())
					{
						LeanOutOfCoverOffset += CoverState.CoverTransform.GetRotation().GetRightVector() * -LeanFromCoverDist;
					}
					else
					{
						LeanOutOfCoverOffset += CoverState.CoverTransform.GetRotation().GetRightVector() * LeanFromCoverDist;
					}
				}
			}
		}
	}

	Location = (CoverState.CoverTransform.GetLocation() + Rotation.Vector() * PlayerOffsetFromCover) + LeanOutOfCoverOffset;
}

bool UNarrativeCharacterMovement::HasCover() const
{
	return CoverState.CoverTransform.GetLocation() != InvalidCoverTransform.GetLocation() && IsWalking();
}

bool UNarrativeCharacterMovement::ShouldInvalidateCover() const
{

	if (FVector::Dist(CapLoc(), CoverLoc()) > 1000.f)
	{
		return true;
	}

	return false;
}

bool UNarrativeCharacterMovement::CoverMove(const FVector2D& LocalInput)
{
	//Dont allow for movement whilst we're aiming from cover. 
	if (IsAiming())
	{
		return false; 
	}

	const bool bGoingLeft = LocalInput.Y < 0.0f;

	//Dont move if we're blocked, but do rotate to the other direction 
	//if (bGoingLeft && CoverState.bIsLeftOpen)
	//{
	//	CoverState.CoverPlayerRotation = (CoverState.CoverTransform.GetRotation().GetRightVector()).Rotation();
	//	return false;
	//}

	//if (!bGoingLeft && CoverState.bIsRightOpen)
	//{
	//	CoverState.CoverPlayerRotation = (-CoverState.CoverTransform.GetRotation().GetRightVector()).Rotation();
	//	return false; 
	//}


	if (CVarCoverDebug.GetValueOnAnyThread())
	{
		FString RoleStr = IsServer() ? "Server" : "Client";
		GEngine->AddOnScreenDebugMessage(-1, 0.001f, FColor::Red, FString::Printf(TEXT("%s: localinput %f"), *RoleStr, LocalInput.Y));
	}

	const FVector CheckDir = CoverState.CoverTransform.GetRotation().GetRightVector() * (LocalInput.Y < 0.0f ? 1.f : -1.f);
	const FVector CheckDirMag = CheckDir * NextCoverTraceSpacing;
	const FVector ControlNormal = GetPawnOwner()->GetControlRotation().Vector();

	const FVector StartLoc = (GetActorLocation() + FVector(0.f, 0.f, 50.f)) + CheckDirMag;

	//Optionally angle the capsule inwards a bit so the trace goes around corners. 
	const FVector Angle = bCanWalkAroundCornersInCover ? CoverFwd().RotateAngleAxis(bGoingLeft ? 15.f : -15.f, FVector::UpVector) : CoverFwd();

	const FVector EndLoc = StartLoc + (Angle * -NextCoverTraceDepth) ;

	FCollisionQueryParams QueryParams = GetIgnoreCharacterParams();

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		StartLoc,
		EndLoc,
		FQuat::Identity,
		TraceChannel_NarrativeCover,
		FCollisionShape::MakeCapsule(CapR(), CapHH()),
		QueryParams
	);

#if ENABLE_DRAW_DEBUG
	if (CVarCoverDebug.GetValueOnAnyThread())
	{
		FString RoleStr = IsServer() ? "Server" : "Client";
		GEngine->AddOnScreenDebugMessage(-1, 0.001f, FColor::Red, FString::Printf(TEXT("%s: Cover trace hit %s"), *RoleStr, *GetNameSafe(Hit.GetActor())));
		DrawDebugCapsuleTraceSingle(GetWorld(), StartLoc, EndLoc, CapR(), CapHH(), EDrawDebugTrace::Type::ForOneFrame, bHit, Hit, FLinearColor::Green, FLinearColor::Red, 5.f);
	}
#endif
	if (bHit)
	{
		return SetCoverFromHit(Hit);
	}
	else
	{
		//We hit nothing, just set existing to stop spam
		return SetCoverFromHit(CoverState.CoverHit);
	}

	return false;
}

FVector UNarrativeCharacterMovement::CoverRight() const
{
	return CoverState.CoverTransform.GetRotation().GetRightVector();
}

FVector UNarrativeCharacterMovement::CoverFwd() const
{
	return CoverState.CoverTransform.GetRotation().GetForwardVector();
}

FVector UNarrativeCharacterMovement::CoverLoc() const
{
	return CoverState.CoverTransform.GetLocation();
}

FRotator UNarrativeCharacterMovement::CoverRot() const
{
	return CoverState.CoverTransform.GetRotation().Rotator();
}

bool UNarrativeCharacterMovement::IsCoverStrafingLeft() const
{
	const FVector CoverRight = CoverState.CoverTransform.GetRotation().GetRightVector();

	return (FVector::DotProduct(CoverRight, GetForwardVector()) < 0.f);
}

bool UNarrativeCharacterMovement::IsAiming() const
{
	if (GetNarrativeCharacterOwner())
	{
		return GetNarrativeCharacterOwner()->HasMatchingGameplayTag(FNarrativeGameplayTags::Get().State_Weapon_IsAiming);
	}

	return false; 
}

FGameplayTag IClimbableActor::GetClimbOverrideTag_Implementation() const
{
	return FNarrativeGameplayTags::Get().Narrative_Anim_OverrideLayer_Climbing;
}

bool IClimbableActor::AdjustInitialWarpProps_Implementation(FAttachWarpProps& AttachWarpProps, const FHitResult& InitialClimableHit, class ANarrativeCharacter* Character, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const
{
	return true; 
}

bool IClimbableActor::TryFindClimbTransform_Implementation(FAttachWarpProps& AttachWarpProps, const FAttachWarpProps& OldProps, ANarrativeCharacter* Character, bool PressedJump, FVector2D InputVector, float OptionalBlendInTime) const
{
	return false;
}

